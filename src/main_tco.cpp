#include <string>
#include <string_view>
#include <array>
#include <print>
#include <iostream>

import derkjs_impl;

constexpr std::string_view fancy_name = " _              __\n"
                                        "| \\ _   _|    |(_\n"
                                        "|_/(/_ | |< \\_|__)\n";
constexpr std::size_t derkjs_gc_threshold = 144000; // ~ 2K heap objects
constexpr int derkjs_heap_count = 4096;

int main(int argc, char* argv[]) {
    using namespace DerkJS;

    if (argc < 2 || argc > 3) {
        std::println(std::cerr, "usage: ./derkjs [-v | [-d | -r] <script name>]");
        return 1;
    }

    Core::Driver driver {
        Core::DriverInfo {
            .name = fancy_name,
            .author = "DrkWithT (GitHub)",
            .version_major = 0,
            .version_minor = 4,
            .version_patch = 1
        },
        derkjs_heap_count // increase object count limit for VM if needed
    };

    std::string source_path;
    std::string polyfill_path {"./test_suite/stdlib/polyfill.js"};
    std::string_view arg_1 = argv[1];

    if (arg_1 == "-h") {
        std::println(std::cerr, "usage: ./derkjs [-h | -v | [-d | -r] <script name>]\n\t-h: show help\n\t-v: show version & author");
        return 0;
    } else if (arg_1 == "-v") {
        const auto& [app_name, author_name, major, minor, patch] = driver.get_info();
        std::println("\x1b[1;93m{}\x1b[0m\nv{}.{}.{}\tBy: {}", app_name, major, minor, patch, author_name);
        return 0;
    } else if (arg_1 == "-d") {
        source_path = argv[2];
        driver.enable_bc_dump(true);
    } else if (arg_1 == "-r") {
        source_path = argv[2];
    } else {
        std::println(std::cerr, "usage: ./derkjs [-h | -v | [-d | -r] <script name>]\n\t-h: show help\n\t-v: show version & author");
        return 1;
    }

    /// String.prototype natives
    Core::NativePropertyStub str_props[] {
        Core::NativePropertyStub {
            .name_str = "constructor",
            .item = std::make_unique<NativeFunction>(
                DerkJS::native_str_ctor,
                nullptr,
                driver.get_length_key_str_p(),
                Value {1}
            ),
        },
        Core::NativePropertyStub {
            .name_str = "charCodeAt",
            .item = std::make_unique<NativeFunction>(
                DerkJS::native_str_charcode_at,
                nullptr,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "substr",
            .item = std::make_unique<NativeFunction>(
                DerkJS::native_str_substr,
                nullptr,
                driver.get_length_key_str_p(),
                Value {2}
            )
        },
        Core::NativePropertyStub {
            .name_str = "substring",
            .item = std::make_unique<NativeFunction>(
                DerkJS::native_str_substring,
                nullptr,
                driver.get_length_key_str_p(),
                Value {2}
            )
        },
        Core::NativePropertyStub {
            .name_str = "trim",
            .item = std::make_unique<NativeFunction>(
                DerkJS::native_str_trim,
                nullptr,
                driver.get_length_key_str_p(),
                Value {1}
            )
        }
    };

    /// 2. Register keywords, operators, etc. for parser's lexer. This makes the lexer's configuration flexible. ///
    driver.add_js_lexical("var", TokenTag::keyword_var);
    driver.add_js_lexical("if", TokenTag::keyword_if);
    driver.add_js_lexical("else", TokenTag::keyword_else);
    driver.add_js_lexical("return", TokenTag::keyword_return);
    driver.add_js_lexical("while", TokenTag::keyword_while);
    driver.add_js_lexical("break", TokenTag::keyword_break);
    driver.add_js_lexical("continue", TokenTag::keyword_continue);
    driver.add_js_lexical("function", TokenTag::keyword_function);
    driver.add_js_lexical("prototype", TokenTag::keyword_prototype);
    driver.add_js_lexical("this", TokenTag::keyword_this);
    driver.add_js_lexical("new", TokenTag::keyword_new);
    driver.add_js_lexical("void", TokenTag::keyword_void);
    driver.add_js_lexical("typeof", TokenTag::keyword_typeof);
    driver.add_js_lexical("undefined", TokenTag::keyword_undefined);
    driver.add_js_lexical("null", TokenTag::keyword_null);
    driver.add_js_lexical("true", TokenTag::keyword_true);
    driver.add_js_lexical("false", TokenTag::keyword_false);
    driver.add_js_lexical("++", TokenTag::symbol_two_pluses);
    driver.add_js_lexical("--", TokenTag::symbol_two_minuses);
    driver.add_js_lexical("%", TokenTag::symbol_percent);
    driver.add_js_lexical("*", TokenTag::symbol_times);
    driver.add_js_lexical("/", TokenTag::symbol_slash);
    driver.add_js_lexical("+", TokenTag::symbol_plus);
    driver.add_js_lexical("-", TokenTag::symbol_minus);
    driver.add_js_lexical("!", TokenTag::symbol_bang);
    driver.add_js_lexical("==", TokenTag::symbol_equal);
    driver.add_js_lexical("!=", TokenTag::symbol_bang_equal);
    driver.add_js_lexical("===", TokenTag::symbol_strict_equal);
    driver.add_js_lexical("!==", TokenTag::symbol_strict_bang_equal);
    driver.add_js_lexical("<", TokenTag::symbol_less);
    driver.add_js_lexical("<=", TokenTag::symbol_less_equal);
    driver.add_js_lexical(">", TokenTag::symbol_greater);
    driver.add_js_lexical(">=", TokenTag::symbol_greater_equal);
    driver.add_js_lexical("&&", TokenTag::symbol_amps);
    driver.add_js_lexical("||", TokenTag::symbol_pipes);
    driver.add_js_lexical("=", TokenTag::symbol_assign);
    driver.add_js_lexical("%=", TokenTag::symbol_percent_assign);
    driver.add_js_lexical("*=", TokenTag::symbol_times_assign);
    driver.add_js_lexical("/=", TokenTag::symbol_slash_assign);
    driver.add_js_lexical("+=", TokenTag::symbol_plus_assign);
    driver.add_js_lexical("-=", TokenTag::symbol_minus_assign);

    /// 3. Register bytecode compiler modules since the emission logic per AST type is mostly decoupled from the compiler state. ///
    driver.add_expr_emitter(ExprNodeTag::primitive, std::make_unique<Backend::PrimitiveEmitter>());
    driver.add_expr_emitter(ExprNodeTag::object_literal, std::make_unique<Backend::ObjectLiteralEmitter>());
    driver.add_expr_emitter(ExprNodeTag::array_literal, std::make_unique<Backend::ArrayLiteralEmitter>());
    driver.add_expr_emitter(ExprNodeTag::lambda_literal, std::make_unique<Backend::LambdaLiteralEmitter>());
    driver.add_expr_emitter(ExprNodeTag::member_access, std::make_unique<Backend::MemberAccessEmitter>());
    driver.add_expr_emitter(ExprNodeTag::unary, std::make_unique<Backend::UnaryEmitter>());
    driver.add_expr_emitter(ExprNodeTag::binary, std::make_unique<Backend::BinaryEmitter>());
    driver.add_expr_emitter(ExprNodeTag::assign, std::make_unique<Backend::AssignEmitter>());
    driver.add_expr_emitter(ExprNodeTag::call, std::make_unique<Backend::CallEmitter>());

    driver.add_stmt_emitter(StmtNodeTag::stmt_expr_stmt, std::make_unique<Backend::ExprStmtEmitter>());
    driver.add_stmt_emitter(StmtNodeTag::stmt_variables, std::make_unique<Backend::VariablesEmitter>());
    driver.add_stmt_emitter(StmtNodeTag::stmt_if, std::make_unique<Backend::IfEmitter>());
    driver.add_stmt_emitter(StmtNodeTag::stmt_return, std::make_unique<Backend::ReturnEmitter>());
    driver.add_stmt_emitter(StmtNodeTag::stmt_while, std::make_unique<Backend::WhileEmitter>());
    driver.add_stmt_emitter(StmtNodeTag::stmt_break, std::make_unique<Backend::BreakEmitter>());
    driver.add_stmt_emitter(StmtNodeTag::stmt_continue, std::make_unique<Backend::ContinueEmitter>());
    driver.add_stmt_emitter(StmtNodeTag::stmt_block, std::make_unique<Backend::BlockEmitter>());

    /// 4. Prepare native objects alongside prototypes of Object, Array, etc. These are VERY important for DerkJS to interpret certain scripts properly. ///
    auto string_prototype_p = driver.setup_string_prototype(std::to_array(std::move(str_props)));

    if (!string_prototype_p) {
        std::println(std::cerr, "SETUP ERROR: failed to allocate String.prototype object.");
        return 1;
    }

    /// Function.prototype natives
    Core::NativePropertyStub function_helper_props[] {
        Core::NativePropertyStub {
            .name_str = "call",
            .item = std::make_unique<NativeFunction>(
                DerkJS::native_function_call,
                nullptr,
                driver.get_length_key_str_p(),
                Value {1}
            )
        }
    };

    /// Array.prototype natives
    Core::NativePropertyStub array_obj_props[] {
        Core::NativePropertyStub {
            .name_str = "constructor",
            .item = std::make_unique<NativeFunction>(
                DerkJS::native_array_ctor,
                nullptr,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "push",
            .item = std::make_unique<NativeFunction>(
                DerkJS::native_array_push,
                nullptr,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "join",
            .item = std::make_unique<NativeFunction>(
                DerkJS::native_array_join,
                nullptr,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
    };

    /// Object.prototype natives
    Core::NativePropertyStub object_helper_props[] {
        Core::NativePropertyStub {
            .name_str = "constructor",
            .item = std::make_unique<NativeFunction>(
                DerkJS::native_object_ctor,
                nullptr,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "create",
            .item = std::make_unique<NativeFunction>(
                DerkJS::native_object_create,
                nullptr,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "freeze",
            .item = std::make_unique<NativeFunction>(
                DerkJS::native_object_freeze,
                nullptr,
                driver.get_length_key_str_p(),
                Value {1}
            )
        }
    };

    /// Console methods
    Core::NativePropertyStub console_obj_props[] {
        Core::NativePropertyStub {
            .name_str = "log",
            .item = std::make_unique<NativeFunction>(
                DerkJS::native_console_log,
                nullptr,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "readln",
            .item = std::make_unique<NativeFunction>(
                DerkJS::native_console_read_line,
                nullptr,
                driver.get_length_key_str_p(),
                Value {1}
            )
        }
    };

    Core::NativePropertyStub date_obj_props[] {
        Core::NativePropertyStub {
            .name_str = "now",
            .item = std::make_unique<NativeFunction>(
                DerkJS::clock_time_now,
                nullptr,
                driver.get_length_key_str_p(),
                Value {1}
            )
        }
    };

    driver.add_native_global<NativeFunction>(
        "String",
        DerkJS::native_str_ctor,
        string_prototype_p,
        driver.get_length_key_str_p(),
        Value {1}
    );

    // Add `Array.prototype` object here!
    auto array_prototype_object_p = driver.setup_basic_prototype(
        string_prototype_p,
        "Array::prototype",
        std::to_array(std::move(array_obj_props))
    );

    if (!array_prototype_object_p) {
        std::println(std::cerr, "SETUP ERROR: failed to allocate Array.prototype object.");
        return 1;
    }

    driver.add_native_global<NativeFunction>(
        "Array",
        DerkJS::native_array_ctor,
        array_prototype_object_p,
        driver.get_length_key_str_p(),
        Value {1}
    );

    auto object_interface_prototype_p = driver.setup_basic_prototype(
        string_prototype_p,
        "Object::prototype",
        std::to_array(std::move(object_helper_props))
    );

    if (!object_interface_prototype_p) {
        std::println(std::cerr, "SETUP ERROR: failed to allocate Object.prototype object.");
        return 1;
    }

    driver.add_native_global<NativeFunction>(
        "Object",
        DerkJS::native_object_ctor,
        object_interface_prototype_p,
        driver.get_length_key_str_p(),
        Value {1}
    );

    /// TODO: add Function ctor, const length accessor for functions, etc.
    driver.setup_basic_prototype(
        string_prototype_p,
        "Function::prototype",
        std::to_array(std::move(function_helper_props))
    );

    driver.add_native_object<Object>(
        string_prototype_p,
        "console",
        std::to_array(std::move(console_obj_props)),
        object_interface_prototype_p
    );

    driver.add_native_object<Object>(
        string_prototype_p,
        "Date",
        std::to_array(std::move(date_obj_props)),
        object_interface_prototype_p
    );

    driver.add_native_global<NativeFunction>(
        "parseInt",
        DerkJS::native_parse_int,
        nullptr,
        driver.get_length_key_str_p(),
        Value {2}
    );

    /// 4. Run the script after all configuration. ///
    return driver.run(source_path, polyfill_path, derkjs_gc_threshold);
}
