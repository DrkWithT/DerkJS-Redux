module;

#include <utility>
#include <memory>
#include <optional>
#include <vector>
#include <variant>
#include <forward_list>
#include <string>

export module backend.expr_gen;

import backend.bc_generate;

namespace DerkJS::Backend {
    export class PrimitiveEmitter : public ExprEmitterBase<Expr> {
    public:
        PrimitiveEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Expr& node, const std::string& source) -> bool override {
            const auto& [pmt_token, pmt_is_key] = std::get<Primitive>(node.data);
            std::string atom_lexeme = pmt_token.as_string(source);
            const TokenTag expr_token_tag = pmt_token.tag;

            auto primitive_locator = ([&] () -> std::optional<Arg> {
                switch (expr_token_tag) {
                case TokenTag::keyword_this:
                    context.m_has_string_ops = false;
                    return Arg { .n = -2, .tag = Location::heap_obj, .is_str_literal = false, .from_closure = false };
                case TokenTag::keyword_undefined:
                    context.m_has_string_ops = false;
                    context.m_accessing_property = false;
                    return context.lookup_symbol("undefined", FindGlobalConstsOpt {});
                case TokenTag::keyword_null:
                    context.m_has_string_ops = false;
                    context.m_accessing_property = false;
                    return context.lookup_symbol("null", FindGlobalConstsOpt {});
                case TokenTag::keyword_true: case TokenTag::keyword_false:
                    context.m_has_string_ops = false;
                    context.m_accessing_property = false;
                    return context.lookup_symbol(atom_lexeme, FindGlobalConstsOpt {});
                case TokenTag::literal_int:
                    context.m_has_string_ops = false;
                    context.m_accessing_property = false;
                    return context.record_symbol(atom_lexeme, Value {std::stoi(atom_lexeme)}, FindGlobalConstsOpt {});
                case TokenTag::literal_real:
                    context.m_has_string_ops = false;
                    context.m_accessing_property = false;
                    return context.record_symbol(atom_lexeme, Value {std::stod(atom_lexeme)}, FindGlobalConstsOpt {});
                case TokenTag::literal_string: {
                    context.m_has_string_ops = true;
                    // Map '"abc"' -> "abc"
                    return context.record_symbol(atom_lexeme, atom_lexeme.substr(1, atom_lexeme.length() - 2), FindGlobalConstsOpt {});
                }
                case TokenTag::keyword_prototype: {
                    context.m_has_string_ops = false;
                    return Arg { .n = -3, .tag = Location::heap_obj, .is_str_literal = false, .from_closure = false };
                }
                case TokenTag::identifier: {
                    context.m_has_string_ops = false;

                    // Case 1: property keys are always constant strings.
                    if (context.m_callee_name == atom_lexeme && !context.m_accessing_property && context.m_has_call) {
                        return Arg {.n = -1, .tag = Location::end, .is_str_literal = false, .from_closure = false};
                    } else if (pmt_is_key) {
                        return context.record_symbol(atom_lexeme, atom_lexeme, FindKeyConstOpt {});
                    } else {
                        // Case 2.1: The name is for a global native / local variable.
                        return context.lookup_symbol(atom_lexeme)
                        // Case 2.2: The name is a non-local variable BUT non-global.
                        .or_else([&] () {
                            return context.record_symbol(atom_lexeme, atom_lexeme, FindCaptureOpt {});
                        });
                    }
                }
                default: return {};
                }
            })();

            if (!primitive_locator) {
                return false;
            } else if (const auto primitive_locate_type = primitive_locator->tag; primitive_locate_type == Location::constant) {
                context.encode_instruction(Opcode::djs_put_const, *primitive_locator);

                if (primitive_locator->from_closure) {
                    context.encode_instruction(Opcode::djs_ref_upval);

                    if (!context.m_access_as_lval) {
                        context.encode_instruction(Opcode::djs_deref);
                    }
                }
            } else if (const auto local_var_opcode = (context.m_access_as_lval && !context.m_accessing_property) ? Opcode::djs_ref_local : Opcode::djs_dup_local; primitive_locate_type == Location::local) {
                context.encode_instruction(local_var_opcode, *primitive_locator);
            } else if (primitive_locate_type == Location::heap_obj && primitive_locator->n == -2) {
                context.encode_instruction(Opcode::djs_put_this);
            } else if (primitive_locate_type == Location::heap_obj && primitive_locator->n == -3) {
                context.encode_instruction(Opcode::djs_put_proto_key);
            } else if (primitive_locate_type == Location::end && primitive_locator->n == -1) {
                // Put dud instruction where self-load should be before a `djs_object_call`.
                context.m_local_maps.back().callee_self_refs.emplace_back(static_cast<int>(context.m_code_blobs.front().size()));
                context.encode_instruction(Opcode::djs_nop);
            }

            return true;
        }
    };

    export class ObjectLiteralEmitter : public ExprEmitterBase<Expr> {
    public:
        ObjectLiteralEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Expr& node, const std::string& source) -> bool override {
            const auto& [fields] = std::get<ObjectLiteral>(node.data);

            context.encode_instruction(Opcode::djs_put_obj_dud);
            context.m_accessing_property = false;

            for (const auto& [prop_name_token, prop_init_expr] : fields) {
                std::string prop_name = prop_name_token.as_string(source);

                auto prop_name_locator = context.record_symbol(prop_name, prop_name, FindKeyConstOpt {});

                if (!prop_name_locator) {
                    return false;
                }

                context.encode_instruction(Opcode::djs_put_const, *prop_name_locator);

                if (!context.emit_expr(*prop_init_expr, source)) {
                    return false;
                }

                context.encode_instruction(Opcode::djs_put_prop);
            }

            return true;
        }
    };

    export class ArrayLiteralEmitter : public ExprEmitterBase<Expr> {
    public:
        ArrayLiteralEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Expr& node, const std::string& source) -> bool override {
            const auto& [items] = std::get<ArrayLiteral>(node.data);

            // 1. Push each JS array item via evaluation.
            int item_count = 0;

            for (const auto& item_expr : items) {
                if (!context.emit_expr(*item_expr, source)) {
                    return false;
                }

                ++item_count;
            }

            // 2. Invoke this special opcode. Now there's a new array for use. :)
            context.encode_instruction(
                Opcode::djs_make_arr,
                Arg {.n = static_cast<int16_t>(item_count), .tag = Location::immediate}
            );

            return true;
        }
    };

    export class LambdaLiteralEmitter : public ExprEmitterBase<Expr> {
    public:
        LambdaLiteralEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Expr& node, const std::string& source) -> bool override {
            const auto& [lambda_params, lambda_body] = std::get<LambdaLiteral>(node.data);
            context.m_accessing_property = false;

            // 1. Begin lambda code emission, but let's note that this nested "code scope" place a new buffer as the currently filling one before it's done & craps out a Lambda object... Which gets moved into the bytecode `Program` later.
            context.m_local_maps.emplace_back(CodeGenScope {
                .locals = {},
                .callee_self_refs = {},
                .next_local_id = 0,
                .block_level = 0
            });
            context.m_code_blobs.emplace_front();

            for (const auto& param_token : lambda_params) {
                std::string param_name = param_token.as_string(source);

                if (!context.record_symbol(param_name, RecordLocalOpt {}, FindLocalsOpt {})) {
                    return false;
                }
            }

            const auto old_prepass_vars_flag = context.m_prepass_vars;
            context.m_prepass_vars = true;
            context.m_in_callable = true;

            if (!context.emit_stmt(*lambda_body, source)) {
                return false;
            }

            context.m_prepass_vars = false;

            if (!context.emit_stmt(*lambda_body, source)) {
                return false;
            }

            context.m_in_callable = false;

            // 2. Record the Lambda's code as a wrapper object into the VM heap, preloaded.
            const int16_t next_global_ref_const_id = context.m_consts.size();
            Arg next_global_ref_loc {
                .n = next_global_ref_const_id,
                .tag = Location::constant
            };

            // 3. Patch bytecode NOP stubs where self-calls should be.
            for (const auto& dud_self_call_offsets = context.m_local_maps.back().callee_self_refs; const auto call_stub_pos : dud_self_call_offsets) {
                context.m_code_blobs.front().at(call_stub_pos) = Instruction {
                    .args = {next_global_ref_const_id, 0},
                    .op = Opcode::djs_put_const
                };
            }

            Lambda temp_callable {
                std::move(context.m_code_blobs.front()),
                context.m_heap.add_item(context.m_heap.get_next_id(), std::make_unique<Object>(nullptr, std::to_underlying(AttrMask::unused)))
            };
            
            if (auto lambda_object_ptr = context.m_heap.add_item(context.m_heap.get_next_id(), std::move(temp_callable)); lambda_object_ptr) {
                // As per any DerkJS object, the real thing is owned by the VM heap but is referenced by many non-owning raw pointers to its base: `ObjectBase<Value>*`.
                context.m_consts.emplace_back(Value {lambda_object_ptr});
            } else {
                return false;
            }

            context.m_code_blobs.pop_front();
            context.m_local_maps.pop_back();
            context.m_prepass_vars = old_prepass_vars_flag;
            context.m_callee_name.clear();
            context.encode_instruction(
                Opcode::djs_put_const,
                next_global_ref_loc
            );

            return true;
        }
    };

    export class MemberAccessEmitter : public ExprEmitterBase<Expr> {
    public:
        MemberAccessEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Expr& node, const std::string& source) -> bool override {
            const auto& [target_expr, key_expr] = std::get<MemberAccess>(node.data);

            // 1. Emit the target and then the key evaluations. The `djs_get_prop` opcode takes the target and the property key (PropertyHandle<Value>) on top... These two are "popped" and the property reference takes their place in the VM. If the top callee expr has a calling property, the target object is emitted again (member-access-depth == 1).
            context.m_accessing_property = true;
            context.m_member_depth++;

            // For possible this object on member calls OR the parent object reference itself...
            if (!context.emit_expr(*target_expr, source)) {
                return false;
            }

            // If a property call is enclosing the member access: The callee's parent object is over the copied reference for `this`...
            if (context.m_has_call && context.m_member_depth <= 1) {
                context.encode_instruction(Opcode::djs_dup);
            }

            if (!context.emit_expr(*key_expr, source)) {
                return false;
            }

            context.m_member_depth--;

            /// NOTE: If assignment (lvalues apply) pass `1` for defaulting flag so the RHS goes somewhere legitimate.
            context.encode_instruction(
                Opcode::djs_get_prop,
                Arg {
                    .n = static_cast<int16_t>(!context.m_has_call && context.m_access_as_lval),
                    .tag = Location::immediate
                }
            );

            // NOTE: IF !m_access_as_lval, the member access dereferences the property reference for a usable temporary: `foo.a (ref Value(1)) + foo.b (ref Value(2))` -> `1 + 2 == 3`
            if (!context.m_access_as_lval) {
                context.encode_instruction(Opcode::djs_deref);
            }

            return true;
        }
    };

    export class UnaryEmitter : public ExprEmitterBase<Expr> {
    public:
        UnaryEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Expr& node, const std::string& source) -> bool override {
            struct OpcodeWithGenFlag {
                Opcode op;
                bool inner_ok;
            };

            const auto& [inner_expr, expr_op] = std::get<Unary>(node.data);
            context.m_accessing_property = false;

            const auto opcode_and_check = ([&]() -> std::optional<OpcodeWithGenFlag> {
                switch (expr_op) {
                    case AstOp::ast_op_bang:
                        return OpcodeWithGenFlag {
                            .op = Opcode::djs_test_falsy,
                            .inner_ok = context.emit_expr(*inner_expr, source)
                        };
                    case AstOp::ast_op_plus:
                        return OpcodeWithGenFlag {
                            .op = Opcode::djs_numify,
                            .inner_ok = context.emit_expr(*inner_expr, source)
                        };
                    case AstOp::ast_op_prefix_inc: {
                        const bool old_access_as_lval = context.m_access_as_lval;
                        context.m_access_as_lval = true;
                        
                        OpcodeWithGenFlag temp_inc_result {
                            .op = Opcode::djs_pre_inc,
                            .inner_ok = context.emit_expr(*inner_expr, source)
                        };

                        context.m_access_as_lval = old_access_as_lval;
                        return temp_inc_result;
                    }
                    case AstOp::ast_op_prefix_dec: {
                        const bool old_access_as_lval = context.m_access_as_lval;
                        context.m_access_as_lval = true;
                        
                        OpcodeWithGenFlag temp_inc_result {
                            .op = Opcode::djs_pre_dec,
                            .inner_ok = context.emit_expr(*inner_expr, source)
                        };

                        context.m_access_as_lval = old_access_as_lval;
                        return temp_inc_result;
                    }
                    case AstOp::ast_op_void:
                        return OpcodeWithGenFlag {
                            .op = Opcode::djs_discard,
                            .inner_ok = context.emit_expr(*inner_expr, source)
                        };
                    case AstOp::ast_op_typeof:
                        return OpcodeWithGenFlag {
                            .op = Opcode::djs_typename,
                            .inner_ok = context.emit_expr(*inner_expr, source)
                        };
                    case AstOp::ast_op_new: {
                        context.m_has_new_applied = true;

                        /// NOTE: Only ctor calls are done for new exprs e.g `new Foo(1, 2, 3);`
                        const auto within_new_expr_ok = context.emit_expr(*inner_expr, source);

                        return OpcodeWithGenFlag {
                            .op = Opcode::djs_nop,
                            .inner_ok = within_new_expr_ok
                        };
                    }
                    default: return {};
                }
            })();

            if (!opcode_and_check) {
                return false;
            } else if (const auto [opcode, inner_ok] = *opcode_and_check; !inner_ok) {
                return false;
            } else if (opcode != Opcode::djs_nop) {
                context.encode_instruction(opcode);
            }

            return true;
        }
    };

    export class BinaryEmitter : public ExprEmitterBase<Expr> {
    private:
        [[nodiscard]] auto emit_logical_expr(BytecodeEmitterContext& context, AstOp logical_operator, const ExprPtr& lhs, const ExprPtr& rhs, const std::string& source) -> bool {
            /// NOTE: No matter the logical operator, only one temporary remains but by differing rules:
            // 1. AND: IFF LHS is falsy, it's the result. RHS otherwise.
            // 2. OR: IFF LHS is truthy, it's the result. RHS otherwise.
            context.m_accessing_property = false;

            // 1. Emit LHS evaluation
            if (!context.emit_expr(*lhs, source)) {
                return false;
            }

            // 2. Emit dud conditional jump for the "OR" / "AND"
            const int pre_logic_check_pos = context.m_code_blobs.front().size();

            if (logical_operator == AstOp::ast_op_and) {
                context.encode_instruction(
                    Opcode::djs_jump_else,
                    /// NOTE: backpatch relative jump offset next
                    Arg {.n = -1, .tag = Location::immediate}
                );
            } else if (logical_operator == AstOp::ast_op_or) {
                context.encode_instruction(
                    Opcode::djs_jump_if,
                    Arg {.n = -1, .tag = Location::immediate}
                );
            }

            // 3. Emit RHS evaluation
            if (!context.emit_expr(*rhs, source)) {
                return false;
            }

            const int post_logic_check_pos = context.m_code_blobs.front().size();
            const auto patch_jump_offset = post_logic_check_pos - pre_logic_check_pos;

            context.encode_instruction(Opcode::djs_nop, {});

            // 4. Patch conditional jump offset which is taken on short-circuited evaluation.
            context.m_code_blobs.front().at(pre_logic_check_pos).args[0] = patch_jump_offset;

            return true;
        }

    public:
        BinaryEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Expr& node, const std::string& source) -> bool override {
            const auto& [expr_lhs, expr_rhs, expr_op] = std::get<Binary>(node.data);
            context.m_accessing_property = false;

            // Case 1: emit logical operator expressions if applicable since these have special short-circuiting vs. arithmetic ones...
            if (expr_op == AstOp::ast_op_and || expr_op == AstOp::ast_op_or) {
                return emit_logical_expr(context, expr_op, expr_lhs, expr_rhs, source);
            }

            // Case 2 (General): arithmtic operators MUST evaluate RHS before LHS in case one operator is right-associative. The only other left-associative ones supported now are commutative: ADD, MUL, MOD...
            if (!context.emit_expr(*expr_rhs, source)) {
                return false;
            }
            context.m_member_depth = 0;

            if (!context.emit_expr(*expr_lhs, source)) {
                return false;
            }
            context.m_member_depth = 0;

            const auto deduced_opcode = ([](AstOp op) noexcept -> std::optional<Opcode> {
                switch (op) {
                    case AstOp::ast_op_percent: return Opcode::djs_mod;
                    case AstOp::ast_op_times: return Opcode::djs_mul;
                    case AstOp::ast_op_slash: return Opcode::djs_div;
                    case AstOp::ast_op_plus: return Opcode::djs_add;
                    case AstOp::ast_op_minus: return Opcode::djs_sub;
                    case AstOp::ast_op_equal: case AstOp::ast_op_strict_equal: return Opcode::djs_test_strict_eq;
                    case AstOp::ast_op_bang_equal: case AstOp::ast_op_strict_bang_equal: return Opcode::djs_test_strict_ne;
                    case AstOp::ast_op_less: return Opcode::djs_test_lt;
                    case AstOp::ast_op_less_equal: return Opcode::djs_test_lte;
                    case AstOp::ast_op_greater: return Opcode::djs_test_gt;
                    case AstOp::ast_op_greater_equal: return Opcode::djs_test_gte;
                    default: return {};
                }
            })(expr_op);

            if (!deduced_opcode) {
                return false;
            }

            if (context.m_has_string_ops && expr_op == AstOp::ast_op_plus) {
                context.encode_instruction(Opcode::djs_strcat);
            } else {
                context.encode_instruction(*deduced_opcode);
            }

            return true;
        }
    };

    export class AssignEmitter : public ExprEmitterBase<Expr> {
    public:
        AssignEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Expr& node, const std::string& source) -> bool override {
            /// NOTE: JS "lvalues" are assignable, so set this flag accordingly for codegen.
            const auto& [lhs_expr, rhs_expr] = std::get<Assign>(node.data);
            context.m_accessing_property = false;
            
            /// NOTE: Only members and local names (non-duped slots) are assignable. This easily prevents stack misalignments by 1.
            if (!std::holds_alternative<Primitive>(lhs_expr->data) && !std::holds_alternative<MemberAccess>(lhs_expr->data)) {
                return false;
            }

            context.m_access_as_lval = true;

            if (!context.emit_expr(*lhs_expr, source)) {
                return false;
            }

            context.m_access_as_lval = false;
            context.m_member_depth = 0;

            if (!context.emit_expr(*rhs_expr, source)) {
                return false;
            }

            context.encode_instruction(Opcode::djs_emplace);

            return true;
        }
    };

    export class CallEmitter : public ExprEmitterBase<Expr> {
    public:
        CallEmitter() noexcept = default;

        [[nodiscard]] auto emit(BytecodeEmitterContext& context, const Expr& node, const std::string& source) -> bool override {
            const auto& [expr_args, expr_callee] = std::get<Call>(node.data);
            auto call_argc = 0;

            for (const auto& arg_p : expr_args) {
                if (!context.emit_expr(*arg_p, source)) {
                    return false;
                }

                ++call_argc;
            }
            
            context.m_has_call = true;
            context.m_access_as_lval = true;

            if (!context.emit_expr(*expr_callee, source)) {
                return false;
            }

            auto has_extra_this_arg = context.m_has_call && std::holds_alternative<MemberAccess>(expr_callee->data);

            context.m_access_as_lval = false;
            context.m_member_depth = 0;

            if (!context.m_has_new_applied) {
                context.encode_instruction(
                    Opcode::djs_object_call,
                    Arg {.n = static_cast<int16_t>(call_argc), .tag = Location::immediate},
                    Arg {.n = static_cast<int16_t>(has_extra_this_arg), .tag = Location::immediate}
                );
            } else {
                context.encode_instruction(
                    Opcode::djs_ctor_call,
                    Arg {.n = static_cast<int16_t>(call_argc), .tag = Location::immediate}
                );
                context.m_has_new_applied = false;
            }

            context.m_has_call = false;

            return true;
        }
    };
}
