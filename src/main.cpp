#include <string>
#include <filesystem>
#include <print>
#include <iostream>
#include <sstream>
#include <fstream>

import frontend.lexicals;
import frontend.ast;
import frontend.parse;

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

    if (std::string_view arg_1 = argv[1]; arg_1 == "-v") {
        std::println("DerkJS v0.0.1\nBy: DrkWithT (GitHub)");
        return 0;
    }

    std::string source {read_file(argv[1])};
    Lexer lexer {source};

    while (true) {
        if (auto [tag, start, length, line, column] = lexer(source); tag == TokenTag::unknown) {
            std::println("\x1b[1;31mLexer Error\x1b[0m [ln {}, col {}]:\n\tUnknown token \x1b[0;33m'{}'\x1b[0m", line, column, source.substr(start, length));
            return 1;
        } else if (tag == TokenTag::eof) {
            break;
        }
    }
}
