#include <string>
#include <array>
#include <flat_map>
#include <print>
#include <iostream>

import derkjs_impl;

[[nodiscard]] auto native_console_log(DerkJS::ExternVMCtx* ctx, [[maybe_unused]] DerkJS::PropPool<DerkJS::PropertyHandle<DerkJS::Value>, DerkJS::Value>* props, int argc) -> bool {
    const int vm_rsbp = ctx->rsbp;

    for (auto passed_arg_local_offset = 0; passed_arg_local_offset < argc; ++passed_arg_local_offset) {
        std::print("{} ", ctx->stack[vm_rsbp + passed_arg_local_offset].to_string().value());
    }

    std::println();

    return true;
}

[[nodiscard]] auto native_console_read_line(DerkJS::ExternVMCtx* ctx, [[maybe_unused]] DerkJS::PropPool<DerkJS::PropertyHandle<DerkJS::Value>, DerkJS::Value>* props, int argc) -> bool {    
    /// NOTE: Show user the passed-in prompt string: It MUST be the 1st argument on the stack.
    const auto passed_rbsp = ctx->rsbp;
    std::print("{}", ctx->stack[passed_rbsp].to_string().value());

    std::string temp_line;
    std::getline(std::cin, temp_line);

    if (auto line_str_p = ctx->heap.add_item(ctx->heap.get_next_id(), DerkJS::DynamicString {std::move(temp_line)}); !line_str_p) {
        return false;
    } else {
        ctx->stack[passed_rbsp] = DerkJS::Value {line_str_p};
        return true;
    }
}

int main(int argc, char* argv[]) {
    using namespace DerkJS;

    if (argc < 2 || argc > 3) {
        std::println(std::cerr, "usage: ./derkjs [-v | [-d | -r] <script name>]");
        return 1;
    }

    Core::Driver driver {
        Core::DriverInfo {
            .name = "DerkJS",
            .author = "DrkWithT (GitHub)",
            .version_major = 0,
            .version_minor = 1,
            .version_patch = 2
        },
        1024 // increase object count limit for VM if needed
    };

    std::string source_path;
    std::string_view arg_1 = argv[1];

    if (arg_1 == "-h") {
        std::println(std::cerr, "usage: ./derkjs [-h | -v | [-d | -r] <script name>]\n\t-h: show help\n\t-v: show version & author");
        return 0;
    } else if (arg_1 == "-v") {
        const auto& [app_name, author_name, major, minor, patch] = driver.get_info();
        std::println("{} v{}.{}.{}\nBy: {}", app_name, major, minor, patch, author_name);
        return 0;
    } else if (arg_1 == "-d") {
        source_path = argv[2];
        driver.enable_bc_dump(true);
    } else if (arg_1 == "-r") {
        source_path = argv[2];
    }

    Core::NativePropertyStub console_obj_props[] {
        Core::NativePropertyStub {
            .name_str = "log",
            .item = std::make_unique<NativeFunction>(native_console_log)
        },
        Core::NativePropertyStub {
            .name_str = "readln",
            .item = std::make_unique<NativeFunction>(native_console_read_line)
        }
    }; 

    driver.add_js_lexical("var", TokenTag::keyword_var);
    driver.add_js_lexical("if", TokenTag::keyword_if);
    driver.add_js_lexical("else", TokenTag::keyword_else);
    driver.add_js_lexical("return", TokenTag::keyword_return);
    driver.add_js_lexical("while", TokenTag::keyword_while);
    driver.add_js_lexical("function", TokenTag::keyword_function);
    driver.add_js_lexical("prototype", TokenTag::keyword_prototype);
    driver.add_js_lexical("this", TokenTag::keyword_this);
    driver.add_js_lexical("new", TokenTag::keyword_new);
    driver.add_js_lexical("undefined", TokenTag::keyword_undefined);
    driver.add_js_lexical("null", TokenTag::keyword_null);
    driver.add_js_lexical("true", TokenTag::keyword_true);
    driver.add_js_lexical("false", TokenTag::keyword_false);
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

    driver.add_native_object(
        "console",
        std::to_array(std::move(console_obj_props))
    );

    return driver.run<DispatchPolicy::tco>(source_path) ? 0 : 1;
}
