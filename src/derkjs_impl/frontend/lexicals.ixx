module;

#include <optional>
#include <string>
#include <string_view>
#include <flat_map>

export module frontend.lexicals;

export namespace DerkJS {
    enum class TokenTag : uint8_t {
        unknown,
        spaces,
        block_comment,
        identifier,
        keyword_var,
        keyword_if,
        keyword_else,
        keyword_return,
        keyword_while,
        keyword_function,
        keyword_prototype,
        keyword_new,
        keyword_this,
        keyword_undefined,
        keyword_null,
        keyword_true,
        keyword_false,
        literal_int,
        literal_real,
        literal_string,
        symbol_percent,
        symbol_times,
        symbol_slash,
        symbol_plus,
        symbol_minus,
        symbol_bang,
        symbol_equal,
        symbol_bang_equal,
        symbol_strict_equal,
        symbol_strict_bang_equal,
        symbol_less,
        symbol_less_equal,
        symbol_greater,
        symbol_greater_equal,
        symbol_amps, // `&&` symbol
        symbol_pipes, // `||` symbol
        symbol_assign,
        symbol_percent_assign,
        symbol_times_assign,
        symbol_slash_assign,
        symbol_plus_assign,
        symbol_minus_assign,
        left_paren,
        right_paren,
        left_brace,
        right_brace,
        left_bracket,
        right_bracket,
        dot,
        colon,
        comma,
        semicolon,
        eof,
    };

    struct LexicalItem {
        std::string_view lexeme;
        TokenTag tag;
    };

    struct Token {
        TokenTag tag;
        int start;
        int length;
        int line;
        int column;

        constexpr Token() noexcept
        : tag {TokenTag::unknown}, start {0}, length {1}, line {1}, column {1} {}

        constexpr Token(TokenTag tag_, int start_, int line_, int column_) noexcept
        : Token {tag_, start_, 1, line_, column_} {}

        constexpr Token(TokenTag tag_, int start_, int length_, int line_, int column_) noexcept
        : tag {tag_}, start {start_}, length {length_}, line {line_}, column {column_} {}

        [[nodiscard]] constexpr auto match_tag_to(this auto&& self, std::same_as<TokenTag> auto first, std::same_as<TokenTag> auto ... more) noexcept -> bool {
            return ((self.tag == first) || ... || (self.tag == more));
        }

        [[nodiscard]] constexpr auto as_str(this auto&& self, std::string_view source_view) noexcept -> std::string_view {
            return source_view.substr(self.start, self.length);
        }

        [[nodiscard]] constexpr auto as_string(this auto&& self, const std::string& source) noexcept -> std::string {
            return source.substr(self.start, self.length);
        }
    };

    class Lexer {
    private:
        // Maps unique, special lexemes to keywords & operators. All string_views here MUST view string literals, having lifetimes akin to Rust's `&'static str`.
        std::flat_map<std::string_view, TokenTag> m_specials;
        int m_pos;
        int m_end;
        int m_line;
        int m_column;

        [[nodiscard]] static constexpr auto is_any_of(char c, std::same_as<char> auto first, std::same_as<char> auto ... more) noexcept -> bool {
            return ((c == first) || ... || (c == more));
        }

        [[nodiscard]] static constexpr auto is_whitespace(char c) noexcept -> bool {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }

        [[nodiscard]] static constexpr auto is_alphabetic(char c) noexcept -> bool {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
        }

        [[nodiscard]] static constexpr auto is_digit(char c) noexcept -> bool {
            return (c >= '0' && c <= '9');
        }

        [[nodiscard]] static constexpr auto is_numeric(char c) noexcept -> bool {
            return is_digit(c) || c == '.';
        }

        [[nodiscard]] static constexpr auto is_op_symbol(char c) noexcept -> bool {
            return is_any_of(c, '%', '*', '/', '+', '-', '=', '!', '<', '>', '&', '|');
        }

        [[nodiscard]] auto lookup_lexeme_as_special(std::string_view lexeme) noexcept -> std::optional<TokenTag> {
            if (auto maybe_word_tag = m_specials.find(lexeme); maybe_word_tag != m_specials.end()) {
                return maybe_word_tag->second;
            }

            return {};
        }

        [[nodiscard]] constexpr auto at_eof() const noexcept -> bool {
            return m_pos >= m_end;
        }

        void update_source_location(char c) noexcept {
            if (c == '\n') {
                ++m_line;
                m_column = 1;
            } else {
                ++m_column;
            }

            ++m_pos;
        }

        [[nodiscard]] auto lex_single(const std::string& source, TokenTag tag) noexcept -> Token {
            const auto temp_start = m_pos;
            const auto temp_line = m_line;
            const auto temp_column = m_column;

            update_source_location(source.at(m_pos));

            return {
                tag,
                temp_start,
                temp_line,
                temp_column
            };
        }

        [[nodiscard]] auto lex_whitespace(const std::string& source) noexcept -> Token {
            auto temp_start = m_pos;
            auto temp_length = 0;
            const auto temp_line = m_line;
            const auto temp_column = m_column;

            while (!at_eof()) {
                if (const auto c = source.at(m_pos); is_whitespace(c)) {
                    update_source_location(c);
                    ++temp_length;
                } else {
                    break;
                }
            }

            return {
                TokenTag::spaces,
                temp_start,
                temp_length,
                temp_line,
                temp_column
            };
        }

        [[nodiscard]] auto lex_block_comment(const std::string& source) noexcept -> Token {
            auto temp_start = m_pos;
            auto temp_length = 0;
            const auto temp_line = m_line;
            const auto temp_column = m_column;
            bool closed = false;

            while (!at_eof()) {
                if (const auto c = source.at(m_pos); c != '*') {
                    update_source_location(c);
                    ++temp_length;
                } else {
                    break;
                }
            }

            // NOTE: handle the ending of a JS block comment: '*/'
            update_source_location(source.at(m_pos));

            if (const auto last_comment_delim = source.at(m_pos); last_comment_delim == '/') {
                update_source_location(last_comment_delim);
                closed = true;
            }

            return {
                (closed) ? TokenTag::block_comment : TokenTag::unknown,
                temp_start,
                temp_length,
                temp_line,
                temp_column
            };
        }

        [[nodiscard]] auto lex_numeric(const std::string& source) noexcept -> Token {
            auto temp_start = m_pos;
            auto temp_length = 0;
            const auto temp_line = m_line;
            const auto temp_column = m_column;
            auto dot_count = 0;

            if (const auto prefix_c = source.at(m_pos); prefix_c == '-') {
                ++temp_length;
                ++m_pos;
            }

            while (!at_eof()) {
                if (const auto c = source.at(m_pos); is_numeric(c)) {
                    update_source_location(c);
                    ++temp_length;

                    if (c == '.') ++dot_count;
                } else {
                    break;
                }
            }

            const auto checked_tag = ([] (int dots) noexcept -> TokenTag {
                switch (dots) {
                case 0: return TokenTag::literal_int;
                case 1: return TokenTag::literal_real;
                default: return TokenTag::unknown;
                }
            })(dot_count);

            return {
                checked_tag,
                temp_start,
                temp_length,
                temp_line,
                temp_column
            };
        }

        [[nodiscard]] auto lex_word(const std::string& source) noexcept -> Token {
            auto temp_start = m_pos;
            auto temp_length = 0;
            const auto temp_line = m_line;
            const auto temp_column = m_column;

            while (!at_eof()) {
                if (const auto c = source.at(m_pos); is_alphabetic(c) || is_digit(c)) {
                    update_source_location(c);
                    ++temp_length;
                } else {
                    break;
                }
            }

            std::string_view word_lexeme {
                source.begin() + temp_start,
                source.begin() + temp_start + temp_length
            };

            const auto checked_tag = lookup_lexeme_as_special(word_lexeme).value_or(TokenTag::identifier);

            return {
                checked_tag,
                temp_start,
                temp_length,
                temp_line,
                temp_column
            };
        }

        [[nodiscard]] auto lex_operator(const std::string& source) noexcept -> Token {
            auto temp_start = m_pos;
            auto temp_length = 0;
            const auto temp_line = m_line;
            const auto temp_column = m_column;

            while (!at_eof()) {
                if (const auto c = source.at(m_pos); is_op_symbol(c)) {
                    update_source_location(c);
                    ++temp_length;
                } else {
                    break;
                }
            }

            std::string_view operator_lexeme {
                source.begin() + temp_start,
                source.begin() + temp_start + temp_length
            };

            const auto checked_tag = lookup_lexeme_as_special(operator_lexeme).value_or(TokenTag::unknown);

            return {
                checked_tag,
                temp_start,
                temp_length,
                temp_line,
                temp_column
            };
        }

    public:
        Lexer(std::string_view source, std::flat_map<std::string_view, TokenTag> lexicals) noexcept
        : m_specials (std::move(lexicals)), m_pos {}, m_end {static_cast<int>(source.size())}, m_line {1}, m_column {1} {}

        [[nodiscard]] auto operator()(const std::string& source) -> Token {
            if (at_eof()) {
                return {TokenTag::eof, m_end, m_line, m_column};
            }

            const auto peek_0 = source.at(m_pos);

            switch (peek_0) {
            case '(': return lex_single(source, TokenTag::left_paren);
            case ')': return lex_single(source, TokenTag::right_paren);
            case '{': return lex_single(source, TokenTag::left_brace);
            case '}': return lex_single(source, TokenTag::right_brace);
            case '[': return lex_single(source, TokenTag::left_bracket);
            case ']': return lex_single(source, TokenTag::right_bracket);
            case '.': return lex_single(source, TokenTag::dot);
            case ':': return lex_single(source, TokenTag::colon);
            case ',': return lex_single(source, TokenTag::comma);
            case ';': return lex_single(source, TokenTag::semicolon);
            default: break;
            }

            if (const auto peek_1 = source[m_pos + 1]; peek_0 == '/' && peek_1 == '*') {
                update_source_location(peek_0);
                update_source_location(peek_1);

                return lex_block_comment(source);
            } else if (is_whitespace(peek_0)) {
                return lex_whitespace(source);
            } else if ((peek_0 == '-' && is_numeric(peek_1)) || is_numeric(peek_0)) {
                return lex_numeric(source);
            } else if (is_op_symbol(peek_0)) {
                return lex_operator(source);
            } else if (is_alphabetic(peek_0)) {
                return lex_word(source);
            }

            return lex_single(source, TokenTag::unknown);
        }
    };
}