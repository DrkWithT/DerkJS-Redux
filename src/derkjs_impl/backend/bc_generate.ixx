module;

#include <type_traits>
#include <limits>
#include <utility>

#include <optional>
#include <string>
#include <vector>
#include <variant>
#include <flat_map>
#include <iostream>
#include <print>

export module backend.bc_generate;

import frontend.lexicals;
import frontend.ast;
import runtime.objects;
import runtime.value;
import runtime.strings;
import runtime.bytecode;

export namespace DerkJS {
    struct PreloadItem {
        std::string lexeme;
        Value value;
        Location location;
    };

    class BytecodeGenPass {
    private:
        static constexpr int min_bc_jump_offset = std::numeric_limits<int16_t>::min();
        static constexpr int max_bc_jump_offset = std::numeric_limits<int16_t>::max();

        static constexpr Arg dud_arg {
            .n = 0,
            .tag = Location::immediate
        };

        // TODO: make helper methods for mapping names, constants, heap objects, etc. to locations... See `codegen.md`!!

        /// NOTE: This overload is for no-argument opcode instructions e.g NOP
        void encode_instruction(Opcode op) {
            m_code.emplace_back(Instruction {
                .args = { 0, 0 },
                .op = op
            });
        }

        void encode_instruction(Opcode op, Arg a0) {
            m_code.emplace_back(Instruction {
                .args = { a0.n, 0 },
                .op = op
            });
        }

        void encode_instruction(Opcode op, Arg a0, Arg a1) {
            m_code.emplace_back(Instruction {
                .args = { a0.n, a1.n},
                .op = op
            });
        }

        [[maybe_unused]] auto update_temp_id(int delta) -> int {
            const auto next_temp_id = m_next_temp_id;

            m_next_temp_id += delta;

            return next_temp_id;
        }

        [[nodiscard]] auto current_temp_id() const noexcept -> int {
            return m_next_temp_id - 1;
        }

        [[nodiscard]] auto emit_primitive(const Primitive& expr, const std::string& source) -> std::optional<Arg>;

        [[nodiscard]] auto emit_object_literal(const ObjectLiteral& object, const std::string& source) -> std::optional<Arg>;

        [[nodiscard]] auto emit_unary(const Unary& expr, const std::string& source) -> std::optional<Arg>;

        [[nodiscard]] auto emit_logical_expr(AstOp logical_operator, const Expr& lhs, const Expr& rhs, const std::string& source) -> std::optional<Arg>;

        [[nodiscard]] auto emit_member_access(const MemberAccess& member_access, const std::string& source) -> std::optional<Arg>;

        [[nodiscard]] auto emit_binary(const Binary& expr, const std::string& source) -> std::optional<Arg>;

        [[nodiscard]] auto emit_assign(const Assign& expr, const std::string& source) -> std::optional<Arg>;

        [[nodiscard]] auto emit_call(const Call& expr, const std::string& source) -> std::optional<Arg>;

        [[nodiscard]] auto emit_expr(const Expr& expr, const std::string& source) -> std::optional<Arg> {
            if (auto primitive_p = std::get_if<Primitive>(&expr.data); primitive_p) {
                return emit_primitive(*primitive_p, source);
            } else if (auto object_p = std::get_if<ObjectLiteral>(&expr.data); object_p) {
                /// TODO: maybe add opcode support for object cloning... could be good for instances of prototypes.
                return emit_object_literal(*object_p, source);
            } else if (auto member_access_p = std::get_if<MemberAccess>(&expr.data); member_access_p) {
                return emit_member_access(*member_access_p, source);
            } else if (auto unary_p = std::get_if<Unary>(&expr.data); unary_p) {
                return emit_unary(*unary_p, source);
            } else if (auto binary_p = std::get_if<Binary>(&expr.data); binary_p) {
                return emit_binary(*binary_p, source);
            } else if (auto assign_p = std::get_if<Assign>(&expr.data); assign_p) {
                return emit_assign(*assign_p, source);
            } else if (auto call_p = std::get_if<Call>(&expr.data); call_p) {
                return emit_call(*call_p, source);
            }

            return {};
        }

        [[nodiscard]] auto emit_expr_stmt(const ExprStmt& stmt, const std::string& source) -> bool;

        [[nodiscard]] auto emit_var_decl(const VarDecl& stmt, const std::string& source) -> bool;

        [[nodiscard]] auto emit_variables(const Variables& stmt, const std::string& source) -> bool {
            for (const auto& sub_var_decl : stmt.vars) {
                if (!emit_var_decl(sub_var_decl, source)) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] auto emit_if(const If& stmt_if, const std::string& source) -> bool;

        [[nodiscard]] auto emit_return(const Return& stmt, const std::string& source) -> bool;

        [[nodiscard]] auto emit_while(const While& loop_by_while, const std::string& source) -> bool;

        [[nodiscard]] auto emit_block(const Block& stmt, const std::string& source) -> bool {
            for (const auto& temp_stmt : stmt.items) {
                if (!emit_stmt(*temp_stmt, source)) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] auto emit_function_decl(const FunctionDecl& stmt, const std::string& source) -> bool;

        [[nodiscard]] auto emit_stmt(const Stmt& stmt, const std::string& source) -> bool {
            if (auto expr_stmt_p = std::get_if<ExprStmt>(&stmt.data); expr_stmt_p) {
                return emit_expr_stmt(*expr_stmt_p, source);
            } else if (auto variables_p = std::get_if<Variables>(&stmt.data); variables_p) {
                return emit_variables(*variables_p, source);
            } else if (auto stmt_if_p = std::get_if<If>(&stmt.data); stmt_if_p) {
                return emit_if(*stmt_if_p, source);
            } else if (auto ret_p = std::get_if<Return>(&stmt.data); ret_p) {
                return emit_return(*ret_p, source);
            } else if (auto loop_by_while_p = std::get_if<While>(&stmt.data); loop_by_while_p) {
                return emit_while(*loop_by_while_p, source);
            } else if (auto block_p = std::get_if<Block>(&stmt.data); block_p) {
                return emit_block(*block_p, source);
            } else if (auto func_p = std::get_if<FunctionDecl>(&stmt.data); func_p) {
                return emit_function_decl(*func_p, source);
            }

            return false;
        }

    public:
        BytecodeGenPass(PolyPool<ObjectBase<Value>> heap_items_)
        : {
            // todo: implement...
        }

        [[nodiscard]] auto operator()([[maybe_unused]] const ASTUnit& tu, [[maybe_unused]] const std::vector<std::string>& source_map) -> std::optional<Program> {
            /// TODO: 1. emit all function decls ONLY as per JS function hoisting.
            for (const auto& [src_filename, decl, src_id] : tu) {
                if (std::holds_alternative<FunctionDecl>(decl->data)) {
                    if (!emit_stmt(*decl, source_map.at(src_id))) {
                        std::println(std::cerr, "Compile Error at source '{}' around '{}': unsupported JS construct :(\n", src_filename, source_map.at(src_id).substr(decl->text_begin, decl->text_length / 2));
                        return {};
                    }
                }
            }

            auto global_func_chunk = record_this_func();

            /// TODO: 2. emit all top-level non-function statements as an implicit function that's called right away.
            for (const auto& [src_filename, decl, src_id] : tu) {
                if (!std::holds_alternative<FunctionDecl>(decl->data)) {
                    if (!emit_stmt(*decl, source_map.at(src_id))) {
                        std::println(std::cerr, "Compile Error at source '{}' around '{}': unsupported JS construct :(\n", src_filename, source_map.at(src_id).substr(decl->text_begin, decl->text_length / 2));
                        return {};
                    }
                }
            }

            /// NOTE: Place dud offset marker for bytecode dumping to stop properly.
            m_func_offsets.emplace_back(-1);

            return Program {
                .heap_items = std::move(m_heap_items), // PolyPool<ObjectBase<Value>>
                .consts = std::move(m_consts), // std::vector<Value>
                .code = std::move(m_code), // std::vector<Instruction>
                .offsets = std::move(m_func_offsets), // std::vector<int>
                .entry_func_id = global_func_chunk.n, // int
            };
        }
    };
}
