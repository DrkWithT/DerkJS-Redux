module;

#include <utility>
#include <optional>
#include <variant>
#include <array>
#include <vector>
#include <forward_list>
#include <flat_map>
#include <string>

export module backend.stmt_gen;

import backend.bc_generate;

namespace DerkJS::Backend {
    export class ExprStmtEmitter : public StmtEmitterBase<Stmt> {
    public:
        ExprStmtEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Stmt& node, const std::string& source) -> bool override {
            if (context.m_prepass_vars) {
                return true;
            }

            const auto& [wrapped_expr] = std::get<ExprStmt>(node.data);

            return context.emit_expr(*wrapped_expr, source);
        }
    };

    export class VariablesEmitter : public StmtEmitterBase<Stmt> {
    private:
        [[nodiscard]] auto emit_var_decl(BytecodeEmitterContext& context, const VarDecl& stmt, const std::string& source) -> bool {
            const auto& [var_name_token, var_init_expr] = stmt;
            std::string var_name = var_name_token.as_string(source);

            if (var_init_expr->tag == ExprNodeTag::lambda_literal) {
                context.m_callee_name = var_name; // handle possibly self-recursing callee here...
            } else {
                context.m_callee_name.clear();
            }

            auto var_local_slot = context.record_symbol(var_name, RecordLocalOpt {}, FindLocalsOpt {});
            
            // 1. When hoisting the var declaration, just set it to undefined 1st.
            if (const auto placeholder_undefined = context.lookup_symbol("undefined", FindGlobalConstsOpt {}).value(); context.m_prepass_vars) {
                context.encode_instruction(Opcode::djs_put_const, placeholder_undefined);
                context.m_local_maps.back().locals[var_name] = *var_local_slot;
                return true;
            }

            // 2. In the 2nd pass, define the filler var in function scope. Although this is a var decl, it's been defined as undefined before, so treat the initialization as a var-assignment.
            context.encode_instruction(Opcode::djs_ref_local, *var_local_slot);

            if (!context.emit_expr(*var_init_expr, source)) {
                return false;
            }

            // 2b. Capture variable in environment in case its used as a closure.
            context.encode_instruction(Opcode::djs_put_const, context.record_symbol(var_name, var_name, FindKeyConstOpt {}).value()); // variable name is the capturing Object key!
            context.encode_instruction(Opcode::djs_store_upval);
            context.m_member_depth = 0;
            context.encode_instruction(Opcode::djs_emplace);
            context.m_callee_name.clear();

            return true;
        }
    public:
        VariablesEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Stmt& node, const std::string& source) -> bool override {
            const auto& [vars] = std::get<Variables>(node.data);

            for (const auto& sub_var_decl : vars) {
                if (!emit_var_decl(context, sub_var_decl, source)) {
                    return false;
                }
            }

            return true;
        }
    };

    export class IfEmitter : public StmtEmitterBase<Stmt> {
    public:
        IfEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Stmt& node, const std::string& source) -> bool override {
            const auto& [check, truthy_body, falsy_body] = std::get<If>(node.data);

            /// NOTE: emit all var decls in these bodies
            if (context.m_prepass_vars) {
                if (!context.emit_stmt(*truthy_body, source)) {
                    return false;
                }

                if (falsy_body && !context.emit_stmt(*falsy_body, source)) {
                    return false;
                }

                return true;
            }

            if (!context.emit_expr(*check, source)) {
                return false;
            }

            const int pre_if_jump_pos = context.m_code_blobs.front().size();
            context.encode_instruction(
                Opcode::djs_jump_else,
                /// NOTE: backpatch this jump afterward.
                Arg {.n = -1, .tag = Location::immediate},
                /// NOTE: pop off that one collapsed slot with the condition's temporary
                Arg {.n = 1, .tag = Location::immediate}
            );

            if (!context.emit_stmt(*truthy_body, source)) {
                return false;
            }

            if (!falsy_body) {
                int post_lone_if_jump_pos = context.m_code_blobs.front().size();
                context.encode_instruction(Opcode::djs_nop, {});

                context.m_code_blobs.front().at(pre_if_jump_pos).args[0] = post_lone_if_jump_pos - pre_if_jump_pos;

                return true;
            }

            int skip_else_jump_pos = context.m_code_blobs.front().size();
            context.encode_instruction(Opcode::djs_jump, Arg {.n = -1, .tag = Location::immediate});

            int to_else_jump_pos = context.m_code_blobs.front().size();
            context.m_code_blobs.front().at(pre_if_jump_pos).args[0] = to_else_jump_pos - pre_if_jump_pos;

            if (!context.emit_stmt(*falsy_body, source)) {
                return false;
            }

            const int post_else_jump_pos = context.m_code_blobs.front().size();
            context.m_code_blobs.front().at(skip_else_jump_pos).args[0] = post_else_jump_pos - skip_else_jump_pos;

            return true;
        }
    };

    export class ReturnEmitter : public StmtEmitterBase<Stmt> {
    public:
        ReturnEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Stmt& node, const std::string& source) -> bool override {
            if (context.m_prepass_vars) {
                return true;
            }

            const auto& [result_expr] = std::get<Return>(node.data);

            context.m_access_as_lval = false;

            if (!context.emit_expr(*result_expr, source)) {
                return false;
            }

            context.encode_instruction(Opcode::djs_ret);

            return true;
        }
    };

    export class WhileEmitter : public StmtEmitterBase<Stmt> {
    public:
        WhileEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Stmt& node, const std::string& source) -> bool override {
            const auto& [loop_check, loop_body] = std::get<While>(node.data);

            if (context.m_prepass_vars) {
                if (!context.emit_stmt(*loop_body, source)) {
                    return false;
                }

                return true;
            }

            const int loop_begin_pos = context.m_code_blobs.front().size();

            context.m_active_loops.push_front(ActiveLoop {
                .exits = {},
                .repeats = {}
            });

            if (!context.emit_expr(*loop_check, source)) {
                return false;
            }

            const int loop_exit_jump_pos = context.m_code_blobs.front().size();
            context.encode_instruction(Opcode::djs_jump_else, Arg {.n = -1, .tag = Location::immediate}, Arg {.n = 1, .tag = Location::immediate});

            if (!context.emit_stmt(*loop_body, source)) {
                return false;
            }

            // 2. Patch main loop jumps!
            const int loop_repeat_jump_pos = context.m_code_blobs.front().size();
            context.encode_instruction(Opcode::djs_jump, Arg {.n = -1, .tag = Location::immediate});
            const int loop_exit_pos = loop_repeat_jump_pos + 1;

            context.m_code_blobs.front().at(loop_exit_jump_pos).args[0] = 1 + loop_repeat_jump_pos - loop_exit_jump_pos;
            context.m_code_blobs.front().at(loop_repeat_jump_pos).args[0] = loop_begin_pos - loop_repeat_jump_pos;

            // 3. Patch breaks, continues...
            const auto& [wloop_exits, wloop_repeats] = context.m_active_loops.front();

            for (auto wloop_break_pos : wloop_exits) {
                context.m_code_blobs.front().at(wloop_break_pos).args[0] = loop_exit_pos - wloop_break_pos;
            }

            for (auto wloop_repeat_pos : wloop_repeats) {
                context.m_code_blobs.front().at(wloop_repeat_pos).args[0] = loop_begin_pos - wloop_repeat_pos;
            }

            context.m_active_loops.pop_front();

            return true;
        }
    };

    export class ForSteppedEmitter : public StmtEmitterBase<Stmt> { 
    public:
        ForSteppedEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, [[maybe_unused]] const Stmt& node, [[maybe_unused]] const std::string& source) -> bool override {
            const auto& [init_part, check_expr, update_expr, loop_stmt] = std::get<ForStepped>(node.data);

            if (std::holds_alternative<StmtPtr>(init_part)) {
                if (!context.emit_stmt(*std::get<StmtPtr>(init_part), source)) {
                    return false;
                }
            } else if (!context.m_prepass_vars && std::holds_alternative<ExprPtr>(init_part)) {
                if (!context.emit_expr(*std::get<ExprPtr>(init_part), source)) {
                    return false;
                }
            }

            if (context.m_prepass_vars) {
                return context.emit_stmt(*loop_stmt, source);
            }

            // 1. Begin for-step loop emission here, ensuring that its active jump positions are tracked. Any omitted constructs in its (init; condition; update) portion will become NOPs.
            context.m_active_loops.push_front(ActiveLoop {
                .exits = {},
                .repeats = {}
            });

            const int begin_checked_loop_pos = context.m_code_blobs.front().size();

            if (check_expr) {
                if (!context.emit_expr(*check_expr, source)) {
                    return false;
                }
            } else {
                context.encode_instruction(Opcode::djs_nop);
            }

            const int exit_loop_jmp_pos = context.m_code_blobs.front().size();
            context.encode_instruction(Opcode::djs_jump_else);

            if (loop_stmt) {
                if (!context.emit_stmt(*loop_stmt, source)) {
                    return false;
                }
            } else {
                context.encode_instruction(Opcode::djs_nop);
            }

            const int update_loop_pos = context.m_code_blobs.front().size();
            if (update_expr) {
                if (!context.emit_expr(*update_expr, source)) {
                    return false;
                }
            } else {
                context.encode_instruction(Opcode::djs_nop);
            }

            const int repeat_loop_jmp_pos = context.m_code_blobs.front().size();
            context.encode_instruction(Opcode::djs_jump);

            const int end_loop_pos = context.m_code_blobs.front().size();
            context.encode_instruction(Opcode::djs_nop);

            // 2. Backpatch for-loop break, continue, and normal jumps here!
            const auto& [w_loop_exits, w_loop_repeats] = context.m_active_loops.front();

            for (auto loop_break_pos : w_loop_exits) {
                context.m_code_blobs.front().at(loop_break_pos).args[0] = end_loop_pos - loop_break_pos;
            }

            for (auto loop_continue_pos : w_loop_repeats) {
                context.m_code_blobs.front().at(loop_continue_pos).args[0] = update_loop_pos - loop_continue_pos;
            }

            context.m_code_blobs.front().at(repeat_loop_jmp_pos).args[0] = begin_checked_loop_pos - repeat_loop_jmp_pos;
            context.m_code_blobs.front().at(exit_loop_jmp_pos).args[0] = end_loop_pos - exit_loop_jmp_pos;

            context.m_active_loops.pop_front();

            return true;
        }
    };

    export class BreakEmitter : public StmtEmitterBase<Stmt> {
    public:
        BreakEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, [[maybe_unused]] const Stmt& node, [[maybe_unused]] const std::string& source) -> bool override {
            if (context.m_prepass_vars) {
                return true;
            }

            const int current_code_pos = context.m_code_blobs.front().size();

            context.encode_instruction(Opcode::djs_jump, Arg {
                .n = -1,
                .tag = Location::immediate
            });
            context.m_active_loops.front().exits.push_back(current_code_pos);

            return true;
        }
    };

    export class ContinueEmitter : public StmtEmitterBase<Stmt> {
    public:
        ContinueEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Stmt& node, const std::string& source) -> bool override {
            if (context.m_prepass_vars) {
                return true;
            }

            const int current_code_pos = context.m_code_blobs.front().size();

            context.encode_instruction(Opcode::djs_jump, Arg {
                .n = -1,
                .tag = Location::immediate
            });
            context.m_active_loops.front().repeats.push_back(current_code_pos);

            return true;
        }
    };

    export class BlockEmitter : public StmtEmitterBase<Stmt> {
    public:
        BlockEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Stmt& node, const std::string& source) -> bool override {
            const auto& [items] = std::get<Block>(node.data);

            context.m_local_maps.back().block_level++;

            for (const auto& temp_stmt : items) {
                if (!context.emit_stmt(*temp_stmt, source)) {
                    return false;
                }
            }

            context.m_local_maps.back().block_level--;

            // Put implicit return at top-level lambda body. If there's a previous return, that will avoid this one.
            if (!context.m_prepass_vars && context.m_in_callable && context.m_local_maps.back().block_level == 0) {
                context.encode_instruction(Opcode::djs_ret, Arg {
                    .n = 1, // is-implicit-return flag -> a0
                    .tag = Location::immediate,
                    .is_str_literal = false,
                    .from_closure = false
                });
            }

            return true;
        }
    };

    export class ThrowEmitter : public StmtEmitterBase<Stmt> {
    public:
        ThrowEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Stmt& node, const std::string& source) -> bool override {
            if (context.m_prepass_vars) {
                return true;
            }

            const auto& [error_data_expr] = std::get<Throw>(node.data);

            if (!context.emit_expr(*error_data_expr, source)) {
                return false;
            }

            context.encode_instruction(Opcode::djs_throw, Arg {
                .n = static_cast<int16_t>(context.m_in_try_block),
                .tag = Location::immediate,
                .is_str_literal = false,
                .from_closure = false
            });

            return true;
        }
    };

    export class TryCatchEmitter : public StmtEmitterBase<Stmt> {
    public:
        TryCatchEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Stmt& node, const std::string& source) -> bool override {
            const auto& [error_name_token, block_try, block_catch] = std::get<TryCatch>(node.data);
            std::string error_name_prepassed = error_name_token.as_string(source);

            if (context.m_prepass_vars) {
                auto error_name_slot = context.record_symbol(error_name_prepassed, RecordLocalOpt {}, FindLocalsOpt {});

                // 1. When hoisting the var declaration, just set it to undefined 1st.
                if (const auto placeholder_undefined_loc = context.lookup_symbol("undefined", FindGlobalConstsOpt {}); placeholder_undefined_loc) {
                    context.encode_instruction(Opcode::djs_put_const, *placeholder_undefined_loc);
                    context.m_local_maps.back().locals[error_name_prepassed] = *error_name_slot;
                    return true;
                }

                return false;
            }

            context.m_in_try_block = true;

            if (!context.emit_stmt(*block_try, source)) {
                return false;
            }

            context.m_in_try_block = false;

            const int skip_catch_jump_pos = context.m_code_blobs.front().size();
            context.encode_instruction(Opcode::djs_jump);

            // 3. Emit catch block (error name 1st)...
            context.m_local_maps.back().locals[error_name_prepassed] = Arg {
                .n = 0,
                .tag = Location::error_var,
                .is_str_literal = false,
                .from_closure = false
            };
            context.encode_instruction(Opcode::djs_catch);

            if (!context.emit_stmt(*block_catch, source)) {
                return false;
            }

            const int end_catch_pos = context.m_code_blobs.front().size();
            context.encode_instruction(Opcode::djs_nop);

            // 4. backpatch try's jump over the catch for non-errorneous runs.
            context.m_code_blobs.front().at(skip_catch_jump_pos).args[0] = end_catch_pos - skip_catch_jump_pos;

            return true;
        }
    };
}
