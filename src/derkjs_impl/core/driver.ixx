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

export module core.driver;

export import frontend.ast;
import frontend.parse;
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
        static constexpr std::size_t default_stack_size = 8192;
        static constexpr std::size_t default_call_depth_limit = 384;
        static constexpr std::array<std::string_view, static_cast<std::size_t>(VMErrcode::last)> error_code_msgs = {
            "",
            "ERROR: cannot access undefined property.",
            "ERROR: bad Function call or assignment operation.",
            "ERROR: heap allocation failed.",
            "ERROR: VM aborted via halt.",
            "ERROR: Uncaught error:\n",
            "OK",
        };

    private:
        Backend::BytecodeEmitterContext m_compile_state;
        Parser m_parser;
        Lexer m_lexer;
        
        std::flat_map<std::string_view, TokenTag> m_js_lexicals;
        std::vector<Backend::PreloadItem> m_preloads;
        std::vector<std::string> m_src_map;
        std::string_view m_app_name;
        std::string_view m_app_author;
        ObjectBase<Value>* m_length_str_length_key_p;
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

            src_buffer << '\n';

            return src_buffer.str();
        }

        [[nodiscard]] auto parse_script(const std::string& file_path, const std::string& polyfill_file_path) -> std::optional<ASTUnit> {
            std::string source_with_prelude = read_script(polyfill_file_path);
            source_with_prelude.append_range(read_script(file_path));

            m_src_map.emplace_back(std::move(source_with_prelude));

            m_lexer = Lexer {m_src_map.at(0), std::move(m_js_lexicals)};

            return m_parser(m_lexer, file_path, m_src_map.at(0));
        }

        [[nodiscard]] auto compile_script(const ASTUnit& ast) -> std::optional<Program> {
            return m_compile_state.compile_script(std::move(m_preloads), m_max_heap_object_n, ast, m_src_map);
        }

    public:
        Driver(DriverInfo info, int max_heap_object_count)
        : m_compile_state {}, m_parser {}, m_lexer {}, m_js_lexicals {}, m_src_map {}, m_app_name {info.name}, m_app_author {info.author}, m_length_str_length_key_p {}, m_version_major {info.version_major}, m_version_minor {info.version_minor}, m_version_patch {info.version_patch}, m_max_heap_object_n {max_heap_object_count}, m_allow_bytecode_dump {false} {
            // 1.1: hack in "length" here as a special key that could be used for strings (but also arrays).
            auto length_str_length_key_p = std::make_unique<DynamicString>(nullptr, Value {nullptr}, std::string {"length"}); // the property name value string itself- only to check against!
            m_length_str_length_key_p = length_str_length_key_p.get();
            length_str_length_key_p->patch_length_property(Value {m_length_str_length_key_p}, 6); // backpatch "length" string literal with length property key-value

            m_preloads.emplace_back(Backend::PreloadItem {
                .lexeme = "length",
                .entity = std::move(length_str_length_key_p),
                .location = Location::key_str
            });

            m_preloads.emplace_back(Backend::PreloadItem {
                .lexeme = "message",
                /// NOTE: Create this important "message" key to save later within the VM, used for Error creation.
                .entity = std::make_unique<DynamicString>(
                    nullptr,
                    Value {m_length_str_length_key_p},
                    std::string {"message"}
                ),
                .location = Location::key_str
            });

            m_preloads.emplace_back(Backend::PreloadItem {
                .lexeme = "name",
                /// NOTE: Create this important "name" key to save later within the VM, used for Error creation.
                .entity = std::make_unique<DynamicString>(
                    nullptr,
                    Value {m_length_str_length_key_p},
                    std::string {"name"}
                ),
                .location = Location::key_str
            });
        }

        void add_js_lexical(std::string_view lexeme, TokenTag tag) {
            m_js_lexicals[lexeme] = tag;
        }

        void add_expr_emitter(ExprNodeTag expr_type_tag, std::unique_ptr<Backend::ExprEmitterBase<Expr>> helper) {
            m_compile_state.add_expr_emitter(expr_type_tag, std::move(helper));
        }

        void add_stmt_emitter(StmtNodeTag stmt_type_tag, std::unique_ptr<Backend::StmtEmitterBase<Stmt>> helper) {
            m_compile_state.add_stmt_emitter(stmt_type_tag, std::move(helper));
        }

        [[nodiscard]] auto get_length_key_str_p() noexcept -> ObjectBase<Value>* {
            return m_length_str_length_key_p;
        }

        template <typename ObjectSubType, typename ... CtorArgs>
        requires (
            std::is_base_of_v<ObjectBase<Value>, ObjectSubType>
            && std::is_constructible_v<ObjectSubType, CtorArgs...>
        )
        [[maybe_unused]] auto add_native_object(std::string optional_name, CtorArgs&& ... ctor_args) -> ObjectBase<Value>* {
            auto object_p = std::make_unique<ObjectSubType>(std::forward<CtorArgs>(ctor_args)...);
            auto object_raw_p = object_p.get();

            m_preloads.emplace_back(Backend::PreloadItem {
                .lexeme = optional_name,
                .entity = std::move(object_p),
                .location = Location::heap_obj
            });

            return object_raw_p;
        }

        template <std::size_t N>
        [[maybe_unused]] auto patch_native_object(ObjectBase<Value>* target_object_p, ObjectBase<Value>* string_prototype_p, std::array<NativePropertyStub, N> property_items) -> bool {
            for (auto& [stub_name, item] : property_items) {
                auto prop_name_p = std::make_unique<DynamicString>(string_prototype_p, Value {m_length_str_length_key_p}, stub_name);
                auto prop_name_value = Value {prop_name_p.get()};

                m_preloads.emplace_back(Backend::PreloadItem {
                    .lexeme = stub_name,
                    .entity = prop_name_value,
                    .location = Location::constant
                });

                m_preloads.emplace_back(Backend::PreloadItem {
                    .lexeme = "",
                    .entity = std::move(prop_name_p),
                    .location = Location::heap_obj
                });

                if (auto item_as_primitive_p = std::get_if<Value>(&item); item_as_primitive_p) {
                    if (!target_object_p->set_property_value(prop_name_value, *item_as_primitive_p)) {
                        return false;
                    }
                } else {
                    auto item_as_object_p = std::get_if<std::unique_ptr<ObjectBase<Value>>>(&item);

                    if (!target_object_p->set_property_value(prop_name_value, Value {item_as_object_p->get()})) {
                        return false;
                    } else {
                        /// NOTE: Here, prepare an anonymous JS Object value to be inserted into the heap, likely referenced by this object.
                        m_preloads.emplace_back(Backend::PreloadItem {
                            .lexeme = "",
                            .entity = std::move(*item_as_object_p),
                            .location = Location::heap_obj
                        });
                    }
                }
            }

            return true;
        } 

        void add_native_object_alias(std::string name, ObjectBase<Value>* object_p) {
            m_preloads.emplace_back(Backend::PreloadItem {
                .lexeme = name,
                .entity = Value {object_p},
                .location = Location::constant
            });
        }

        void enable_bc_dump(bool flag) noexcept {
            m_allow_bytecode_dump = flag;
        }

        [[nodiscard]] auto get_info() const noexcept -> DriverInfo {
            return DriverInfo {m_app_name, m_app_author, m_version_major, m_version_minor, m_version_patch};
        }

        [[nodiscard]] auto run(const std::string& file_path, const std::string& polyfill_file_path, std::size_t gc_threshold) -> int {
            auto script_ast = parse_script(file_path, polyfill_file_path);

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

            auto& prgm_ref = prgm.value();
            DerkJS::VM vm {
                prgm_ref,
                default_stack_size, default_call_depth_limit, gc_threshold,
                &m_lexer, &m_parser, &m_compile_state, Backend::compile_snippet_helper
            };

            vm.run();

            switch (const auto vm_status = vm.peek_status(); vm_status) {
            case VMErrcode::pending:
            case VMErrcode::bad_property_access:
            case VMErrcode::bad_operation:
            case VMErrcode::bad_heap_alloc:
            case VMErrcode::vm_abort:
                std::println(std::cerr, "{}", error_code_msgs.at(static_cast<int>(vm_status)));
                return 1;
            case VMErrcode::uncaught_error:
                std::println(
                    std::cerr, "{}\n{}",
                    error_code_msgs.at(static_cast<int>(vm_status)),
                    vm.peek_leftover_error().to_string().value()
                );
                return 1;
            case VMErrcode::ok:
            default:
                return 0;
            }
        }
    };
}
