module;

#include <utility>
#include <optional>
#include <array>
#include <vector>
#include <variant>
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
            context.m_callee_name = var_name;

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
}
