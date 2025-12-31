module;

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
import runtime.value;
import runtime.bytecode;

export namespace DerkJS {
    struct GlobalFuncOpt {};

    class BytecodeGenPass {
    private:
        static constexpr Arg dud_arg {
            .n = 0,
            .tag = Location::immediate,
        };

        struct Backpatch {
            uint16_t dest_ip;
            uint16_t src_ip;
            bool is_reversed;
        };

        struct SimStackFrame {
            std::flat_map<std::string, Arg> entries;
            int stack_base;
        };

        std::vector<Backpatch> m_patches;
        std::vector<SimStackFrame> m_mappings;
        std::vector<Object<Value>> m_heap_items;
        std::vector<Value> m_consts;
        std::vector<Chunk> m_chunks;
        int m_temp_entry_id;

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

        [[nodiscard]] auto lookup_named_item(const std::string& name) -> std::optional<Arg> {
            if (auto targeted_frame = std::find_if(m_mappings.rbegin(), m_mappings.rend(), [&name](const SimStackFrame& frame) -> bool {
                return frame.entries.contains(name);
            }); targeted_frame != m_mappings.rend()) {
                if (const auto& [locator_n, locator_tag] = targeted_frame->entries.at(name); locator_tag == Location::temp) {
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

        void record_named_item(const std::string& name, Arg locator) {
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

        [[nodiscard]] auto record_constants(std::same_as<Value> auto&& new_value) -> Arg {
            const int next_const_id = m_consts.size();

            m_consts.emplace_back(std::forward<Value>(new_value));

            return Arg {
                .n = static_cast<int16_t>(next_const_id),
                .tag = Location::constant
            };
        }

        [[nodiscard]] auto record_this_func() -> Arg {
            const int next_func_id = m_chunks.size();

            m_chunks.emplace_back();

            return Arg {
                .n = static_cast<int16_t>(next_func_id),
                .tag = Location::code_chunk
            };
        }

        /// NOTE: This overload is for no-argument opcode instructions e.g NOP
        void encode_instruction(Opcode op) {
            m_chunks.back().emplace_back(Instruction {
                .args = {
                    dud_arg,
                    dud_arg,
                },
                .op = op
            });
        }

        void encode_instruction(Opcode op, Arg a0) {
            m_chunks.back().emplace_back(Instruction {
                .args = {
                    a0,
                    dud_arg,
                },
                .op = op
            });
        }

        void encode_instruction(Opcode op, Arg a0, Arg a1) {
            m_chunks.back().emplace_back(Instruction {
                .args = {
                    a0,
                    a1,
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
                auto undef_lt_locator = record_constants(Value {});

                encode_instruction(Opcode::djs_push, undef_lt_locator);

                return Arg {
                    .n = static_cast<int16_t>(update_temp_id(1)),
                    .tag = Location::temp
                };
            }
            case TokenTag::keyword_null:{
                auto null_lt_locator = record_constants(Value {JSNullOpt {}});

                encode_instruction(Opcode::djs_push, null_lt_locator);

                return null_lt_locator;
            }
            case TokenTag::keyword_true:
            case TokenTag::keyword_false: {
                auto boolean_lt_locator = record_constants(Value {
                    literal_token.as_str(source) == "true"
                });

                encode_instruction(Opcode::djs_push, boolean_lt_locator);

                return Arg {
                    .n = static_cast<int16_t>(update_temp_id(1)),
                    .tag = Location::temp
                };
            }
            case TokenTag::literal_int: {
                auto int_lt_locator = record_constants(Value {
                    std::stoi(literal_token.as_string(source))
                });

                encode_instruction(Opcode::djs_push, int_lt_locator);

                return Arg {
                    .n = static_cast<int16_t>(update_temp_id(1)),
                    .tag = Location::temp
                };
            }
            case TokenTag::literal_real: {
                auto real_lt_locator = record_constants(Value {
                    std::stoi(literal_token.as_string(source))
                });

                encode_instruction(Opcode::djs_push, real_lt_locator);

                return Arg {
                    .n = static_cast<int16_t>(update_temp_id(1)),
                    .tag = Location::temp
                };
            }
            case TokenTag::identifier: {
                std::string any_name_lexeme = literal_token.as_string(source);

                if (auto name_locator_opt = lookup_named_item(any_name_lexeme); name_locator_opt) {
                    if (const auto locator_tag = name_locator_opt->tag; locator_tag == Location::temp) {
                        encode_instruction(djs_push, *name_locator_opt);

                        return Arg {
                            .n = static_cast<int16_t>(update_temp_id(1)),
                            .tag = Location::temp
                        };
                    } else if (locator_tag == Location::constant || locator_tag == Location::heap_obj) {
                        /// NOTE: Push constants, object refs, and argument-temps as needed within blocks (even in functions).
                        encode_instruction(djs_push, *name_locator_opt);

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
            return {}; // TODO: support negation before implementing this: it must emit code for evaluating the inner expr first BEFORE the opcode!
        }

        [[nodiscard]] auto emit_binary(const Binary& expr, const std::string& source) -> std::optional<Arg> {
            struct OpcodeAssocPair {
                Opcode op;
                bool lhs_first;
            };

            const auto& [lhs, rhs, bin_op] = expr;

            const auto opcode_associated_pair = ([](AstOp ast_op) noexcept -> std::optional<OpcodeAssocPair> {
                switch (ast_op) {
                    case AstOp::ast_op_percent: return OpcodeAssocPair {
                        Opcode::djs_mod,
                        true
                    };
                    case AstOp::ast_op_times: return OpcodeAssocPair {
                        Opcode::djs_mul,
                        true
                    };
                    case AstOp::ast_op_slash: return OpcodeAssocPair {
                        Opcode::djs_div,
                        false
                    };
                    case AstOp::ast_op_plus: return OpcodeAssocPair {
                        Opcode::djs_add,
                        true
                    };
                    case AstOp::ast_op_minus: return OpcodeAssocPair {
                        Opcode::djs_sub,
                        true
                    };
                    case AstOp::ast_op_equal:
                    case AstOp::ast_op_strict_equal: return OpcodeAssocPair {
                        Opcode::djs_test_strict_eq,
                        true
                    };
                    case AstOp::ast_op_bang_equal:
                    case AstOp::ast_op_strict_bang_equal: return OpcodeAssocPair {
                        Opcode::djs_test_strict_ne,
                        true
                    };
                    case AstOp::ast_op_less: return OpcodeAssocPair {
                        Opcode::djs_test_lt,
                        true
                    };
                    case AstOp::ast_op_less_equal: return OpcodeAssocPair {
                        Opcode::djs_test_lte,
                        true
                    };
                    case AstOp::ast_op_greater: return OpcodeAssocPair {
                        Opcode::djs_test_gt,
                        true
                    };
                    case AstOp::ast_op_greater_equal: return OpcodeAssocPair {
                        Opcode::djs_test_gte,
                        true
                    };
                    default: return {};
                }
            })(bin_op);

            if (!opcode_associated_pair) {
                return {};
            }

            const auto [opcode, is_opcode_lefty] = *opcode_associated_pair;
            std::optional<Arg> lhs_locator;
            std::optional<Arg> rhs_locator;
            std::optional<Arg> result_locator;

            if (is_opcode_lefty) {
                lhs_locator = emit_expr(*lhs, source);
                rhs_locator = emit_expr(*rhs, source);
                result_locator = lhs_locator;
            } else {
                rhs_locator = emit_expr(*rhs, source);
                lhs_locator = emit_expr(*lhs, source);
                result_locator = rhs_locator;
            }

            if (!lhs_locator || !rhs_locator) {
                return {};
            }

            encode_instruction(opcode);
            update_temp_id(-1);

            return result_locator;
        }

        [[nodiscard]] auto emit_assign(const Assign& expr, const std::string& source) -> std::optional<Arg> {
            /// NOTE: the name-to-Arg mapping of some variable will be updated to this new assignment's RHS's slot. This eliminates the kludge of 'emplacing' values under the stack top which would require some hacky indexing.
            const auto& [assign_lhs, assign_rhs] = expr;
            auto assign_rhs_locator = emit_expr(*assign_rhs, source);

            if (!assign_rhs_locator) {
                return {};
            }

            if (auto var_name_literal = std::get_if<Primitive>(&assign_lhs->data); var_name_literal) {
                std::string lhs_name = var_name_literal->token.as_string(source);

                record_named_item(lhs_name, *assign_rhs_locator);

                return assign_rhs_locator;
            }
            
            /// TODO: handle complex LHS exprs: a member access must result in a reference being duped to the stack top.

            return {};
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

            const auto post_eval_temp_id = current_temp_id();

            /// NOTE: Pop off all temporary values generated the expr-stmt despite side effects- They will not be used as their lifetime is brief.
            encode_instruction(Opcode::djs_pop, Arg {
                .n = static_cast<int16_t>(post_eval_temp_id - pre_eval_temp_id),
                .tag = Location::immediate
            });
            update_temp_id(pre_eval_temp_id - post_eval_temp_id);

            return true;
        }

        [[nodiscard]] auto emit_var_decl(const VarDecl& stmt, const std::string& source) -> bool {
            const auto& [var_name_token, var_init_expr] = stmt;
            
            if (auto var_init_locator = emit_expr(*var_init_expr, source); !var_init_locator) {
                return false;
            } else {
                std::string var_name = var_name_token.as_string(source);

                record_named_item(var_name, *var_init_locator);
            }

            return true;
        }

        [[nodiscard]] auto emit_variables(const Variables& stmt, const std::string& source) -> bool {
            /// NOTE: Here, I handle RHS evaluations and record each slot they end up in, mapping each slot to a variable name... the LHS'es may later need further evaluation- BUT not now since local names' storage become their initializer on the VM stack.

            for (const auto& sub_var_decl : stmt.vars) {
                if (!emit_var_decl(sub_var_decl, source)) {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] auto emit_if(const If& stmt_if, const std::string& source) -> bool {
            /// TODO: 1st, generate the check evaluation... 2nd, save a backpatch with partial information (except forward jump offset)... 3rd, emit the body and then complete the backpatch... lastly, apply the patch
            auto cond_check_locator = emit_expr(*stmt_if.check, source);

            if (!cond_check_locator) {
                return false;
            }
            
            // Emit dud jump to patch later...
            encode_instruction(Opcode::djs_test);

            const int pre_if_body_jump_ip = m_chunks.back().size();

            encode_instruction(Opcode::djs_jump_else, Arg {
                .n = -1,
                .tag = Location::immediate
            });

            if (!emit_stmt(*stmt_if.body_true, source)) {
                return false;
            }

            const int post_if_body_jump_ip = m_chunks.back().size();

            encode_instruction(Opcode::djs_nop);

            const int falsy_jump_offset = post_if_body_jump_ip - pre_if_body_jump_ip;

            m_chunks.back().at(pre_if_body_jump_ip).args[0].n = falsy_jump_offset;

            return true;
        }

        [[nodiscard]] auto emit_return(const Return& stmt, const std::string& source) -> bool {
            if (!emit_expr(*stmt.result, source)) {
                return false;
            }

            encode_instruction(Opcode::djs_ret);

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

            record_named_item(func_name, record_this_func());

            enter_simulated_frame();

            for (const auto& param : func_params) {
                std::string param_name = param.as_string(source);

                record_named_item(param_name, Arg {
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
            } else if (auto block_p = std::get_if<Block>(&stmt.data); block_p) {
                return emit_block(*block_p, source);
            } else if (auto func_p = std::get_if<FunctionDecl>(&stmt.data); func_p) {
                return emit_function_decl(*func_p, source);
            }

            return false;
        }

    public:
        BytecodeGenPass()
        : m_patches {}, m_mappings {}, m_heap_items {}, m_consts {}, m_chunks {}, m_temp_entry_id {-1}, m_next_temp_id {0} {
            /// NOTE: Consider global scope 1st for global variables / stmts...
            enter_simulated_frame();
        }

        [[nodiscard]] auto operator()([[maybe_unused]] const ASTUnit& tu, [[maybe_unused]] const std::flat_map<int, std::string>& source_map) -> std::optional<Program> {
            /// TODO: 1. emit all function decls ONLY as per JS function hoisting.
            for (const auto& [src_filename, decl, src_id] : tu) {
                if (std::holds_alternative<FunctionDecl>(decl->data)) {
                    if (!emit_stmt(*decl, source_map.at(src_id))) {
                        std::println(std::cerr, "Compile Error at source '{}' around '{}': unsupported JS construct :(\n", src_filename, source_map.at(src_id).substr(decl->text_begin, decl->text_length / 2));
                        continue;
                    }
                }
            }

            auto global_func_chunk = record_this_func();

            /// TODO: 2. emit all top-level non-function statements as an implicit function that's called right away.
            for (const auto& [src_filename, decl, src_id] : tu) {
                if (!std::holds_alternative<FunctionDecl>(decl->data)) {
                    if (!emit_stmt(*decl, source_map.at(src_id))) {
                        std::println(std::cerr, "Compile Error at source '{}' around '{}': unsupported JS construct :(\n", src_filename, source_map.at(src_id).substr(decl->text_begin, decl->text_length / 2));
                        continue;
                    }
                }
            }

            return Program {
                .heap_items = std::exchange(m_heap_items, {}),
                .consts = std::exchange(m_consts, {}),
                .chunks = std::exchange(m_chunks, {}),
                .entry_func_id = global_func_chunk.n,
            };
        }
    };
}
