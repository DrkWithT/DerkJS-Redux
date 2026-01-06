module;

#include <utility>
#include <limits>

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
import runtime.value;
import runtime.bytecode;

export namespace DerkJS {
    struct GlobalFuncOpt {};

    class BytecodeGenPass {
    private:
        static constexpr int min_bc_jump_offset = std::numeric_limits<int16_t>::min();
        static constexpr int max_bc_jump_offset = std::numeric_limits<int16_t>::max();

        static constexpr Arg dud_arg {
            .n = 0,
            .tag = Location::immediate,
        };

        struct SimStackFrame {
            std::flat_map<std::string, Arg> entries;
            int stack_base;
        };

        std::vector<SimStackFrame> m_mappings;
        std::vector<Object<Value>> m_heap_items;
        std::vector<Value> m_consts;
        std::vector<Instruction> m_code;
        std::vector<int> m_func_offsets;

        int m_next_temp_id;
        // bool is_op_emplaced;
        // bool is_temp_poppable;

        void enter_simulated_frame() {
            m_mappings.emplace_back(SimStackFrame {
                .entries = {},
                .stack_base = m_next_temp_id - 1
            });
        }

        void leave_simulated_frame() {
            m_next_temp_id = m_mappings.back().stack_base + 1;
            m_mappings.pop_back();
        }

        [[nodiscard]] auto lookup_item(const std::string& lexeme) -> std::optional<Arg> {
            if (auto targeted_frame = std::find_if(m_mappings.rbegin(), m_mappings.rend(), [&lexeme](const SimStackFrame& frame) -> bool {
                return frame.entries.contains(lexeme);
            }); targeted_frame != m_mappings.rend()) {
                if (const auto& [locator_n, locator_tag] = targeted_frame->entries.at(lexeme); locator_tag == Location::temp) {
                    return Arg {
                        .n = static_cast<int16_t>(locator_n - targeted_frame->stack_base),
                        .tag = Location::temp,
                    };
                } else {
                    return Arg {
                        .n = locator_n,
                        .tag = locator_tag
                    };
                }
            }

            return {};
        }

        void record_item(const std::string& name, Arg locator) {
            auto& targeted_frame = m_mappings.back();

            if (const auto& [locator_n, locator_tag] = locator; locator_tag != Location::temp)
            {
                targeted_frame.entries[name] = locator;
            } else {
                const auto absolute_temp_id = static_cast<int16_t>(locator_n + m_mappings.back().stack_base);

                targeted_frame.entries[name] = {
                    .n = absolute_temp_id,
                    .tag = Location::temp
                };
            }
        }

        [[nodiscard]] auto record_heap_item(std::same_as<Object<Value>> auto&& object) -> Arg {
            const int next_object_id = m_heap_items.size();

            m_heap_items.emplace_back(std::forward<Object<Value>>(object));

            return Arg {
                .n = static_cast<int16_t>(next_object_id),
                .tag = Location::heap_obj
            };
        }

        [[nodiscard]] auto record_constants(const std::string& literal_text, std::same_as<Value> auto&& new_value) -> Arg {
            if (auto existing_constant = lookup_item(literal_text); !existing_constant) {
                const int next_const_id = m_consts.size();
                Arg result_locator {
                    .n = static_cast<int16_t>(next_const_id),
                    .tag = Location::constant
                };
                
                record_item(literal_text, result_locator);
                m_consts.emplace_back(std::forward<Value>(new_value));

                return result_locator;
            } else if (existing_constant->tag == Location::constant) {
                return *existing_constant;
            } else {
                return Arg {
                    .n = -1,
                    .tag = Location::immediate
                };
            }
        }

        [[nodiscard]] auto record_this_func() -> Arg {
            const int next_func_id = m_func_offsets.size();
            const int next_func_code_offset = m_code.size();

            m_func_offsets.emplace_back(next_func_code_offset);

            return Arg {
                .n = static_cast<int16_t>(next_func_id),
                .tag = Location::code_chunk
            };
        }

        /// NOTE: This overload is for no-argument opcode instructions e.g NOP
        void encode_instruction(Opcode op) {
            m_code.emplace_back(Instruction {
                .args = {
                    0,
                    0,
                },
                .op = op
            });
        }

        void encode_instruction(Opcode op, Arg a0) {
            m_code.emplace_back(Instruction {
                .args = {
                    a0.n,
                    0,
                },
                .op = op
            });
        }

        void encode_instruction(Opcode op, Arg a0, Arg a1) {
            m_code.emplace_back(Instruction {
                .args = {
                    a0.n,
                    a1.n,
                },
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

        [[nodiscard]] auto emit_primitive(const Primitive& expr, const std::string& source) -> std::optional<Arg> {
            const auto& literal_token = expr.token;

            switch (literal_token.tag) {
            case TokenTag::keyword_undefined: {
                auto undef_lt_locator = record_constants("undefined", Value {});

                encode_instruction(Opcode::djs_put_const, undef_lt_locator);

                return Arg {
                    .n = static_cast<int16_t>(update_temp_id(1)),
                    .tag = Location::temp
                };
            }
            case TokenTag::keyword_null:{
                auto null_lt_locator = record_constants("null", Value {JSNullOpt {}});

                encode_instruction(Opcode::djs_put_const, null_lt_locator);

                return Arg {
                    .n = static_cast<int16_t>(update_temp_id(1)),
                    .tag = Location::temp
                };
            }
            case TokenTag::keyword_true: {
                auto boolean_lt_locator = record_constants("true", Value {
                    true
                });

                encode_instruction(Opcode::djs_put_const, boolean_lt_locator);

                return Arg {
                    .n = static_cast<int16_t>(update_temp_id(1)),
                    .tag = Location::temp
                };
            }
            case TokenTag::keyword_false: {
                auto boolean_lt_locator = record_constants("false", Value {
                    false
                });

                encode_instruction(Opcode::djs_put_const, boolean_lt_locator);

                return Arg {
                    .n = static_cast<int16_t>(update_temp_id(1)),
                    .tag = Location::temp
                };
            }
            case TokenTag::literal_int: {
                std::string int_lexeme = literal_token.as_string(source);

                auto int_lt_locator = record_constants(int_lexeme, Value {
                    std::stoi(int_lexeme)
                });

                encode_instruction(Opcode::djs_put_const, int_lt_locator);

                return Arg {
                    .n = static_cast<int16_t>(update_temp_id(1)),
                    .tag = Location::temp
                };
            }
            case TokenTag::literal_real: {
                std::string dbl_lexeme = literal_token.as_string(source);

                auto dbl_lt_locator = record_constants(dbl_lexeme, Value {
                    std::stod(dbl_lexeme)
                });

                encode_instruction(Opcode::djs_put_const, dbl_lt_locator);

                return Arg {
                    .n = static_cast<int16_t>(update_temp_id(1)),
                    .tag = Location::temp
                };
            }
            case TokenTag::identifier: {
                std::string any_name_lexeme = literal_token.as_string(source);

                if (auto name_locator_opt = lookup_item(any_name_lexeme); name_locator_opt) {
                    if (const auto locator_tag = name_locator_opt->tag; locator_tag == Location::temp) {
                        encode_instruction(djs_dup, *name_locator_opt);

                        return Arg {
                            .n = static_cast<int16_t>(update_temp_id(1)),
                            .tag = Location::temp
                        };
                    } else if (locator_tag == Location::constant) {
                        /// NOTE: Push constants as needed within blocks (even in functions), using the dedicated opcode for this case.
                        encode_instruction(Opcode::djs_put_const, *name_locator_opt);

                        return Arg {
                            .n = static_cast<int16_t>(update_temp_id(1)),
                            .tag = Location::temp
                        };
                    } else if (locator_tag == Location::heap_obj) {
                        /// NOTE: Push constants as needed within blocks (even in functions).
                        encode_instruction(Opcode::djs_put_obj_ref, *name_locator_opt);

                        return Arg {
                            .n = static_cast<int16_t>(update_temp_id(1)),
                            .tag = Location::temp
                        };
                    } else {
                        return *name_locator_opt;
                    }
                }

                return {};
            }
            default: return {};
            }
        }

        [[nodiscard]] auto emit_unary([[maybe_unused]] const Unary& expr, [[maybe_unused]] const std::string& source) -> std::optional<Arg> {
            if (const auto& [unary_expr, unary_op] = expr; unary_op == AstOp::ast_op_bang) {
                if (!emit_expr(*unary_expr, source)) {
                    return {};
                }

                encode_instruction(Opcode::djs_test_falsy);
                update_temp_id(1);

                return Arg {
                    .n = static_cast<int16_t>(current_temp_id()),
                    .tag = Location::temp
                };
            }

            return {};
        }

        [[nodiscard]] auto emit_logical_expr(AstOp logical_operator, const Expr& lhs, const Expr& rhs, const std::string& source) -> std::optional<Arg> {
            /// NOTE: no matter the stack ops, the local should rest at the bottom of the stack-slice used by the LHS, RHS evals...
            const auto logical_result_slot = m_next_temp_id;

            switch (logical_operator) {
            case AstOp::ast_op_or: {
                int lhs_jump_if_pos = -1000;
                int post_rhs_jump_pos = -1000;

                // 1. By the ES5 spec, the logical OR short-circuits: If LHS is truthy, it becomes the result and RHS is not evaluated!
                if (auto lhs_locator = emit_expr(lhs, source); !lhs_locator) {
                    return {};
                } else {
                    lhs_jump_if_pos = m_code.size();
                    encode_instruction(
                        Opcode::djs_jump_if,
                        // NOTE: this relative jump offset is a dud which will be backpatched later!
                        Arg {.n = -1, .tag = Location::immediate},
                        // NOTE: optionally pop of LHS temporary IFF LHS is falsy!
                        Arg {.n = 1, .tag = Location::immediate}
                    );
                }

                // 2. Emit the RHS evaluation & result since control flow reaching this point must have a falsy LHS.
                if (auto rhs_locator = emit_expr(rhs, source); !rhs_locator) {
                    return {};
                } else {
                    post_rhs_jump_pos = m_code.size();
                    encode_instruction(Opcode::djs_nop);
                }

                m_code[lhs_jump_if_pos].args[0] = post_rhs_jump_pos - lhs_jump_if_pos;
                m_next_temp_id = logical_result_slot + 1;

                return Arg {
                    .n = static_cast<int16_t>(logical_result_slot),
                    .tag = Location::temp
                };
            }
            case AstOp::ast_op_and: {
                int lhs_jump_else_pos = -1000;
                int post_rhs_jump_pos = -1000;

                // 1. By the ES5 spec, the logical AND short-circuits: If LHS is falsy, it becomes the result and RHS is not evaluated!
                if (auto lhs_locator = emit_expr(lhs, source); !lhs_locator) {
                    return {};
                } else {
                    lhs_jump_else_pos = m_code.size();
                    encode_instruction(
                        Opcode::djs_jump_else,
                        // NOTE: this relative, filler jump will be backpatched later!
                        Arg {.n = -1, .tag = Location::immediate},
                        // NOTE: optionally pop of LHS temporary IFF LHS is truthy!
                        Arg {.n = 1, .tag = Location::immediate}
                    );
                }

                // 2. Emit the RHS evaluation & result since control flow reaching this point must have a truthy LHS.
                if (auto rhs_locator = emit_expr(rhs, source); !rhs_locator) {
                    return {};
                } else {
                    post_rhs_jump_pos = m_code.size();
                    encode_instruction(Opcode::djs_nop);
                }

                m_code[lhs_jump_else_pos].args[0] = post_rhs_jump_pos - lhs_jump_else_pos;
                m_next_temp_id = logical_result_slot + 1;

                return Arg {
                    .n = static_cast<int16_t>(logical_result_slot),
                    .tag = Location::temp
                };
            }
            default: return {};
            }
        }

        [[nodiscard]] auto emit_binary(const Binary& expr, const std::string& source) -> std::optional<Arg> {
            struct OpcodeAssocPair {
                Opcode op;
                bool lhs_first;
                bool logical_eval; // indicates special cases: && OR ||
            };

            const auto& [lhs, rhs, bin_op] = expr;

            const auto opcode_associated_pair = ([](AstOp ast_op) noexcept -> std::optional<OpcodeAssocPair> {
                switch (ast_op) {
                    case AstOp::ast_op_percent: return OpcodeAssocPair {
                        Opcode::djs_mod,
                        true,
                        false
                    };
                    case AstOp::ast_op_times: return OpcodeAssocPair {
                        Opcode::djs_mul,
                        true,
                        false
                    };
                    case AstOp::ast_op_slash: return OpcodeAssocPair {
                        Opcode::djs_div,
                        true,
                        false
                    };
                    case AstOp::ast_op_plus: return OpcodeAssocPair {
                        Opcode::djs_add,
                        true,
                        false
                    };
                    case AstOp::ast_op_minus: return OpcodeAssocPair {
                        Opcode::djs_sub,
                        true,
                        false
                    };
                    case AstOp::ast_op_equal:
                    case AstOp::ast_op_strict_equal: return OpcodeAssocPair {
                        Opcode::djs_test_strict_eq,
                        true,
                        false
                    };
                    case AstOp::ast_op_bang_equal:
                    case AstOp::ast_op_strict_bang_equal: return OpcodeAssocPair {
                        Opcode::djs_test_strict_ne,
                        true,
                        false
                    };
                    case AstOp::ast_op_less: return OpcodeAssocPair {
                        Opcode::djs_test_lt,
                        true,
                        false
                    };
                    case AstOp::ast_op_less_equal: return OpcodeAssocPair {
                        Opcode::djs_test_lte,
                        true,
                        false
                    };
                    case AstOp::ast_op_greater: return OpcodeAssocPair {
                        Opcode::djs_test_gt,
                        true,
                        false
                    };
                    case AstOp::ast_op_greater_equal: return OpcodeAssocPair {
                        Opcode::djs_test_gte,
                        true,
                        false
                    };
                    case AstOp::ast_op_and:
                    case AstOp::ast_op_or: return OpcodeAssocPair {
                        Opcode::djs_nop,
                        true,
                        true,
                    };
                    default: return {};
                }
            })(bin_op);

            if (!opcode_associated_pair) {
                return {};
            }

            const auto [opcode, is_opcode_lefty, has_logical_eval] = *opcode_associated_pair;

            if (has_logical_eval) {
                return emit_logical_expr(bin_op, *lhs, *rhs, source);
            }

            std::optional<Arg> lhs_locator;
            std::optional<Arg> rhs_locator;
            std::optional<Arg> result_locator;

            if (is_opcode_lefty) {
                rhs_locator = emit_expr(*rhs, source);
                lhs_locator = emit_expr(*lhs, source);
                result_locator = rhs_locator;
            } else {
                lhs_locator = emit_expr(*lhs, source);
                rhs_locator = emit_expr(*rhs, source);
                result_locator = lhs_locator;
            }

            if (!lhs_locator || !rhs_locator) {
                return {};
            }

            encode_instruction(opcode);
            update_temp_id(-1);

            return result_locator;
        }

        /// TODO: handle complex LHS exprs: a member access must result in a reference being duped to the stack top.
        [[nodiscard]] auto emit_assign(const Assign& expr, const std::string& source) -> std::optional<Arg> {
            /// NOTE: the name-to-Arg mapping of some variable will be updated to this new assignment's RHS's slot. This eliminates the kludge of 'emplacing' values under the stack top which would require some hacky indexing.
            const auto& [assign_lhs, assign_rhs] = expr;
            auto assign_rhs_locator = emit_expr(*assign_rhs, source);

            if (!assign_rhs_locator) {
                return {};
            }

            auto var_name_literal = std::get_if<Primitive>(&assign_lhs->data);

            if (!var_name_literal) {
                return {};
            }

            std::string lhs_name = var_name_literal->token.as_string(source);
            auto dest_locator = lookup_item(lhs_name);

            if (!dest_locator) {
                return {};
            }

            encode_instruction(Opcode::djs_emplace_local, *dest_locator);

            return dest_locator;
        }

        [[nodiscard]] auto emit_call(const Call& expr, const std::string& source) -> std::optional<Arg> {
            /// TODO: handle call: call function by chunk ID AFTER emitting argument evaluations... The stack 'base' of each call will be the result destination.
            const auto& [call_args, callee_expr] = expr;
            int arg_count = 0;

            for (const auto& arg_box : call_args) {
                if (auto arg_locator = emit_expr(*arg_box, source); !arg_locator) {
                    return {};
                }

                ++arg_count;
            }

            auto callee_locator = emit_expr(*callee_expr, source);

            if (!callee_locator) {
                return {};
            }

            encode_instruction(Opcode::djs_call,
                *callee_locator,
                Arg {
                    .n = static_cast<int16_t>(arg_count),
                    .tag = Location::immediate
                }
            );

            const auto pre_return_temp_id = update_temp_id(-arg_count);

            return Arg {
                .n = static_cast<int16_t>(pre_return_temp_id - arg_count),
                .tag = Location::temp
            };
        }

        [[nodiscard]] auto emit_expr(const Expr& expr, const std::string& source) -> std::optional<Arg> {
            if (auto primitive_p = std::get_if<Primitive>(&expr.data); primitive_p) {
                return emit_primitive(*primitive_p, source);
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

        [[nodiscard]] auto emit_expr_stmt(const ExprStmt& stmt, const std::string& source) -> bool {
            /// NOTE: evaluate and discard expr stmt result if a temporary is actually placed... I could just make functions return `undefined` by default as per JS and then pop after.
            const auto pre_eval_temp_id = current_temp_id();

            if (!emit_expr(*stmt.expr, source)) {
                return false;
            }

            /// TODO: Conditionally pop off all temporary values generated the expr-stmt despite side effects- They may not be used if they're a discarded call result or non-assigned, temporary value.
            // const auto post_eval_temp_id = current_temp_id();
            // encode_instruction(Opcode::djs_pop, Arg {
            //     .n = static_cast<int16_t>(post_eval_temp_id - pre_eval_temp_id),
            //     .tag = Location::immediate
            // });
            // update_temp_id(pre_eval_temp_id - post_eval_temp_id);

            return true;
        }

        [[nodiscard]] auto emit_var_decl(const VarDecl& stmt, const std::string& source) -> bool {
            const auto& [var_name_token, var_init_expr] = stmt;

            
            if (auto var_init_locator = emit_expr(*var_init_expr, source); !var_init_locator) {
                return false;
            } else {
                const auto var_local_slot = m_next_temp_id - 1;
                std::string var_name = var_name_token.as_string(source);

                record_item(var_name, Arg {
                    .n = static_cast<int16_t>(var_local_slot),
                    .tag = Location::temp
                });
            }

            return true;
        }

        [[nodiscard]] auto emit_variables(const Variables& stmt, const std::string& source) -> bool {
            for (const auto& sub_var_decl : stmt.vars) {
                if (!emit_var_decl(sub_var_decl, source)) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] auto emit_if(const If& stmt_if, const std::string& source) -> bool {
            if (!emit_expr(*stmt_if.check, source)) {
                return false;
            }

            const int pre_if_body_jump_ip = m_code.size();

            encode_instruction(Opcode::djs_jump_else, Arg {
                .n = -1,
                .tag = Location::immediate
            });

            /// NOTE: the check (even as truthy) is a temporary boolean, so pop it to avoid stack waste!
            encode_instruction(Opcode::djs_pop);
            update_temp_id(-1);

            if (!emit_stmt(*stmt_if.body_true, source)) {
                return false;
            }

            const int skip_else_body_jump_ip = m_code.size();

            if (stmt_if.body_false) {
                encode_instruction(Opcode::djs_jump, Arg {.n = -1000, .tag = Location::immediate});
            }

            const int post_truthy_part_jump_ip = m_code.size();

            encode_instruction(Opcode::djs_nop);

            /// NOTE: the check (even if falsy) is a temporary boolean, so pop it to avoid stack waste!
            encode_instruction(Opcode::djs_pop);

            const int falsy_jump_offset = post_truthy_part_jump_ip - pre_if_body_jump_ip;

            if (falsy_jump_offset < min_bc_jump_offset || falsy_jump_offset > max_bc_jump_offset) {
                return false;
            }

            m_code.at(pre_if_body_jump_ip).args[0] = falsy_jump_offset;

            if (const auto falsy_if_part_p = stmt_if.body_false.get(); falsy_if_part_p) {
                if (!emit_stmt(*falsy_if_part_p, source)) {
                    return false;
                } else {
                    const int post_if_else_ip = m_code.size();
                    encode_instruction(Opcode::djs_nop);

                    m_code[skip_else_body_jump_ip].args[0] = post_if_else_ip - skip_else_body_jump_ip;
                }
            }

            return true;
        }

        [[nodiscard]] auto emit_return(const Return& stmt, const std::string& source) -> bool {
            if (!emit_expr(*stmt.result, source)) {
                return false;
            }

            encode_instruction(Opcode::djs_ret);

            return true;
        }

        [[nodiscard]] auto emit_while(const While& loop_by_while, const std::string& source) -> bool {
            const auto& [loop_expr, loop_body] = loop_by_while;

            /// NOTE: 1. Before the loop begins each iteration, the backwards IP jump must repeat the condition. Therefore, the DJS_JUMP after the body's bytecode must return here if truthy.
            const int pre_loop_pos = m_code.size();
            encode_instruction(Opcode::djs_nop);

            if (!emit_expr(*loop_expr, source)) {
                return false;
            }

            // 2. Emit a stub DJS_JUMP_ELSE that'll exit the loop when the preceeding check fails. JUMP_ELSE optionally removes the LHS / tested temporary IFF it's truthy!
            const int loop_checked_jump_pos = m_code.size();
            encode_instruction(Opcode::djs_jump_else, Arg {.n = -1, .tag = Location::immediate}, Arg {.n = 1, .tag = Location::immediate});

            // 2b. Ensure that the temporary loop-check's boolean is popped just after evaluation
            if (!emit_stmt(*loop_body, source)) {
                return false;
            }

            // 3. Emit the back-jump to repeat the loop, especially the check 1st.
            const int repeat_loop_jump_pos = m_code.size();
            encode_instruction(Opcode::djs_jump, Arg {
                .n = static_cast<int16_t>(pre_loop_pos - repeat_loop_jump_pos),
                .tag = Location::immediate
            });

            const int end_loop_pos = m_code.size();
            encode_instruction(Opcode::djs_nop);

            // 4. Patch loop-exit DJS_JUMP_ELSE here as per Step 2
            m_code[loop_checked_jump_pos].args[0] = end_loop_pos - loop_checked_jump_pos;

            return true;
        }

        [[nodiscard]] auto emit_block(const Block& stmt, const std::string& source) -> bool {
            for (const auto& temp_stmt : stmt.items) {
                if (!emit_stmt(*temp_stmt, source)) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] auto emit_function_decl(const FunctionDecl& stmt, const std::string& source) -> bool {
            /// TODO: emit each function declaration like this:
            // 1 -> define params as offsets from a new stack offset point (0 relative to the new call frame which would be pushed by the VM)
            // 2 -> emit the body
            const auto& [func_params, func_name_token, func_body] = stmt;
            std::string func_name = func_name_token.as_string(source);

            record_item(func_name, record_this_func());

            enter_simulated_frame();

            for (const auto& param : func_params) {
                std::string param_name = param.as_string(source);

                record_item(param_name, Arg {
                    .n = static_cast<int16_t>(update_temp_id(1)),
                    .tag = Location::temp,
                });
            }

            if (!emit_stmt(*func_body, source)) {
                leave_simulated_frame();
                return {};
            }

            leave_simulated_frame();
            return true;
        }

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
        BytecodeGenPass()
        : m_mappings {}, m_heap_items {}, m_consts {}, m_code {}, m_func_offsets {}, m_next_temp_id {0} {
            /// NOTE: Consider global scope 1st for global variables / stmts...
            enter_simulated_frame();
        }

        [[nodiscard]] auto operator()([[maybe_unused]] const ASTUnit& tu, [[maybe_unused]] const std::flat_map<int, std::string>& source_map) -> std::optional<Program> {
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
                .heap_items = std::exchange(m_heap_items, {}),
                .consts = std::exchange(m_consts, {}),
                .code = std::exchange(m_code, {}),
                .offsets = std::exchange(m_func_offsets, {}),
                .entry_func_id = global_func_chunk.n,
            };
        }
    };
}
