module;

#include <cstdint>
#include <type_traits>
#include <utility>
#include <memory>
#include <string_view>
#include <string>
#include <optional>
#include <variant>
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
import runtime.strings;
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
        std::flat_map<std::string_view, TokenTag> m_js_lexicals;
        std::vector<PreloadItem> m_preloads;
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
                std::println(std::cerr, "NOTE: could not read source: '{}'", file_path);
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

            Lexer lexer {m_src_map.at(0), std::move(m_js_lexicals)};
            Parser parser;

            return parser(lexer, file_path, m_src_map.at(0));
        }

        [[nodiscard]] auto check_sema_of_script(const ASTUnit& ast) -> bool {
            SemanticAnalyzer sema_check_pass;

            return sema_check_pass(ast, m_src_map);
        }

        [[nodiscard]] auto compile_script(const ASTUnit& ast) -> std::optional<Program> {
            // BytecodeGenPass codegen_pass {std::move(m_js_heap), ... TODO!!};
            // return codegen_pass(ast, m_src_map);

            return {};
        }

    public:
        Driver(DriverInfo info, int heap_obj_capacity)
        : m_js_heap {heap_obj_capacity}, m_js_lexicals {}, m_native_obj_index {}, m_src_map {}, m_app_name {info.name}, m_app_author {info.author}, m_version_major {info.version_major}, m_version_minor {info.version_minor}, m_version_patch {info.version_patch}, m_allow_bytecode_dump {false} {}

        void add_js_lexical(std::string_view lexeme, TokenTag tag) {
            m_js_lexicals.emplace(lexeme, tag);
        }

        /// NOTE: takes the native object's name & a StaticString <-> "item" list to preload. The item is a primitive Value OR ObjectBase<Value>.
        template <std::size_t N>
        [[maybe_unused]] auto add_native_object(std::string name, std::array<NativePropertyStub, N> prop_list) -> bool {
            // todo: re-implement this based on what codegen needs to be preloaded...
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

            if (m_allow_bytecode_dump) {
                disassemble_program(prgm.value());
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
