module;

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
        std::string name_str;
        std::variant<std::unique_ptr<ObjectBase<Value>>, Value> item;
    };

    class Driver {
    public:
        static constexpr std::size_t default_stack_size = 2048;
        static constexpr std::size_t default_call_depth_limit = 208;
        static constexpr std::array<std::string_view, static_cast<std::size_t>(VMErrcode::last)> error_code_msgs = {
            "OK",
            "ERROR: cannot access undefined property.",
            "ERROR: bad Function call or assignment operation.",
            "ERROR: heap allocation failed.",
            "ERROR: VM aborted via halt.",
        };

    private:
        std::flat_map<std::string_view, TokenTag> m_js_lexicals;
        std::vector<PreloadItem> m_preloads;
        std::vector<std::string> m_src_map;
        std::string_view m_app_name;
        std::string_view m_app_author;
        int m_version_major;
        int m_version_minor;
        int m_version_patch;
        int m_max_heap_object_n;
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

            std::vector<int> a;

            return src_buffer.str();
        }

        [[nodiscard]] auto parse_script(const std::string& file_path, std::string file_source) -> std::optional<ASTUnit> {
            m_src_map.emplace_back(std::move(file_source)); // Source 0 is <main>.js

            Lexer lexer {m_src_map.at(0), std::move(m_js_lexicals)};
            Parser parser;

            return parser(lexer, file_path, m_src_map.at(0));
        }

        [[nodiscard]] auto compile_script(const ASTUnit& ast) -> std::optional<Program> {
            BytecodeGenPass codegen_pass {std::move(m_preloads), m_max_heap_object_n};
            return codegen_pass(ast, m_src_map);
        }

    public:
        Driver(DriverInfo info, int max_heap_object_count)
        : m_js_lexicals {}, m_src_map {}, m_app_name {info.name}, m_app_author {info.author}, m_version_major {info.version_major}, m_version_minor {info.version_minor}, m_version_patch {info.version_patch}, m_max_heap_object_n {max_heap_object_count}, m_allow_bytecode_dump {false} {}

        void add_js_lexical(std::string_view lexeme, TokenTag tag) {
            m_js_lexicals[lexeme] = tag;
        }

        template <std::size_t N>
        [[nodiscard]] auto add_anonymous_native_object(ObjectBase<Value>* string_proto, std::array<NativePropertyStub, N> prop_list) -> ObjectBase<Value>* {
            auto anonymous_object_p = std::make_unique<Object>(nullptr);

            for (auto& [stub_name, item] : prop_list) {
                auto prop_name_p = std::make_unique<DynamicString>(string_proto, stub_name);
                auto prop_name_value = Value {prop_name_p.get()};

                m_preloads.emplace_back(PreloadItem {
                    .lexeme = stub_name,
                    .entity = prop_name_value,
                    .location = Location::constant
                });

                m_preloads.emplace_back(PreloadItem {
                    .lexeme = "",
                    .entity = std::move(prop_name_p),
                    .location = Location::heap_obj
                });

                if (auto item_as_primitive_p = std::get_if<Value>(&item); item_as_primitive_p) {
                    PropertyHandle<Value> prop_value_desc {prop_name_value, PropertyHandleTag::key, 0x00}; // immutable property referencing its underlying value

                    if (!anonymous_object_p->set_property_value(prop_value_desc, *item_as_primitive_p)) {
                        return nullptr;
                    }
                } else {
                    auto item_as_object_p = std::get_if<std::unique_ptr<ObjectBase<Value>>>(&item);
                    PropertyHandle<Value> prop_obj_desc {prop_name_value, PropertyHandleTag::key, 0x00};

                    if (!anonymous_object_p->set_property_value(prop_obj_desc, Value {item_as_object_p->get()})) {
                        return nullptr;
                    } else {
                        /// NOTE: Here, prepare an anonymous JS Object value to be inserted into the heap, likely referenced by this object.
                        m_preloads.emplace_back(PreloadItem {
                            .lexeme = "",
                            .entity = std::move(*item_as_object_p),
                            .location = Location::heap_obj
                        });
                    }
                }
            }

            auto anonymous_object_raw_p = anonymous_object_p.get();

            m_preloads.emplace_back(PreloadItem {
                .lexeme = "",
                .entity = std::move(anonymous_object_p),
                .location = Location::heap_obj
            });

            return anonymous_object_raw_p;
        }

        template <typename ObjectSubType, typename ... CtorArgs> requires (std::is_base_of_v<ObjectBase<Value>, ObjectSubType> && std::is_constructible_v<ObjectSubType, CtorArgs...>)
        [[maybe_unused]] auto add_native_global(std::string name, CtorArgs&& ... ctor_args) -> bool {
            auto object_p = std::make_unique<ObjectSubType>(std::forward<CtorArgs>(ctor_args)...);

            m_preloads.emplace_back(PreloadItem {
                .lexeme = name,
                .entity = Value {object_p.get()},
                .location = Location::constant
            });

            m_preloads.emplace_back(PreloadItem {
                .lexeme = "",
                .entity = std::move(object_p),
                .location = Location::heap_obj
            });

            return true;
        }


        /// NOTE: takes the native object's name & a StaticString <-> "item" list to preload. The item is a primitive Value OR ObjectBase<Value>.
        template <typename ObjectSubType, std::size_t N, typename ... CtorArgs> requires (std::is_base_of_v<ObjectBase<Value>, ObjectSubType> && std::is_constructible_v<ObjectSubType, CtorArgs...>)
        [[maybe_unused]] auto add_native_object(ObjectBase<Value>* string_proto, std::string name, std::array<NativePropertyStub, N> prop_list, CtorArgs&& ... ctor_args) -> bool {
            auto object_p = std::make_unique<ObjectSubType>(std::forward<CtorArgs>(ctor_args)...);

            for (auto& [stub_name, item] : prop_list) {
                auto prop_name_p = std::make_unique<DynamicString>(string_proto, stub_name);
                auto prop_name_value = Value {prop_name_p.get()};

                m_preloads.emplace_back(PreloadItem {
                    .lexeme = stub_name,
                    .entity = prop_name_value,
                    .location = Location::constant
                });

                m_preloads.emplace_back(PreloadItem {
                    .lexeme = "",
                    .entity = std::move(prop_name_p),
                    .location = Location::heap_obj
                });

                if (auto item_as_primitive_p = std::get_if<Value>(&item); item_as_primitive_p) {
                    PropertyHandle<Value> prop_value_desc {prop_name_value, PropertyHandleTag::key, 0x00}; // immutable property referencing its underlying value

                    if (!object_p->set_property_value(prop_value_desc, *item_as_primitive_p)) {
                        return false;
                    }
                } else {
                    auto item_as_object_p = std::get_if<std::unique_ptr<ObjectBase<Value>>>(&item);
                    PropertyHandle<Value> prop_obj_desc {prop_name_value, PropertyHandleTag::key, 0x00};

                    if (!object_p->set_property_value(prop_obj_desc, Value {item_as_object_p->get()})) {
                        return false;
                    } else {
                        /// NOTE: Here, prepare an anonymous JS Object value to be inserted into the heap, likely referenced by this object.
                        m_preloads.emplace_back(PreloadItem {
                            .lexeme = "",
                            .entity = std::move(*item_as_object_p),
                            .location = Location::heap_obj
                        });
                    }
                }
            }

            m_preloads.emplace_back(PreloadItem {
                .lexeme = name,
                .entity = Value {object_p.get()},
                .location = Location::constant
            });

            m_preloads.emplace_back(PreloadItem {
                .lexeme = "",
                .entity = std::move(object_p),
                .location = Location::heap_obj
            });

            return true;
        }

        template <std::size_t N>
        [[nodiscard]] auto setup_string_prototype(std::array<NativePropertyStub, N> prop_list) -> ObjectBase<Value>* {
            // 1. Create dud String.prototype & its wrapping String.
            auto str_prototype_object_p = std::make_unique<Object>(nullptr);
            auto str_object_p = std::make_unique<DynamicString>(str_prototype_object_p.get(), "");
            ObjectBase<Value>* str_object_raw_p = str_object_p.get();

            // 2. Fill String.prototype & record string keys to patch prototype of...
            for (auto& [stub_name, item] : prop_list) {
                auto prop_name_p = std::make_unique<DynamicString>(str_object_raw_p, stub_name);
                auto prop_name_value = Value {prop_name_p.get()};

                m_preloads.emplace_back(PreloadItem {
                    .lexeme = stub_name,
                    .entity = prop_name_value,
                    .location = Location::constant
                });

                m_preloads.emplace_back(PreloadItem {
                    .lexeme = "",
                    .entity = std::move(prop_name_p),
                    .location = Location::heap_obj
                });

                if (auto item_as_primitive_p = std::get_if<Value>(&item); item_as_primitive_p) {
                    PropertyHandle<Value> prop_value_desc {prop_name_value, PropertyHandleTag::key, 0x00}; // immutable property referencing its underlying value

                    if (!str_prototype_object_p->set_property_value(prop_value_desc, *item_as_primitive_p)) {
                        return nullptr;
                    }
                } else {
                    auto item_as_object_p = std::get_if<std::unique_ptr<ObjectBase<Value>>>(&item);
                    PropertyHandle<Value> prop_obj_desc {prop_name_value, PropertyHandleTag::key, 0x00};

                    if (!str_prototype_object_p->set_property_value(prop_obj_desc, Value {item_as_object_p->get()})) {
                        return nullptr;
                    } else {
                        /// NOTE: Here, prepare an anonymous JS Object value to be inserted into the heap, likely referenced by this object.
                        m_preloads.emplace_back(PreloadItem {
                            .lexeme = "",
                            .entity = std::move(*item_as_object_p),
                            .location = Location::heap_obj
                        });
                    }
                }
            }

            m_preloads.emplace_back(PreloadItem {
                .lexeme = "",
                .entity = std::move(str_prototype_object_p),
                .location = Location::heap_obj
            });

            m_preloads.emplace_back(PreloadItem {
                .lexeme = "",
                .entity = std::move(str_object_p),
                .location = Location::heap_obj
            });

            m_preloads.emplace_back(PreloadItem {
                .lexeme = "String",
                .entity = Value {str_object_raw_p},
                .location = Location::constant
            });

            return str_object_raw_p;
        }

        void enable_bc_dump(bool flag) noexcept {
            m_allow_bytecode_dump = flag;
        }

        [[nodiscard]] auto get_info() const noexcept -> DriverInfo {
            return DriverInfo {m_app_name, m_app_author, m_version_major, m_version_minor, m_version_patch};
        }

        template <DispatchPolicy Dp>
        [[nodiscard]] auto run(const std::string& file_path, std::size_t gc_threshold) -> int {
            auto script_ast = parse_script(file_path, read_script(file_path));

            if (!script_ast) {
                return 1;
            }

            const auto& ast_ref = script_ast.value();
            auto prgm = compile_script(ast_ref);

            if (!prgm) {
                return 1;
            }

            if (m_allow_bytecode_dump) {
                disassemble_program(prgm.value());
            }

            DerkJS::VM<Dp> vm {prgm.value(), default_stack_size, default_call_depth_limit, gc_threshold};

            auto derkjs_start_time = std::chrono::steady_clock::now();
            const auto derkjs_had_error = vm();
            auto derkjs_running_time = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - derkjs_start_time);

            switch (const auto vm_status = vm.peek_status(); vm_status) {
            case VMErrcode::bad_property_access:
            case VMErrcode::bad_operation:
            case VMErrcode::bad_heap_alloc:
            case VMErrcode::vm_abort:
                std::println(std::cerr, "{}", error_code_msgs.at(static_cast<int>(vm_status)));
                break;
            case VMErrcode::ok:
            default:
                break;
            }

            std::println("DerkJS [Result]: \x1b[1;32m{}\x1b[0m\n\nFinished in \x1b[1;33m{}\x1b[0m", vm.peek_final_result().to_string().value(), derkjs_running_time);

            return (derkjs_had_error) ? 1 : vm.peek_final_result().to_num_i32().value();
        }
    };
}
