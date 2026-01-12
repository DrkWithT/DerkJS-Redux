module;

#include <string>
#include <optional>
#include <flat_map>
#include <vector>
#include <variant>
#include <algorithm>
#include <iostream>
#include <print>

export module frontend.semantics;

import frontend.lexicals;
import frontend.ast;

namespace DerkJS {
    enum class PrepassFlag : uint8_t {
        yes,
        no
    };

    enum class ValueCategory : uint8_t {
        js_temporary, // can be a literal (primitive) or value-yielding expression e.g `1 + 1`
        js_locator, // assignable value e.g `foo = 1;`, `foo.bar = 1;`
        js_unknown, // placeholder for entities with bad semantics
    };

    /// NOTE: tracks semantic information per name
    struct SemanticInfo {
        ValueCategory value_kind;
    };

    struct Scope {
        std::flat_map<std::string, SemanticInfo> entries;
        std::string name;
    };

    export class SemanticAnalyzer {
    private:
        std::vector<Scope> m_scopes; // tracks semantic information by dynamic scoping
        int m_line;
        int m_error_count; // counts errors for enumerating all diagnostics
        bool m_prepass; // whether the semantic checker is dealing with function 'hoisting' for processing top-level definitions

        void enter_scope(std::string name) {
            m_scopes.emplace_back(Scope {
                .entries = {},
                .name = std::move(name)
            });
        }

        void leave_scope() {
            if (m_scopes.size() > 1) {
                m_scopes.pop_back();
            }
        }

        [[nodiscard]] auto record_info_of(const std::string& name, SemanticInfo info) {
            m_scopes.back().entries.emplace(name, info);
        }

        [[nodiscard]] auto lookup_info(const std::string& name, int peek_scope_hops) -> std::optional<SemanticInfo> {
            if (auto target_scope_it = std::find_if(
                m_scopes.rbegin() + peek_scope_hops,
                m_scopes.rend(),
                [&name](const Scope& scope) -> bool { return scope.entries.contains(name); }
            ); target_scope_it != m_scopes.rend()) {
                return target_scope_it->entries.at(name);
            }

            return {};
        }

        void report_error(std::string_view source_name, int line, const std::string& current_source, int area_begin, int area_length, int bad_begin, int bad_length, std::string_view msg) {
            ++m_error_count;

            std::string pre_issue_area = current_source.substr(area_begin, bad_begin - area_begin);
            std::string issue_area = current_source.substr(bad_begin, bad_length);
            std::string post_issue_area = current_source.substr(bad_begin + bad_length, area_length - ((bad_begin - area_begin) + bad_length));

            std::println(std::cerr, 
                "\x1b[1;31mSemantic Error\x1b[0m {} at \x1b[1;33m{}:{}:\x1b[0m\n\nnote: {}\n\n{}\x1b[1;31m{}\x1b[0m{}\n",
                m_error_count,
                source_name,
                line,
                msg,
                pre_issue_area,
                issue_area,
                post_issue_area
            );
        }

        [[nodiscard]] auto check_primitive(const Primitive& primitive, const std::string& current_source, [[maybe_unused]] int area_start, [[maybe_unused]] int area_length) -> std::optional<SemanticInfo> {
            const auto& [token_tag, token_start, token_length, token_line, token_column] = primitive.token;

            switch (token_tag) {
            case TokenTag::keyword_undefined:
            case TokenTag::keyword_null:
            case TokenTag::keyword_true:
            case TokenTag::keyword_false:
            case TokenTag::literal_int:
            case TokenTag::literal_real:
            case TokenTag::literal_string:
                return SemanticInfo {ValueCategory::js_temporary};
            case TokenTag::identifier:
                return lookup_info(current_source.substr(token_start, token_length), 0);
            default:
                return {};
            }
        }

        [[nodiscard]] auto check_object(const ObjectLiteral& object, std::string_view source_name, const std::string& current_source, [[maybe_unused]] int area_start, [[maybe_unused]] int area_length) -> std::optional<SemanticInfo> {
            // NOTE: object literals can have similarly named fields vs. outside variables, so the depths of scope lookup MUST be differing: LHS is in the object's semantic scope BUT the RHS is in the object's parent scope... Until I deal with `this` ;)
            enter_scope("anonymous-object");

            for (const auto& [field_name, field_expr] : object.fields) {
                std::string field_name_lexeme = field_name.as_string(current_source);
                const auto field_name_start = field_name.start;
                const auto field_name_length = field_name.length;
                const auto field_name_line = field_name.line;

                if (lookup_info(field_name_lexeme, 0).has_value()) {
                    report_error(source_name, field_name_line, current_source, area_start, area_length, field_name_start, field_name_length, "Cannot redeclare property within object.");
                    leave_scope();
                    return {};
                }

                /// TODO: support custom scoped-peek-offsets for check_expr... RHS needs to check 1 scope lower??
                if (auto field_rhs_info = check_expr(*field_expr, source_name, current_source, area_start, area_length); !field_rhs_info) {
                    leave_scope();
                    return {};
                } else {   
                    record_info_of(field_name_lexeme, *field_rhs_info);
                }
            }

            leave_scope();

            return SemanticInfo {
                .value_kind = ValueCategory::js_temporary
            };
        }

        [[nodiscard]] auto check_member_access([[maybe_unused]] const MemberAccess& member_access, [[maybe_unused]] std::string_view source_name, [[maybe_unused]] const std::string& current_source, [[maybe_unused]] int area_start, [[maybe_unused]] int area_length) -> std::optional<SemanticInfo> {
            return SemanticInfo {
                .value_kind = ValueCategory::js_locator
            };
        }

        [[nodiscard]] auto check_unary(const Unary& unary, std::string_view source_name, const std::string& current_source, int area_start, int area_length) -> std::optional<SemanticInfo> {
            if (auto inner_info = check_expr(*unary.inner, source_name, current_source, area_start, area_length); inner_info) {
                return inner_info;
            } else {   
                report_error(source_name, m_line, current_source, area_start, area_length, unary.inner->text_begin, unary.inner->text_length, "Invalid unary operand, possibly undeclared.");

                return {};
            }
        }

        [[nodiscard]] auto check_binary(const Binary& binary, std::string_view source_name, const std::string& current_source, int area_start, int area_length) -> std::optional<SemanticInfo> {
            const auto& [expr_lhs, expr_rhs, expr_op] = binary;

            /// NOTE: JS object properties are highly dynamic, so don't check their accesses until runtime!
            if (expr_op == AstOp::ast_op_dot_access || expr_op == AstOp::ast_op_index_access) {
                return SemanticInfo {
                    .value_kind = ValueCategory::js_locator
                };
            }

            if (
                auto lhs_info = check_expr(*expr_lhs, source_name, current_source, area_start, area_length),
                rhs_info = check_expr(*expr_rhs, source_name, current_source, area_start, area_length);
                lhs_info && rhs_info
            ) {
                return SemanticInfo {
                    .value_kind = ValueCategory::js_temporary
                };
            } else if (!lhs_info) {
                report_error(
                    source_name,
                    m_line,
                    current_source,
                    area_start,
                    area_length,
                    binary.lhs->text_begin,
                    binary.lhs->text_length,
                    "Invalid LHS of binary expression, something may be undeclared."
                );

                return {};
            } else {
                report_error(
                    source_name,
                    m_line,
                    current_source,
                    area_start,
                    area_length,
                    binary.rhs->text_begin,
                    binary.rhs->text_length,
                    "Invalid RHS of binary expression, something may be undeclared."
                );

                return {};
            }
        }

        [[nodiscard]] auto check_assign(const Assign& assign, std::string_view source_name, const std::string& current_source, int area_start, int area_length) -> std::optional<SemanticInfo> {
            if (
                auto [lhs_value_group] = check_expr(*assign.lhs, source_name, current_source, area_start, area_length).value_or(SemanticInfo {
                    .value_kind = ValueCategory::js_unknown
                });
                lhs_value_group != ValueCategory::js_locator
            ) {
                report_error(
                    source_name,
                    m_line,
                    current_source,
                    area_start,
                    area_length,
                    assign.lhs->text_begin,
                    assign.lhs->text_length,
                    "Invalid assignment LHS, anonymous or non-property entities are not valid destinations."
                );

                return {};
            } else {
                if (auto rhs_info = check_expr(*assign.rhs, source_name, current_source, area_start, area_length); !rhs_info) {
                    report_error(
                        source_name,
                        m_line,
                        current_source,
                        area_start,
                        area_length,
                        assign.rhs->text_begin,
                        assign.rhs->text_length,
                        "Invalid assignment RHS, something was probably undeclared."
                    );

                    return {};
                }

                return SemanticInfo {
                    .value_kind = ValueCategory::js_temporary
                };
            }
        }

        [[nodiscard]] auto check_call(const Call& call, std::string_view source_name, const std::string& current_source, int area_start, int area_length) -> std::optional<SemanticInfo> {
            if (auto callee_info = check_expr(*call.callee, source_name, current_source, area_start, area_length); callee_info) {   
                return SemanticInfo {
                    .value_kind = ValueCategory::js_temporary
                };
            }

            report_error(
                source_name,
                m_line,
                current_source,
                area_start,
                area_length,
                call.callee->text_begin,
                call.callee->text_length,
                "Invalid callee expression, may be a (primitive) temporary. Primitives cannot be called."
            );

            return {};
        }

        [[nodiscard]] auto check_expr(const Expr& expr, std::string_view source_name, const std::string& current_source, int area_start, int area_length) -> std::optional<SemanticInfo> {
            const auto expr_text_begin = expr.text_begin;
            const auto expr_text_length = expr.text_length;
            if (auto primitive_p = std::get_if<Primitive>(&expr.data); primitive_p) {
                return check_primitive(*primitive_p, current_source, expr_text_begin, expr_text_length);
            } else if (auto object_p = std::get_if<ObjectLiteral>(&expr.data); object_p) {
                return check_object(*object_p, source_name, current_source, area_start, area_length);
            } else if (auto member_access_p = std::get_if<MemberAccess>(&expr.data); member_access_p) {
                return check_member_access(*member_access_p, source_name, current_source, area_start, area_length);
            } else if (auto unary_p = std::get_if<Unary>(&expr.data); unary_p) {
                return check_unary(*unary_p, source_name, current_source, expr_text_begin, expr_text_length);
            } else if (auto binary_p = std::get_if<Binary>(&expr.data); binary_p) {
                return check_binary(*binary_p, source_name, current_source, expr_text_begin, expr_text_length);
            } else if (auto assign_p = std::get_if<Assign>(&expr.data); assign_p) {
                return check_assign(*assign_p, source_name, current_source, expr_text_begin, expr_text_length);
            } else if (auto call_p = std::get_if<Call>(&expr.data); call_p) {
                return check_call(*call_p, source_name, current_source, expr_text_begin, expr_text_length);
            }

            return {};
        }

        [[nodiscard]] auto check_expr_stmt(const ExprStmt& expr_stmt, std::string_view source_name, const std::string& current_source, int area_start, int area_length) -> bool {
            if (m_prepass) {
                return true;
            }

            return check_expr(*expr_stmt.expr, source_name, current_source, expr_stmt.expr->text_begin, expr_stmt.expr->text_length).has_value();
        }

        [[nodiscard]] auto check_variables(const Variables& variables, std::string_view source_name, const std::string& current_source, int area_start, int area_length) -> bool {
            if (m_prepass) {
                return true;
            }

            for (const auto& [var_name, var_init_expr] : variables.vars) {
                std::string var_name_lexeme = current_source.substr(var_name.start, var_name.length);

                record_info_of(var_name_lexeme, SemanticInfo {
                    .value_kind = ValueCategory::js_locator
                });

                if (!check_expr(*var_init_expr, source_name, current_source, area_start, area_length)) {
                    report_error(
                        source_name,
                        m_line,
                        current_source,
                        area_start,
                        area_length,
                        var_init_expr->text_begin,
                        var_init_expr->text_length,
                        "Invalid named declaration in variables statement."
                    );

                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] auto check_if(const If& stmt_if, std::string_view source_name, const std::string& current_source, int area_start, int area_length) -> bool {
            if (m_prepass) {
                return true;
            }

            if (!check_expr(*stmt_if.check, source_name, current_source, area_start, area_length)) {
                return false;
            }

            if (!check_stmt(*stmt_if.body_true, source_name, current_source, area_start, area_length)) {
                return false;
            }

            if (const auto stmt_if_falsy_part_p = stmt_if.body_false.get(); stmt_if_falsy_part_p) {
                return check_stmt(*stmt_if_falsy_part_p, source_name, current_source, area_start, area_length);
            }

            return true;
        }

        [[nodiscard]] auto check_return(const Return& ret, std::string_view source_name, const std::string& current_source, int area_start, int area_length) -> bool {
            if (m_prepass) {
                return true;
            }

            return check_expr(*ret.result, source_name, current_source, area_start, area_length).has_value();
        }

        [[nodiscard]] auto check_while(const While& loop_by_while, std::string_view source_name, const std::string& current_source, int area_start, int area_length) -> bool {
            const auto& [loop_check, loop_body] = loop_by_while;

            if (!check_expr(*loop_check, source_name, current_source, area_start, area_length)) {
                return false;
            }

            if (!check_stmt(*loop_body, source_name, current_source, area_start, area_length)) {
                return false;
            }

            return true;
        }

        [[nodiscard]] auto check_block(const Block& block, std::string_view source_name, const std::string& current_source, int area_start, int area_length) -> bool {
            if (m_prepass) {
                return true;
            }

            for (const auto& stmt : block.items) {
                if (!check_stmt(*stmt, source_name, current_source, stmt->text_begin, stmt->text_length)) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] auto check_function_decl(const FunctionDecl& func, std::string_view source_name, const std::string& current_source, int area_start, int area_length) -> bool {
            std::string func_name {current_source.substr(func.name.start, func.name.length)};

            if (m_prepass) {
                record_info_of(func_name, SemanticInfo {
                    .value_kind = ValueCategory::js_locator
                });
            } else {
                enter_scope(func_name);

                for (const auto& param_token : func.params) {
                    std::string param_name = current_source.substr(param_token.start, param_token.length);

                    record_info_of(param_name, SemanticInfo {
                        .value_kind = ValueCategory::js_locator
                    });
                }

                if (!check_stmt(*func.body, source_name, current_source, func.body->text_begin, func.body->text_length)) {
                    leave_scope();

                    return false;
                }

                leave_scope();
            }

            return true;
        }

        [[nodiscard]] auto check_stmt(const Stmt& stmt, std::string_view source_name, const std::string& current_source, int area_start, int area_length) -> bool {
            const auto stmt_text_begin = stmt.text_begin;
            const auto stmt_text_length = stmt.text_length;

            if (auto expr_stmt_p = std::get_if<ExprStmt>(&stmt.data); expr_stmt_p) {
                return check_expr_stmt(*expr_stmt_p, source_name, current_source, stmt_text_begin, stmt_text_length);
            } else if (auto variables_p = std::get_if<Variables>(&stmt.data); variables_p) {
                return check_variables(*variables_p, source_name, current_source, stmt_text_begin, stmt_text_length);
            } else if (auto stmt_if_p = std::get_if<If>(&stmt.data); stmt_if_p) {
                return check_if(*stmt_if_p, source_name, current_source, stmt_text_begin, stmt_text_length);
            } else if (auto ret_p = std::get_if<Return>(&stmt.data); ret_p) {
                return check_return(*ret_p, source_name, current_source, stmt_text_begin, stmt_text_length);
            } else if (auto loop_by_while_p = std::get_if<While>(&stmt.data); loop_by_while_p) {
                return check_while(*loop_by_while_p, source_name, current_source, stmt_text_begin, stmt_text_length);
            } else if (auto block_p = std::get_if<Block>(&stmt.data); block_p) {
                return check_block(*block_p, source_name, current_source, stmt_text_begin, stmt_text_length);
            } else if (auto func_p = std::get_if<FunctionDecl>(&stmt.data); func_p) {
                return check_function_decl(*func_p, source_name, current_source, stmt_text_begin, stmt_text_length);
            } 

            return false;
        }

        [[maybe_unused]] auto check_top_level(PrepassFlag should_prepass, const ASTUnit& ast, const std::vector<std::string>& source_map) -> bool {
            m_prepass = should_prepass == PrepassFlag::yes;

            for (const auto& [source_filename, ast, src_id] : ast) {
                if (!check_stmt(*ast, source_filename, source_map.at(src_id), 0, source_map.at(src_id).length())) {
                    return false;
                }
            }

            return true;
        }

    public:
        SemanticAnalyzer()
        : m_scopes {}, m_line {1}, m_error_count {}, m_prepass {true} {
            enter_scope("this");
        }

        [[nodiscard]] auto operator()(const ASTUnit& ast, const std::vector<std::string>& source_map) -> bool {
            check_top_level(PrepassFlag::yes, ast, source_map);

            return check_top_level(PrepassFlag::no, ast, source_map);
        }
    };
}
