#include <string>
#include <array>
#include <flat_map>
#include <print>
#include <iostream>

import derkjs_impl;

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
    } else {
        std::println(std::cerr, "usage: ./derkjs [-h | -v | [-d | -r] <script name>]\n\t-h: show help\n\t-v: show version & author");
        return 1;
    }

    Core::NativePropertyStub console_obj_props[] {
        Core::NativePropertyStub {
            .name_str = "log",
            .item = std::make_unique<NativeFunction>(DerkJS::native_console_log)
        },
        Core::NativePropertyStub {
            .name_str = "readln",
            .item = std::make_unique<NativeFunction>(DerkJS::native_console_read_line)
        }
    };

    Core::NativePropertyStub clock_obj_props[] {
        Core::NativePropertyStub {
            .name_str = "now",
            .item = std::make_unique<NativeFunction>(DerkJS::clock_time_now)
        }
    };

    Core::NativePropertyStub array_obj_props[] {
        Core::NativePropertyStub {
            .name_str = "constructor",
            .item = std::make_unique<NativeFunction>(DerkJS::native_array_ctor)
        },
        Core::NativePropertyStub {
            .name_str = "push",
            .item = std::make_unique<NativeFunction>(DerkJS::native_array_push)
        },
        Core::NativePropertyStub {
            .name_str = "pop",
            .item = std::make_unique<NativeFunction>(DerkJS::native_array_pop)
        },
        Core::NativePropertyStub {
            .name_str = "at",
            .item = std::make_unique<NativeFunction>(DerkJS::native_array_at)
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

    driver.add_native_object<Object>(
        "console",
        std::to_array(std::move(console_obj_props)),
        nullptr // TODO: add JSObject as prototype.
    );

    driver.add_native_object<Object>(
        "clock",
        std::to_array(std::move(clock_obj_props)),
        nullptr // TODO: add JSObject as prototype.
    );

    // Add `Array.prototype` object here!
    auto array_prototype_object_p = driver.add_anonymous_native_object(
        std::to_array(std::move(array_obj_props))
    );

    if (!array_prototype_object_p) {
        std::println(std::cerr, "SETUP ERROR: failed to allocate Array.prototype object.");
        return 1;
    }

    driver.add_native_object<Array>(
        "Array",
        std::to_array(std::move(array_obj_props)),
        array_prototype_object_p
    );

    return driver.run<DispatchPolicy::tco>(source_path) ? 0 : 1;
}
