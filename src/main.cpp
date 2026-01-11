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

    Core::Driver driver {DriverInfo {
        .name = "DerkJS",
        .author = "DrkWithT (GitHub)",
        .version_major = 0,
        .version_minor = 1,
        .version_patch = 0
    }};

    std::string source_path;
    std::string_view arg_1 = argv[1];

    if (arg_1 == "-h") {
        std::println(std::cerr, "usage: ./derkjs [-h | -v | [-d | -r] <script name>]\n\t-h: show help\n\t-v: show version & author");
        return 0;
    } else if (arg_1 == "-v") {
        const auto& [app_name, author_name, major, minor, patch] = driver.get_info();
        std::println("{} v{}.{}.{}\nBy: {}", app_name, author_name, major, minor, patch);
        return 0;
    } else if (arg_1 == "-d") {
        source_path = argv[2];
        driver.enable_bc_dump(true);
    } else if (arg_1 == "-r") {
        source_path = argv[2];
    }

    /// TODO: configure driver with "console"

    return driver.run<DispatchPolicy::loop_switch>() ? 0 : 1;
}
