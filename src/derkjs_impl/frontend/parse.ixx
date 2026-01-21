module;

#include <utility>
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <print>
#include <format>

export module frontend.parse;

import frontend.lexicals;
import frontend.ast;

export namespace DerkJS {
    enum class SyntaxTag: uint8_t {
        expr_primary,
        expr_object,
        expr_lambda,
        expr_member,
        expr_new,
        expr_call,
        expr_unary,
        expr_factor,
        expr_term,
        expr_compare,
        expr_equality,
        program_top,
        stmt_var,
        stmt_if,
        stmt_return,
        stmt_while,
        stmt_function,
        stmt_expr,
        last,
    };

    constexpr std::array<std::string_view, static_cast<std::size_t>(SyntaxTag::last)> syntax_names = {
        "expr-primary",
        "expr-object",
        "expr-lambda", 
        "expr-member",
        "expr-new",
        "expr-call",
        "expr-unary",
        "expr-factor",
        "expr-term",
        "expr-compare",
        "expr-equality",
        "program-top",
        "stmt-var",
        "stmt-if",
        "stmt-return",
        "stmt-while",
        "stmt-function",
        "stmt-expr",
    };

    [[nodiscard]] constexpr auto syntax_tag_name(SyntaxTag tag) -> std::string_view {
        return syntax_names.at(static_cast<std::size_t>(tag));
    }

    class Parser {
    private:
        Token m_previous;
        Token m_current;
        int m_error_count;
        SyntaxTag m_syntax;

        void report_syntax_error(const std::string& source, std::string msg, Token culprit) noexcept(false) {
            const auto [culprit_tag, culprit_start, culprit_length, culprit_line, culprit_column] = culprit;

            ++m_error_count;

            throw std::runtime_error {std::format(
                "\x1b[1;31mSyntax Error\x1b[0m \x1b[1;33m[ln {}, col {}]\x1b[0m:\n\tnote: {}\n\tculprit token: '{}'",
                culprit_line,
                culprit_column,
                msg,
                culprit.as_str(source)
            )};
        }

        [[nodiscard]] auto at_eof() const noexcept -> bool {
            return m_current.tag == TokenTag::eof;
        }

        [[nodiscard]] auto advance(Lexer& lexer, const std::string& source) -> Token {
            Token temp;

            do {
                temp = lexer(source);

                if (const auto temp_tag = temp.tag; temp_tag == TokenTag::spaces || temp_tag == TokenTag::block_comment) {
                    continue;
                }

                break;
            } while (true);

            return temp;
        }

        void consume_any(Lexer& lexer, const std::string& source) {
            m_previous = m_current;
            m_current = advance(lexer, source);
        }

        void consume(Lexer& lexer, const std::string& source, std::same_as<TokenTag> auto first, std::same_as<TokenTag> auto ... more) {
            if (m_current.match_tag_to(first, more...) && !m_current.match_tag_to(TokenTag::unknown)) {
                m_previous = m_current;
                m_current = advance(lexer, source);
                return;
            }

            /// FIXME: The syntax tags are not properly tracked... A reversed stack is needed to track the most appropriate syntax reached.
            report_syntax_error(
                source,
                std::format("Invalid token found in {}", syntax_tag_name(m_syntax)),
                m_current
            );
        }

        [[nodiscard]] auto parse_primary(Lexer& lexer, const std::string& source) -> ExprPtr {
            m_syntax = SyntaxTag::expr_primary;

            const auto current_token_tag = m_current.tag;
            Token current_token_copy = m_current;
            const auto snippet_begin = current_token_copy.start;
            const auto primary_txt_length = current_token_copy.length;

            switch (current_token_tag) {
            case TokenTag::keyword_undefined:
            case TokenTag::keyword_null:
            case TokenTag::keyword_this:
            case TokenTag::identifier:
            case TokenTag::keyword_true:
            case TokenTag::keyword_false:
            case TokenTag::literal_int:
            case TokenTag::literal_real:
            case TokenTag::literal_string:
                consume_any(lexer, source);

                return std::make_unique<Expr>(
                    Primitive { .token = current_token_copy, },
                    0, // NOTE: `src_id` is 0 since I'll only support single-file JS programs for now.
                    snippet_begin,
                    primary_txt_length
                );
            case TokenTag::left_brace:
                return parse_object(lexer, source);
            case TokenTag::keyword_function:
                return parse_lambda(lexer, source);
            case TokenTag::left_paren:
                {
                    consume_any(lexer, source);
                    auto enclosed_expr = parse_logical_or(lexer, source);
                    consume(lexer, source, TokenTag::right_paren);

                    return enclosed_expr;
                }
            default: break;
            }

            report_syntax_error(source, "Unexpected token in a primary expr.", m_current);

            std::unreachable();
        }

        [[nodiscard]] auto parse_field(Lexer& lexer, const std::string& source) -> ObjectField {
            consume(lexer, source, TokenTag::identifier); // TODO: add string key support for object fields!

            Token field_name = m_previous;

            consume(lexer, source, TokenTag::colon);

            return ObjectField {
                .name = field_name,
                .value = parse_logical_or(lexer, source)
            };
        }

        [[nodiscard]] auto parse_object(Lexer& lexer, const std::string& source) -> ExprPtr {
            const auto object_lexeme_begin = m_current.start;
            consume_any(lexer, source); // eat pre-checked '{' since this is only called from parse_primary()

            std::vector<ObjectField> fields;

            if (m_current.match_tag_to(TokenTag::right_brace)) {
                consume_any(lexer, source);

                return std::make_unique<Expr>(
                    ObjectLiteral {
                        .fields = std::move(fields)
                    },
                    0,
                    object_lexeme_begin,
                    m_current.start - object_lexeme_begin + 1
                );
            }

            fields.emplace_back(parse_field(lexer, source));

            while (!at_eof()) {
                if (m_current.match_tag_to(TokenTag::right_brace)) {
                    consume_any(lexer, source);
                    break;
                }

                consume(lexer, source, TokenTag::comma);
                fields.emplace_back(parse_field(lexer, source));
            }

            return std::make_unique<Expr>(
                ObjectLiteral {
                    .fields = std::move(fields)
                },
                0,
                object_lexeme_begin,
                m_current.start - object_lexeme_begin + 1
            );
        }

        [[nodiscard]] auto parse_lambda(Lexer& lexer, const std::string& source) -> ExprPtr {
            m_syntax = SyntaxTag::expr_lambda;

            const auto snippet_begin = m_current.start;
            std::vector<Token> param_tokens;

            consume_any(lexer, source); // skip pre-checked 'function'
            consume(lexer, source, TokenTag::left_paren);

            if (!m_current.match_tag_to(TokenTag::right_paren)) {
                consume(lexer, source, TokenTag::identifier);
                param_tokens.emplace_back(m_previous);
            }

            while (!at_eof()) {
                if (!m_current.match_tag_to(TokenTag::comma)) {
                    break;
                }

                consume_any(lexer, source);
                consume(lexer, source, TokenTag::identifier);

                param_tokens.emplace_back(m_previous);
            }

            consume(lexer, source, TokenTag::right_paren);

            return std::make_unique<Expr>(
                LambdaLiteral {
                    .params = std::move(param_tokens),
                    .body = parse_block(lexer, source)
                },
                0,
                snippet_begin,
                m_current.start - snippet_begin + 1
            );
        }

        [[nodiscard]] auto parse_member(Lexer& lexer, const std::string& source) -> ExprPtr {
            m_syntax = SyntaxTag::expr_member;

            const auto snippet_begin = m_current.start;
            auto lhs_primary = parse_primary(lexer, source);

            if (const auto member_mark = m_current.tag; member_mark == TokenTag::dot) {
                consume_any(lexer, source);

                return std::make_unique<Expr>(
                    MemberAccess {
                        .target = std::move(lhs_primary),
                        .key = parse_member(lexer, source),
                    },
                    0,
                    snippet_begin,
                    m_current.start - snippet_begin + 1
                );
            } else if (member_mark == TokenTag::left_bracket) {
                consume_any(lexer, source);

                auto enclosed_expr = parse_logical_or(lexer, source);

                consume(lexer, source, TokenTag::right_bracket);

                return std::make_unique<Expr>(
                    MemberAccess {
                        .target = std::move(lhs_primary),
                        .key = std::move(enclosed_expr),
                    },
                    0,
                    snippet_begin,
                    m_current.start - snippet_begin + 1              
                );
            }

            return lhs_primary;
        }

        [[nodiscard]] auto parse_new(Lexer& lexer, const std::string& source) -> ExprPtr {
            m_syntax = SyntaxTag::expr_new;

            bool has_new = false;
            const auto snippet_begin = m_current.start;

            if (const auto leading_keyword = m_current.tag; leading_keyword == TokenTag::keyword_new) {
                consume_any(lexer, source);
                has_new = true;
            }

            auto inner_primary = parse_member(lexer, source);

            if (!has_new) {
                return inner_primary;
            }

            return std::make_unique<Expr>(
                Unary {
                    .inner = std::move(inner_primary),
                    .op = AstOp::ast_op_new,
                },
                0,
                snippet_begin,
                m_current.start - snippet_begin + 1
            );
        }

        [[nodiscard]] auto parse_call(Lexer& lexer, const std::string& source) -> ExprPtr {
            m_syntax = SyntaxTag::expr_call;

            auto callee_new_expr = parse_new(lexer, source);
            const auto snippet_begin = m_current.start;

            if (!m_current.match_tag_to(TokenTag::left_paren)) {
                return callee_new_expr;
            }

            consume_any(lexer, source);

            std::vector<ExprPtr> args;

            if (m_current.match_tag_to(TokenTag::right_paren)) {
                consume_any(lexer, source);

                return std::make_unique<Expr>(
                    Call {
                        .args = std::move(args),
                        .callee = std::move(callee_new_expr)
                    },
                    0,
                    snippet_begin,
                    m_current.start - snippet_begin + 1
                );
            }

            args.emplace_back(parse_logical_or(lexer, source));

            while (!at_eof()) {
                if (!m_current.match_tag_to(TokenTag::comma)) {
                    break;
                }

                consume_any(lexer, source);

                args.emplace_back(parse_logical_or(lexer, source));
            }

            consume(lexer, source, TokenTag::right_paren);

            return std::make_unique<Expr>(
                Call {
                    .args = std::move(args),
                    .callee = std::move(callee_new_expr)
                },
                0,
                snippet_begin,
                m_current.start - snippet_begin + 1
            );
        }

        [[nodiscard]] auto parse_unary(Lexer& lexer, const std::string& source) -> ExprPtr {
            m_syntax = SyntaxTag::expr_unary;

            const auto snippet_begin = m_current.start;
            const auto unary_op = ([](TokenTag tag) -> AstOp {
                switch (tag) {
                case TokenTag::symbol_bang: return AstOp::ast_op_bang;
                case TokenTag::symbol_plus: return AstOp::ast_op_plus;
                default: return AstOp::ast_op_noop;
                }
            })(m_current.tag);

            if (unary_op != AstOp::ast_op_noop) {
                consume_any(lexer, source);

                return std::make_unique<Expr>(
                    Unary {
                        .inner = parse_call(lexer, source),
                        .op = unary_op
                    },
                    0,
                    snippet_begin,
                    m_current.start - snippet_begin + 1
                );
            }

            return parse_call(lexer, source);
        }

        [[nodiscard]] auto parse_factor(Lexer& lexer, const std::string& source) -> ExprPtr {
            m_syntax = SyntaxTag::expr_factor;

            const auto snippet_begin = m_current.start;
            auto lhs = parse_unary(lexer, source);

            while (!at_eof()) {
                if (!m_current.match_tag_to(TokenTag::symbol_percent, TokenTag::symbol_times, TokenTag::symbol_slash)) {
                    break;
                }

                auto factor_op = ([](TokenTag tag) noexcept -> AstOp {
                    switch (tag) {
                    case TokenTag::symbol_percent: return AstOp::ast_op_percent;
                    case TokenTag::symbol_times: return AstOp::ast_op_times;
                    case TokenTag::symbol_slash: default: return AstOp::ast_op_slash;
                    }
                })(m_current.tag);

                consume_any(lexer, source);

                lhs = std::make_unique<Expr>(
                    Binary {
                        .lhs = std::move(lhs),
                        .rhs = parse_unary(lexer, source),
                        .op = factor_op
                    },
                    0,
                    snippet_begin,
                    m_current.start - snippet_begin + 1
                );
            }

            return lhs;
        }

        [[nodiscard]] auto parse_term(Lexer& lexer, const std::string& source) -> ExprPtr {
            m_syntax = SyntaxTag::expr_term;

            const auto snippet_begin = m_current.start;
            auto lhs = parse_factor(lexer, source);

            while (!at_eof()) {
                if (!m_current.match_tag_to(TokenTag::symbol_plus, TokenTag::symbol_minus)) {
                    break;
                }

                const auto term_op = (m_current.tag == TokenTag::symbol_plus)
                    ? AstOp::ast_op_plus
                    : AstOp::ast_op_minus;

                consume_any(lexer, source);

                lhs = std::make_unique<Expr>(
                    Binary {
                        .lhs = std::move(lhs),
                        .rhs = parse_factor(lexer, source),
                        .op = term_op
                    },
                    0,
                    snippet_begin,
                    m_current.start - snippet_begin + 1
                );
            }

            return lhs;
        }

        [[nodiscard]] auto parse_compare(Lexer& lexer, const std::string& source) -> ExprPtr {
            m_syntax = SyntaxTag::expr_compare;

            const auto snippet_begin = m_current.start;
            auto lhs = parse_term(lexer, source);

            while (!at_eof()) {
                if (!m_current.match_tag_to(TokenTag::symbol_less, TokenTag::symbol_less_equal, TokenTag::symbol_greater, TokenTag::symbol_greater_equal)) {
                    break;
                }

                const auto compare_op = ([](TokenTag tag) noexcept -> AstOp {
                    switch (tag) {
                    case TokenTag::symbol_less: return AstOp::ast_op_less;
                    case TokenTag::symbol_less_equal: return AstOp::ast_op_less_equal;
                    case TokenTag::symbol_greater: return AstOp::ast_op_greater;
                    case TokenTag::symbol_greater_equal: default: return AstOp::ast_op_greater_equal;
                    }
                })(m_current.tag);

                consume_any(lexer, source);

                lhs = std::make_unique<Expr>(
                    Binary {
                        .lhs = std::move(lhs),
                        .rhs = parse_term(lexer, source),
                        .op = compare_op
                    },
                    0,
                    snippet_begin,
                    m_current.start - snippet_begin + 1
                );
            }

            return lhs;
        }

        [[nodiscard]] auto parse_equality(Lexer& lexer, const std::string& source) -> ExprPtr {
            m_syntax = SyntaxTag::expr_equality;

            const auto snippet_begin = m_current.start;
            auto lhs = parse_compare(lexer, source);

            while (!at_eof()) {
                if (!m_current.match_tag_to(TokenTag::symbol_equal, TokenTag::symbol_bang_equal, TokenTag::symbol_strict_equal, TokenTag::symbol_strict_bang_equal)) {
                    break;
                }

                const auto compare_op = ([](TokenTag tag) noexcept -> AstOp {
                    switch (tag) {
                    case TokenTag::symbol_equal: return AstOp::ast_op_equal;
                    case TokenTag::symbol_bang_equal: return AstOp::ast_op_bang_equal;
                    case TokenTag::symbol_strict_equal: return AstOp::ast_op_strict_equal;
                    case TokenTag::symbol_strict_bang_equal: default: return AstOp::ast_op_strict_bang_equal;
                    }
                })(m_current.tag);

                consume_any(lexer, source);

                lhs = std::make_unique<Expr>(
                    Binary {
                        .lhs = std::move(lhs),
                        .rhs = parse_compare(lexer, source),
                        .op = compare_op
                    },
                    0,
                    snippet_begin,
                    m_current.start - snippet_begin + 1
                );
            }

            return lhs;
        }

        [[nodiscard]] auto parse_logical_and(Lexer& lexer, const std::string& source) -> ExprPtr {
            const auto snippet_begin = m_current.start;
            auto and_lhs = parse_equality(lexer, source);

            while (!at_eof()) {
                if (!m_current.match_tag_to(TokenTag::symbol_amps)) {
                    break;
                }

                consume_any(lexer, source);

                and_lhs = std::make_unique<Expr>(
                    Binary {
                        .lhs = std::move(and_lhs),
                        .rhs = parse_equality(lexer, source),
                        .op = AstOp::ast_op_and
                    },
                    0,
                    snippet_begin,
                    m_current.start - snippet_begin + 1
                );
            }

            return and_lhs;
        }

        [[nodiscard]] auto parse_logical_or(Lexer& lexer, const std::string& source) -> ExprPtr {
            const auto snippet_begin = m_current.start;
            auto or_lhs = parse_logical_and(lexer, source);

            while (!at_eof()) {
                if (!m_current.match_tag_to(TokenTag::symbol_pipes)) {
                    break;
                }

                consume_any(lexer, source);

                or_lhs = std::make_unique<Expr>(
                    Binary {
                        .lhs = std::move(or_lhs),
                        .rhs = parse_logical_and(lexer, source),
                        .op = AstOp::ast_op_or
                    },
                    0,
                    snippet_begin,
                    m_current.start - snippet_begin + 1
                );
            }

            return or_lhs;
        }

        [[nodiscard]] auto parse_stmt(Lexer& lexer, const std::string& source) -> StmtPtr {
            if (const auto stmt_keyword = m_current.tag; stmt_keyword == TokenTag::keyword_var) {
                return parse_variable(lexer, source);
            } else if (stmt_keyword == TokenTag::keyword_if) {
                return parse_if(lexer, source);
            } else if (stmt_keyword == TokenTag::keyword_return) {
                return parse_return(lexer, source);
            } else if (stmt_keyword == TokenTag::keyword_while) {
                return parse_while(lexer, source);
            } else if (stmt_keyword == TokenTag::keyword_function) {
                return parse_function(lexer, source);
            } else {
                return parse_expr_stmt(lexer, source);
            }
        }

        [[nodiscard]] auto parse_var_decl(Lexer& lexer, const std::string& source) -> VarDecl {
            const auto snippet_begin = m_current.start;

            consume(lexer, source, TokenTag::identifier);

            Token name_token = m_previous;

            /// NOTE: handle other case of `var x;`... so this example would make `var x = undefined;`
            if (!m_current.match_tag_to(TokenTag::symbol_assign)) {
                return VarDecl {
                    .name = name_token,
                    .rhs = std::make_unique<Expr>(
                        Primitive {
                            .token = Token {TokenTag::keyword_undefined, 0, 0, name_token.line, name_token.column + name_token.length}
                        },
                        0,
                        snippet_begin,
                        snippet_begin
                    )
                };
            }

            consume_any(lexer, source);

            return VarDecl {
                .name = name_token,
                .rhs = parse_logical_or(lexer, source)
            };
        }

        [[nodiscard]] auto parse_variable(Lexer& lexer, const std::string& source) -> StmtPtr {
            m_syntax = SyntaxTag::stmt_var;

            const auto snippet_begin = m_current.start;
            consume_any(lexer, source); // skip 'var'

            std::vector<VarDecl> vars;

            vars.emplace_back(parse_var_decl(lexer, source));

            while (!at_eof()) {
                if (!m_current.match_tag_to(TokenTag::comma)) {
                    break;
                }

                consume_any(lexer, source);

                vars.emplace_back(parse_var_decl(lexer, source));
            }

            consume(lexer, source, TokenTag::semicolon);

            return std::make_unique<Stmt>(
                Variables {
                    .vars = std::move(vars)
                },
                0,
                snippet_begin,
                m_current.start - snippet_begin + 1
            );
        }

        [[nodiscard]] auto parse_if(Lexer& lexer, const std::string& source) -> StmtPtr {
            m_syntax = SyntaxTag::stmt_if;

            const auto snippet_begin = m_current.start;

            consume_any(lexer, source); // skip 'if'
            consume(lexer, source, TokenTag::left_paren);

            auto condition_expr = parse_logical_or(lexer, source);

            consume(lexer, source, TokenTag::right_paren);

            /// TODO: add looser syntax paths for ifs' true bodies...
            auto block_true = parse_block(lexer, source);

            std::unique_ptr<Stmt> stmt_falsy;

            if (const auto current_token_tag = m_current.tag; current_token_tag == TokenTag::keyword_else) {
                consume_any(lexer, source);

                if (const auto current_token_tag = m_current.tag; current_token_tag == TokenTag::left_brace) {
                    stmt_falsy = parse_block(lexer, source);
                } else if (current_token_tag == TokenTag::keyword_if) {
                    stmt_falsy = parse_if(lexer, source);
                } else if (current_token_tag == TokenTag::keyword_return) {
                    stmt_falsy = parse_return(lexer, source);
                } else {
                    stmt_falsy = parse_expr_stmt(lexer, source);
                }
            }

            return std::make_unique<Stmt>(
                If {
                    .check = std::move(condition_expr),
                    .body_true = std::move(block_true),
                    .body_false = std::move(stmt_falsy)
                },
                0,
                snippet_begin,
                m_current.start - snippet_begin + 1
            );
        }

        [[nodiscard]] auto parse_return(Lexer& lexer, const std::string& source) -> StmtPtr {
            m_syntax = SyntaxTag::stmt_return;

            const auto snippet_begin = m_current.start;

            consume_any(lexer, source); // skip 'return'

            auto result_expr = parse_logical_or(lexer, source);

            consume(lexer, source, TokenTag::semicolon);

            return std::make_unique<Stmt>(
                Return {
                    .result = std::move(result_expr)
                },
                0,
                snippet_begin,
                m_current.start - snippet_begin + 1
            );
        }

        [[nodiscard]] auto parse_while(Lexer& lexer, const std::string& source) -> StmtPtr {
            m_syntax = SyntaxTag::stmt_while;

            const auto snippet_begin = m_current.start;

            consume_any(lexer, source); // skip pre-checked 'while'
            consume(lexer, source, TokenTag::left_paren);

            auto loop_check_expr = parse_logical_or(lexer, source);

            consume(lexer, source, TokenTag::right_paren);

            auto loop_body = parse_block(lexer, source);

            return std::make_unique<Stmt>(
                While {
                    .check = std::move(loop_check_expr),
                    .body = std::move(loop_body)
                },
                0,
                snippet_begin,
                m_current.start - snippet_begin + 1
            );
        }

        [[nodiscard]] auto parse_function(Lexer& lexer, const std::string& source) -> StmtPtr {
            m_syntax = SyntaxTag::stmt_function;

            const auto snippet_begin = m_current.start;

            consume_any(lexer, source); // skip pre-checked 'function'
            consume(lexer, source, TokenTag::identifier);

            Token name_token = m_previous;

            consume(lexer, source, TokenTag::left_paren);

            std::vector<Token> params;

            if (!m_current.match_tag_to(TokenTag::right_paren)) {
                consume(lexer, source, TokenTag::identifier);
                params.emplace_back(m_previous);
            }

            while (!at_eof()) {
                if (!m_current.match_tag_to(TokenTag::comma)) {
                    break;
                }

                consume_any(lexer, source); // pass ',' before another parameter name...
                consume(lexer, source, TokenTag::identifier);
                params.emplace_back(m_previous);
            }

            consume(lexer, source, TokenTag::right_paren);

            return std::make_unique<Stmt>(
                FunctionDecl {
                    .params = std::move(params),
                    .name = name_token,
                    .body = parse_block(lexer, source)
                },
                0,
                snippet_begin,
                m_current.start - snippet_begin + 1
            );
        }

        [[nodiscard]] auto parse_block(Lexer& lexer, const std::string& source) -> StmtPtr {
            const auto snippet_begin = m_current.start;
            std::vector<StmtPtr> stmts;

            consume(lexer, source, TokenTag::left_brace);

            while (!at_eof()) {
                stmts.emplace_back(parse_stmt(lexer, source));

                if (m_current.match_tag_to(TokenTag::right_brace)) {
                    consume_any(lexer, source);
                    break;
                }
            }

            return std::make_unique<Stmt>(
                Block {
                    .items = std::move(stmts)
                },
                0,
                snippet_begin,
                m_current.start - snippet_begin + 1
            );
        }

        [[nodiscard]] auto parse_expr_stmt(Lexer& lexer, const std::string& source) -> StmtPtr {
            m_syntax = SyntaxTag::stmt_expr;

            const auto snippet_begin = m_current.start;
            auto lhs_maybe_call = parse_call(lexer, source);

            if (!m_current.match_tag_to(TokenTag::symbol_assign)) {
                consume(lexer, source, TokenTag::semicolon);

                return std::make_unique<Stmt>(
                    ExprStmt {
                        .expr = std::move(lhs_maybe_call),
                    },
                    0,
                    snippet_begin,
                    m_current.start - snippet_begin + 1
                );
            }

            consume_any(lexer, source);

            auto rhs = parse_logical_or(lexer, source);

            consume(lexer, source, TokenTag::semicolon);

            return std::make_unique<Stmt>(
                ExprStmt {
                    .expr = std::make_unique<Expr>(
                        Assign {
                            .lhs = std::move(lhs_maybe_call),
                            .rhs = std::move(rhs)
                        }
                    )
                },
                0,
                snippet_begin,
                m_current.start - snippet_begin + 1
            );
        }

        /// NOTE: this reset method exists because the parser should be reused across multiple sources for efficiency's sake & supporting modular programs later.
        void reset(Lexer& lexer, const std::string& source) {
            m_previous = Token {};
            m_current = advance(lexer, source);
            m_error_count = 0;
            m_syntax = SyntaxTag::program_top;
        }

    public:
        Parser() noexcept
        : m_previous {}, m_current {}, m_error_count {0}, m_syntax {SyntaxTag::program_top} {}

        /// TODO: add source mapping IDs here when requires are added.
        [[nodiscard]] auto operator()(Lexer& lexer, std::string file_name, const std::string& source) -> std::optional<ASTUnit> {
            reset(lexer, source);

            std::vector<StmtPtr> translation_unit_stmts;

            try {
                while (!at_eof()) {
                    translation_unit_stmts.emplace_back(parse_stmt(lexer, source));
                }
            } catch (const std::runtime_error& parse_err) {
                std::println(std::cerr, "{}", parse_err.what());
            }

            if (m_error_count > 0) {
                return {};
            }

            ASTUnit translation_unit;

            for (auto& stmt_ptr : translation_unit_stmts) {
                translation_unit.emplace_back(ASTUnit::value_type {
                    .source_filename = file_name,
                    .decl = std::move(stmt_ptr),
                    .src_id = 0,
                });
            }

            return translation_unit;
        }
    };
}
