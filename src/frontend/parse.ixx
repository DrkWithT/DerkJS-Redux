module;

// #include <utility>
// #include <memory>
// #include <stdexcept>
#include <optional>
// #include <string_view>
#include <string>
// #include <iostream>
// #include <print>

export module frontend.parse;

import frontend.lexicals;
import frontend.ast;

export namespace DerkJS {
    class Parser {
    private:
        Token m_previous;
        Token m_current;
        int m_error_count;

        // TODO: add advance, match, and consume methods

        // TODO: add grammar rule parse methods

    public:
        Parser() noexcept
        : m_previous {}, m_current {}, m_error_count {0} {}

        [[nodiscard]] auto operator()([[maybe_unused]] Lexer& lexer, [[maybe_unused]] const std::string& source) -> std::optional<ASTUnit> {
            return {};
        }
    };
}
