module;

#include <cstdint>
#include <type_traits>
#include <utility>
#include <memory>
#include <string_view>
#include <string>
#include <optional>
#include <vector>
#include <flat_map>

#include <print>
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>

export module core.driver;

import frontend.lexicals;
import frontend.ast;
import frontend.parse;
import frontend.semantics;
import runtime.objects;
import runtime.value;
import runtime.callables;
import runtime.bytecode;
import runtime.vm;
import backend.bc_generate;

export namespace DerkJS::Core {
    struct DriverInfo {
        std::string_view name;
        std::string_view author;
        int version_major;
        int version_minor;
        int version_patch;
    };

    struct NativePropertyStub {
        StaticString name_str;
        std::variant<std::unique_ptr<ObjectBase<Value>>, Value> item;
    };

    class Driver {
    public:
        static constexpr std::size_t default_stack_size = 2048;
        static constexpr std::size_t default_call_depth_limit = 208;

    private:
        PolyPool<ObjectBase<Value>> m_js_heap;
        std::map<std::string_view, TokenTag> m_js_lexicals;
        std::flat_map<std::string, int> m_pooled_string_index;
        std::flat_map<std::string, int> m_native_obj_index;
        std::vector<std::string> m_src_map;
        std::string_view m_app_name;
        std::string_view m_app_author;
        int m_version_major;
        int m_version_minor;
        int m_version_patch;
        bool m_allow_bytecode_dump;

        [[nodiscard]] auto read_script(const std::string& file_path) -> std::string {
            std::ifstream reader {file_path};

            if (!reader.is_open()) {
                return {};
            }

            std::ostringstream src_buffer;
            std::string src_line;

            while (std::getline(reader, src_line)) {
                src_buffer << src_line << '\n';
            }

            return src_buffer.str();
        }

        [[nodiscard]] auto parse_script(const std::string& file_path, std::string file_source) -> std::optional<ASTUnit> {
            m_src_map.emplace_back(std::move(file_source)); // Source 0 is <main>.js

            Lexer lexer {m_src_map.at(0)};
            Parser parser;

            return parser(lexer, file_path, m_src_map.at(0));
        }

        [[nodiscard]] auto check_sema_of_script(const ASTUnit& ast) -> bool {
            SemanticAnalyzer sema_check_pass;

            return sema_check_pass(ast, m_src_map);
        }

        [[nodiscard]] auto compile_script(const ASTUnit& ast) -> std::optional<Program> {
            BytecodeGenPass codegen_pass {std::move(m_js_heap), std::move(m_native_obj_index), std::move(m_pooled_string_index)};

            return codegen_pass(ast, m_src_map.at(0));
        }

    public:
        Driver(DriverInfo info)
        : m_js_heap {}, m_js_lexicals {}, m_native_obj_index {}, m_src_map {}, m_app_name {info.name}, m_app_author {info.author}, m_version_major {info.version_major}, m_version_minor {info.version_minor}, m_version_patch {info.version_patch}, m_allow_bytecode_dump {false} {}

        void add_js_lexical(std::string_view lexeme, TokenTag tag) {
            m_js_lexicals.emplace(lexeme, tag);
        }

        /// NOTE: takes the native object's name & a StaticString-to-< Value / std::unique_ptr<ObjectBase<Value>> > list to preload.
        template <std::size_t N>
        [[nodiscard]] auto add_native_object(std::string name, std::array<NativePropertyStub, N> prop_list) -> bool {
            const auto native_obj_heap_id = m_js_heap.get_next_id();
            
            // 1. Get non-owning object pointer and build the object's properties. Intern corresponding properties' handles into the heap.
            auto object = std::make_unique<Object>(nullptr); // Blank JS object with no prototype.
            auto object_p = object.get();

            for (auto& [prop_name, prop_item] : prop_list) {
                const auto next_key_heap_id = m_js_heap.get_next_id();
                auto interned_str_ptr = m_js_heap.add_item(next_key_heap_id, prop_name);

                // 2a. Put interned property string
                if (!interned_str_ptr) {
                    return false;
                }
    
                m_pooled_string_index.emplace(prop_name.as_string(), next_key_heap_id);

                // 2b. Put property Value: has a non-owning pointer to some object / primitive
                // 2c. Insert the property by PropertyHandle<Value> -> stored value
                if (auto prop_item_as_obj_sp_p = std::get_if<std::unique_ptr<ObjectBase<Value>>>(&prop_item); prop_item_as_obj_sp_p) {
                    /// NOTE: here, the descriptor is for an immutable property... Mutating a native object would probably be risky to program correctness.
                    PropertyHandle<Value> temp_prop_handle {object_p, Value {interned_str_ptr}, PropertyHandleTag::key, 0};

                    /// NOTE: insert property value-object into heap and get its naked ptr. One copy of this ptr is owned, but the other becomes non-owning as a "reference" next...
                    const auto next_prop_value_heap_id = m_js_heap.get_next_id();
                    auto prop_value_p = m_js_heap.add_item(next_prop_value_heap_id, std::move(**prop_item_as_obj_sp_p));

                    if (!object_p->set_property_value(temp_prop_handle, Value {prop_value_p})) {
                        return false;
                    }
                } else {
                    /// TODO: use object_p, Value {interned_str_ptr} to make descriptor.
                    PropertyHandle<Value> temp_primitive_prop_handle {object_p, Value {interned_str_ptr}, PropertyHandleTag::key, 0};

                    if (!object_p->set_property_value(temp_primitive_prop_handle, std::get<Value>(prop_item))) {
                        return false;
                    }
                }
            }

            if (m_js_heap.add_item(native_obj_heap_id, std::move(*object))) {
                m_native_obj_index.emplace(name, native_obj_heap_id);
                return true;
            }

            return false;
        }

        void enable_bc_dump(bool flag) noexcept {
            m_allow_bytecode_dump = flag;
        }

        [[nodiscard]] auto get_info() const noexcept -> DriverInfo {
            return DriverInfo {m_app_name, m_app_author, m_version_major, m_version_minor, m_version_patch};
        }

        /// TODO: run stages before VM here & add configs for VM limits.
        template <DispatchPolicy Dp>
        [[nodiscard]] auto run(const std::string& file_path) -> bool {
            auto script_ast = parse_script(file_path, read_script(file_path));

            if (!script_ast) {
                return false;
            }

            const auto& ast_ref = script_ast.value();

            if (!check_sema_of_script(ast_ref)) {
                return false;
            }

            auto prgm = compile_script(ast_ref);

            if (!prgm) {
                return false;
            }

            DerkJS::VM<Dp> vm {prgm.value(), default_stack_size, default_call_depth_limit};

            auto derkjs_start_time = std::chrono::steady_clock::now();
            const auto vm_failed = vm();
            auto derkjs_running_time = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - derkjs_start_time);

            std::println("DerkJS [Result]: \x1b[1;32m{}\x1b[0m\n\nFinished in \x1b[1;33m{}\x1b[0m", vm.peek_final_result().to_string().value(), derkjs_running_time);

            return !vm_failed;
        }
    };
}
