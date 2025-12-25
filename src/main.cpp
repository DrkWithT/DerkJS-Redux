#include <string>
#include <filesystem>
#include <print>
#include <iostream>
#include <sstream>
#include <fstream>

import frontend.lexicals;
import frontend.ast;
import frontend.parse;
import frontend.semantics;

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

int main(int argc, char* argv[]) {
    using namespace DerkJS;

    if (argc != 2) {
        std::println(std::cerr, "usage: ./derkjs [-v | <script name>]");
        return 1;
    }

    std::string_view arg_1 = argv[1];

    if (arg_1 == "-v") {
        std::println("DerkJS v0.0.1\nBy: DrkWithT (GitHub)");
        return 0;
    }

    std::string source {read_file(argv[1])};
    Lexer lexer {source};
    Parser parser;

    auto ast_opt = parser(lexer, std::string {argv[1]}, source);

    if (!ast_opt) {
        return 1;
    }

    ASTUnit full_ast = std::move(ast_opt.value());

    std::println("Parsed {} top-level statements for source '{}'", full_ast.size(), arg_1);

    SemanticAnalyzer sema_check_pass;

    std::println("Semantic check (dud): {}", sema_check_pass(full_ast, {{0, source}}));

    return 1;
}
