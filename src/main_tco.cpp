#include <string>
#include <string_view>
#include <array>
#include <variant>
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
            .version_minor = 5,
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

    /// 4.1 Prepare native prototypes as stubs. ///

    auto object_prototype_p = driver.add_native_object<Object>("Object::prototype", nullptr);
    auto boolean_prototype_p = driver.add_native_object<Object>("Boolean::prototype", object_prototype_p);
    // auto number_prototype_p = nullptr;
    auto string_prototype_p = driver.add_native_object<Object>("String::prototype", object_prototype_p);
    auto array_prototype_p = driver.add_native_object<Object>("Array::prototype", object_prototype_p);
    auto function_prototype_p = driver.add_native_object<Object>("Function::prototype", object_prototype_p);

    /// 4.2 Patch properties of native prototypes. These are VERY important for DerkJS to interpret certain scripts properly. ///

    /// Object.prototype items
    Core::NativePropertyStub object_prototype_props[] {
        Core::NativePropertyStub {
            .name_str = "constructor",
            .item = std::make_unique<NativeFunction>(
                object_prototype_p,
                DerkJS::native_object_ctor,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "create",
            .item = std::make_unique<NativeFunction>(
                function_prototype_p,
                DerkJS::native_object_create,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "freeze",
            .item = std::make_unique<NativeFunction>(
                function_prototype_p,
                DerkJS::native_object_freeze,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        }
    };

    Core::NativePropertyStub boolean_prototype_props[] {
        Core::NativePropertyStub {
            .name_str = "constructor",
            .item = std::make_unique<NativeFunction>(
                boolean_prototype_p,
                DerkJS::native_boolean_ctor,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "valueOf",
            .item = std::make_unique<NativeFunction>(
                function_prototype_p,
                DerkJS::native_boolean_value_of,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "toString",
            .item = std::make_unique<NativeFunction>(
                function_prototype_p,
                DerkJS::native_boolean_to_string,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        }
    };

    /// String.prototype items
    Core::NativePropertyStub string_prototype_props[] {
        Core::NativePropertyStub {
            .name_str = "constructor",
            .item = std::make_unique<NativeFunction>(
                string_prototype_p,
                DerkJS::native_str_ctor,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "charCodeAt",
            .item = std::make_unique<NativeFunction>(
                function_prototype_p,
                DerkJS::native_str_charcode_at,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "substr",
            .item = std::make_unique<NativeFunction>(
                function_prototype_p,
                DerkJS::native_str_substr,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {2}
            )
        },
        Core::NativePropertyStub {
            .name_str = "substring",
            .item = std::make_unique<NativeFunction>(
                function_prototype_p,
                DerkJS::native_str_substring,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {2}
            )
        },
        Core::NativePropertyStub {
            .name_str = "trim",
            .item = std::make_unique<NativeFunction>(
                function_prototype_p,
                DerkJS::native_str_trim,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        }
    };

    /// Function.prototype items
    Core::NativePropertyStub function_prototype_props[] {
        Core::NativePropertyStub {
            .name_str = "call",
            .item = std::make_unique<NativeFunction>(
                function_prototype_p,
                DerkJS::native_function_call,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        }
    };

    /// Array.prototype items
    Core::NativePropertyStub array_prototype_props[] {
        Core::NativePropertyStub {
            .name_str = "constructor",
            .item = std::make_unique<NativeFunction>(
                array_prototype_p,
                DerkJS::native_array_ctor,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "push",
            .item = std::make_unique<NativeFunction>(
                function_prototype_p,
                DerkJS::native_array_push,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "join",
            .item = std::make_unique<NativeFunction>(
                function_prototype_p,
                DerkJS::native_array_join,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
    };

    /// Console items
    Core::NativePropertyStub console_props[] {
        Core::NativePropertyStub {
            .name_str = "log",
            .item = std::make_unique<NativeFunction>(
                function_prototype_p,
                DerkJS::native_console_log,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        },
        Core::NativePropertyStub {
            .name_str = "readln",
            .item = std::make_unique<NativeFunction>(
                function_prototype_p,
                DerkJS::native_console_read_line,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        }
    };

    /// Date items
    Core::NativePropertyStub date_props[] {
        Core::NativePropertyStub {
            .name_str = "now",
            .item = std::make_unique<NativeFunction>(
                function_prototype_p,
                DerkJS::clock_time_now,
                function_prototype_p,
                driver.get_length_key_str_p(),
                Value {1}
            )
        }
    };

    ObjectBase<Value>* object_ctor_p = std::get<std::unique_ptr<ObjectBase<Value>>>(object_prototype_props[0].item).get();
    ObjectBase<Value>* boolean_ctor_p = std::get<std::unique_ptr<ObjectBase<Value>>>(boolean_prototype_props[0].item).get();
    ObjectBase<Value>* string_ctor_p = std::get<std::unique_ptr<ObjectBase<Value>>>(string_prototype_props[0].item).get();
    ObjectBase<Value>* array_ctor_p = std::get<std::unique_ptr<ObjectBase<Value>>>(array_prototype_props[0].item).get();

    auto console_p = driver.add_native_object<Object>("", object_prototype_p);
    auto date_p = driver.add_native_object<Object>("", object_prototype_p);
    auto parse_int_fn_p = driver.add_native_object<NativeFunction>(
        "",
        function_prototype_p,
        DerkJS::native_parse_int,
        function_prototype_p,
        driver.get_length_key_str_p(),
        Value {2}
    );

    /// Patch prototypes & alias built-in globals ///

    driver.patch_native_object(object_prototype_p, string_prototype_p, std::to_array(std::move(object_prototype_props)));
    driver.add_native_object_alias("Object", object_ctor_p);

    driver.patch_native_object(boolean_prototype_p, string_prototype_p, std::to_array(std::move(boolean_prototype_props)));
    driver.add_native_object_alias("Boolean", boolean_ctor_p);

    driver.patch_native_object(string_prototype_p, string_prototype_p, std::to_array(std::move(string_prototype_props)));
    driver.add_native_object_alias("String", string_ctor_p);

    driver.patch_native_object(array_prototype_p, string_prototype_p, std::to_array(std::move(array_prototype_props)));
    driver.add_native_object_alias("Array", array_ctor_p);

    driver.patch_native_object(function_prototype_p, string_prototype_p, std::to_array(std::move(function_prototype_props)));

    driver.patch_native_object(console_p, string_prototype_p, std::to_array(std::move(console_props)));
    driver.add_native_object_alias("console", console_p);

    driver.patch_native_object(date_p, string_prototype_p, std::to_array(std::move(date_props)));
    driver.add_native_object_alias("Date", date_p);

    driver.add_native_object_alias("parseInt", parse_int_fn_p);

    /// 6. Run the script after all configuration. ///

    return driver.run(source_path, polyfill_path, derkjs_gc_threshold);
}
