#include <string>
#include <flat_map>
#include <filesystem>
#include <print>
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>

import derkjs_impl;

constexpr std::size_t default_stack_size = 2048;
constexpr std::size_t default_call_depth_limit = 208;

[[nodiscard]] auto read_file(std::filesystem::path file_path) -> std::string {
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

[[nodiscard]] constexpr auto run_script_bytecode(DerkJS::Program& prgm, std::size_t stack_limit, std::size_t recursion_limit) -> DerkJS::ExitStatus {
    DerkJS::VM<DerkJS::DispatchPolicy::loop_switch> vm {prgm, stack_limit, recursion_limit};

    return vm();
}

int main(int argc, char* argv[]) {
    using namespace DerkJS;

    if (argc < 2 || argc > 3) {
        std::println(std::cerr, "usage: ./derkjs [-v | [-d | -r] <script name>]");
        return 1;
    }

    std::string_view arg_1 = argv[1];
    std::string source_path;
    auto enable_bytecode_dump = false;

    if (arg_1 == "-v") {
        std::println("DerkJS v0.0.1\nBy: DrkWithT (GitHub)");
        return 0;
    }
    
    if (arg_1 == "-d") {
        source_path = argv[2];
        enable_bytecode_dump = true;
    } else if (arg_1 == "-r") {
        source_path = argv[2];
    } else {
        std::println(std::cerr, "usage: ./derkjs [-v | [-d | -r] <script name>]");
        return 1;
    }

    std::string source {read_file(source_path)};
    std::flat_map<int, std::string> source_map;
    source_map[0] = source;

    Lexer lexer {source};
    Parser parser;

    auto ast_opt = parser(lexer, source_path, source);

    if (!ast_opt) {
        return 1;
    }

    ASTUnit full_ast = std::move(ast_opt.value());

    SemanticAnalyzer sema_check_pass;

    BytecodeGenPass codegen_pass;

    auto program_opt = codegen_pass(full_ast, source_map);

    if (!program_opt) {
        std::println(std::cerr, "Could not compile program.");
        return 1;
    }

    if (enable_bytecode_dump) {
        disassemble_program(program_opt.value());
    }

    auto derkjs_start_time = std::chrono::steady_clock::now();
    const auto vm_status = run_script_bytecode(program_opt.value(), default_stack_size, default_call_depth_limit);
    auto derkjs_running_time = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - derkjs_start_time);

    std::println("Finished in \x1b[1;33m{}\x1b[0m", derkjs_running_time);

    return (vm_status == ExitStatus::ok) ? 0 : 1 ;
}
