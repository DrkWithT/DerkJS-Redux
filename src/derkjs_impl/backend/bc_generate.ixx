module;

#include <type_traits>
#include <utility>

#include <optional>
#include <string>
#include <forward_list>
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
import runtime.callables;
import runtime.bytecode;

export namespace DerkJS {
    struct PreloadItem {
        std::string lexeme; // empty strings are for hidden items e.g function object in console.log
        std::variant<std::unique_ptr<ObjectBase<Value>>, Value> entity;
        Location location;
    };

    struct FindGlobalConstsOpt {};
    struct FindGlobalNamesOpt {};
    struct FindLocalsOpt {};
    struct RecordLocalOpt {};
    struct RecordSpecialThisOpt {};

    struct OpcodeDelta {
        int offset;      // opcode's magnitude of RSP increment or decrement
        bool implicit;   // if opcode's effect on RSP follows delta / outside calculation
    };

    struct CodeGenScope {
        std::flat_map<std::string, Arg> locals;  // Map names to variables.
        int next_local_id;                       // Tracks next stack slot for a local variable value.
    };

    class BytecodeGenPass {
    private:
        static constexpr Arg dud_arg {
            .n = 0,
            .tag = Location::immediate
        };

        // 1st priority lookup: contains interned strings & primitive constants from top-level
        std::flat_map<std::string, Arg> m_global_consts_map;

        // 2nd priority lookup: contains function names, variable names, etc. at top level
        std::flat_map<std::string, Arg> m_globals_map;

        // 3rd priority lookup
        std::vector<CodeGenScope> m_local_maps;

        // filled with interned strings -> object-refs in consts -> stack temporaries
        PolyPool<ObjectBase<Value>> m_heap;

        // filled with primitive literals & interned string refs
        std::vector<Value> m_consts;

        // stack of bytecode buffers, accounting for arbitrary nesting of lambdas
        std::forward_list<std::vector<Instruction>> m_code_blobs;

        // filled with global function IDs -> absolute offsets into the bytecode blob
        std::vector<int> m_chunk_offsets;

        int m_immediate_inline_fn_id;

        // Whether an addition has any string operands or not.
        bool m_has_string_ops;

        // Whether the parent expression is something like `new Foo(...)`.
        bool m_has_new_applied;

        // Whether an object member access is assignable or read-from.
        bool m_access_as_lval;

        // Whether an object's property is being accessed from the parent.
        bool m_accessing_property;

        bool m_has_call;

        // Overload for universal lookup via priorities 1, 2, 3
        [[nodiscard]] auto lookup_symbol(const std::string& symbol) -> std::optional<Arg> {
            return lookup_symbol(symbol, FindGlobalConstsOpt {})
                .or_else([&symbol, this]() { return lookup_symbol(symbol, FindGlobalNamesOpt {}); })
                .or_else([&symbol, this]() { return lookup_symbol(symbol, FindLocalsOpt {}); });
        }

        // Overloads for group-specific symbol lookups
        [[nodiscard]] auto lookup_symbol(const std::string& symbol, [[maybe_unused]] FindGlobalConstsOpt opt) -> std::optional<Arg> {
            if (auto global_consts_opt = m_global_consts_map.find(symbol); global_consts_opt != m_global_consts_map.end()) {
                return global_consts_opt->second;
            }

            return {};
        }

        [[nodiscard]] auto lookup_symbol(const std::string& symbol, [[maybe_unused]] FindGlobalNamesOpt opt) -> std::optional<Arg> {
            if (auto top_local_opt = m_globals_map.find(symbol); top_local_opt != m_globals_map.end()) {
                return top_local_opt->second;
            }

            return {};
        }

        [[nodiscard]] auto lookup_symbol(const std::string& symbol, [[maybe_unused]] FindLocalsOpt opt) -> std::optional<Arg> {
            if (auto fn_local_opt = m_local_maps.back().locals.find(symbol); fn_local_opt != m_local_maps.back().locals.end()) {
                return fn_local_opt->second;
            }

            return {};
        }

        template <typename Item>
        [[maybe_unused]] auto record_valued_symbol(const std::string& symbol, Item&& item) -> std::optional<Arg> {
            using plain_item_type = std::remove_cvref_t<Item>;

            if (auto existing_arg = lookup_symbol(symbol); existing_arg) {
                return existing_arg;
            }

            if constexpr (std::is_same_v<plain_item_type, Value>) {
                // 1a. For global primitive constants:
                const int16_t next_global_const_id = m_consts.size();
                Arg next_global_loc {
                    .n = next_global_const_id,
                    .tag = Location::constant
                };

                m_consts.emplace_back(std::forward<Item>(item));
                m_global_consts_map[symbol] = next_global_loc;

                return next_global_loc;
            } else if constexpr (std::is_same_v<plain_item_type, StaticString> || std::is_same_v<plain_item_type, DynamicString> || std::is_same_v<plain_item_type, std::unique_ptr<ObjectBase<Value>>>) {
                // 1a, 1b. For global & interned string references as constants:
                const int16_t next_global_ref_const_id = m_consts.size();
                Arg next_global_ref_loc {
                    .n = next_global_ref_const_id,
                    .tag = Location::constant
                };

                if (auto heap_static_str_p = m_heap.add_item(m_heap.get_next_id(), std::move(item)); heap_static_str_p) {
                    m_consts.emplace_back(Value {heap_static_str_p});
                    m_global_consts_map[symbol] = next_global_ref_loc;
                } else {
                    return {};
                }

                return next_global_ref_loc;
            } else if constexpr (std::is_same_v<plain_item_type, RecordLocalOpt>) {
                // 3. For named locals in functions:
                const int16_t next_local_id = m_local_maps.back().next_local_id;
                Arg next_local_loc {
                    .n = next_local_id,
                    .tag = Location::local
                };

                m_local_maps.back().locals[symbol] = next_local_loc;
                ++m_local_maps.back().next_local_id;

                return next_local_loc;
            } else if constexpr (std::is_same_v<plain_item_type, RecordSpecialThisOpt>) {
                Arg temp_this_special_loc {
                    .n = -2,
                    .tag = Location::heap_obj
                };

                m_global_consts_map[symbol] = temp_this_special_loc;

                return temp_this_special_loc;
            }

            return {};
        }
    
        [[nodiscard]] auto record_function_begin(const std::string& func_name) -> int {
            const int next_fn_id = m_chunk_offsets.size();
            int abs_code_offset = m_code_blobs.front().size();

            if (m_globals_map.contains(func_name)) {
                return -1;
            }

            m_chunk_offsets.emplace_back(abs_code_offset);
            m_globals_map[func_name] = Arg {
                .n = static_cast<int16_t>(next_fn_id),
                .tag = Location::code_chunk
            };

            return next_fn_id;
        }

        /// NOTE: This overload is for no-argument opcodes
        void encode_instruction(Opcode op) {
            m_code_blobs.front().emplace_back(Instruction {
                .args = { 0, 0 },
                .op = op
            });
        }

        /// NOTE: this overload is for single-argument opcodes
        void encode_instruction(Opcode op, Arg a0) {
            m_code_blobs.front().emplace_back(Instruction {
                .args = { a0.n, 0 },
                .op = op
            });
        }

        /// NOTE: this overload is for double-argument opcodes
        void encode_instruction(Opcode op, Arg a0, Arg a1) {
            m_code_blobs.front().emplace_back(Instruction {
                .args = { a0.n, a1.n},
                .op = op
            });
        }

        [[nodiscard]] auto emit_primitive(const Primitive& expr, const std::string& source) -> bool {
            std::string atom_lexeme = expr.token.as_string(source);
            const TokenTag expr_token_tag = expr.token.tag;

            auto primitive_locator = ([&, this] () -> std::optional<Arg> {
                switch (expr_token_tag) {
                case TokenTag::keyword_this:
                    m_has_string_ops = false;
                    return record_valued_symbol(atom_lexeme, RecordSpecialThisOpt {});
                case TokenTag::keyword_undefined:
                    m_has_string_ops = false;
                    m_accessing_property = false;
                    return record_valued_symbol(atom_lexeme, Value {});
                case TokenTag::keyword_null:
                    m_has_string_ops = false;
                    m_accessing_property = false;
                    return record_valued_symbol(atom_lexeme, Value {JSNullOpt {}});
                case TokenTag::keyword_true: case TokenTag::keyword_false:
                    m_has_string_ops = false;
                    m_accessing_property = false;
                    return record_valued_symbol(atom_lexeme, Value {atom_lexeme == "true"});
                case TokenTag::literal_int:
                    m_has_string_ops = false;
                    m_accessing_property = false;
                    return record_valued_symbol(atom_lexeme, Value {std::stoi(atom_lexeme)});
                case TokenTag::literal_real:
                    m_has_string_ops = false;
                    m_accessing_property = false;
                    return record_valued_symbol(atom_lexeme, Value {std::stod(atom_lexeme)});
                case TokenTag::literal_string: {
                    m_has_string_ops = true;
                    m_accessing_property = false;

                    if (const int atom_text_length = atom_lexeme.length(); atom_text_length <= StaticString::max_length_v) {
                        return record_valued_symbol(atom_lexeme, StaticString {nullptr, atom_lexeme.c_str(), atom_text_length});
                    } else {
                        return record_valued_symbol(atom_lexeme, DynamicString {std::move(atom_lexeme)});
                    }
                }
                case TokenTag::identifier: {
                    m_has_string_ops = false;

                    if (m_accessing_property) {
                        if (const int atom_text_length = atom_lexeme.length(); atom_text_length <= StaticString::max_length_v) {
                            return record_valued_symbol(atom_lexeme, StaticString {nullptr, atom_lexeme.c_str(), atom_text_length});
                        } else {
                            return record_valued_symbol(atom_lexeme, DynamicString {std::move(atom_lexeme)});
                        }
                    } else {
                        return lookup_symbol(atom_lexeme);
                    }
                }
                default: return {};
                }
            })();

            if (!primitive_locator) {
                return false;
            } else if (const auto primitive_locate_type = primitive_locator->tag; primitive_locate_type == Location::constant) {
                encode_instruction(Opcode::djs_put_const, *primitive_locator);
            } else if (const auto local_var_opcode = (m_access_as_lval && !m_accessing_property) ? Opcode::djs_ref_local : Opcode::djs_dup_local; primitive_locate_type == Location::local) {
                encode_instruction(local_var_opcode, *primitive_locator);
            } else if (primitive_locate_type == Location::heap_obj && primitive_locator->n == -2) {
                encode_instruction(Opcode::djs_put_this);
            } else if (primitive_locate_type == Location::code_chunk) {
                m_immediate_inline_fn_id = primitive_locator->n;
            }

            return true;
        }

        [[nodiscard]] auto emit_object_literal(const ObjectLiteral& object, const std::string& source) -> bool {
            encode_instruction(Opcode::djs_put_obj_dud);
            m_accessing_property = false;

            for (const auto& [prop_name_token, prop_init_expr] : object.fields) {
                std::string prop_name = prop_name_token.as_string(source);

                auto prop_name_locator = ([this] (std::string name_s) -> std::optional<Arg> {
                    if (const int name_length = name_s.length(); name_length <= StaticString::max_length_v) {
                        return record_valued_symbol(name_s, StaticString {nullptr, name_s.c_str(), name_length});
                    } else {
                        return record_valued_symbol(name_s, DynamicString {std::move(name_s)});
                    }
                })(prop_name);

                if (!prop_name_locator) {
                    return false;
                }

                encode_instruction(Opcode::djs_put_const, *prop_name_locator);

                if (!emit_expr(*prop_init_expr, source)) {
                    return false;
                }

                encode_instruction(Opcode::djs_put_prop);
            }

            return true;
        }

        [[nodiscard]] auto emit_lambda(const LambdaLiteral& lambda, const std::string& source) -> bool {
            const auto& [lambda_params, lambda_body] = lambda;
            m_accessing_property = false;

            // 1. Begin lambda code emission, but let's note that this nested "code scope" place a new buffer as the currently filling one before it's done & craps out a Lambda object... Which gets moved into the bytecode `Program` later.
            m_local_maps.emplace_back(CodeGenScope {
                .locals = {},
                .next_local_id = 0
            });
            m_code_blobs.emplace_front();

            for (const auto& param_token : lambda_params) {
                std::string param_name = param_token.as_string(source);

                if (!record_valued_symbol(param_name, RecordLocalOpt {})) {
                    return false;
                }
            }

            if (!emit_stmt(*lambda_body, source)) {
                return false;
            }

            // 2. Record the Lambda's code as a wrapper object into the VM heap, preloaded.
            const int16_t next_global_ref_const_id = m_consts.size();
            Arg next_global_ref_loc {
                .n = next_global_ref_const_id,
                .tag = Location::constant
            };

            Lambda temp_callable {std::move(m_code_blobs.front())};
            m_code_blobs.pop_front();
            m_local_maps.pop_back();

            if (auto lambda_object_ptr = m_heap.add_item(m_heap.get_next_id(), std::move(temp_callable)); lambda_object_ptr) {
                // As per any DerkJS object, the real thing is owned by the VM heap but is referenced by many non-owning raw pointers to its base: `ObjectBase<Value>*`.
                m_consts.emplace_back(Value {lambda_object_ptr});
            } else {
                return false;
            }

            encode_instruction(
                Opcode::djs_put_const,
                next_global_ref_loc
            );

            return true;
        }

        [[nodiscard]] auto emit_unary(const Unary& expr, const std::string& source) -> bool {
            struct OpcodeWithGenFlag {
                Opcode op;
                bool inner_ok;
            };

            const auto& [inner_expr, expr_op] = expr;
            m_accessing_property = false;

            const auto opcode_and_check = ([&, this]() -> std::optional<OpcodeWithGenFlag> {
                switch (expr_op) {
                    case AstOp::ast_op_bang:
                        return OpcodeWithGenFlag {
                            .op = Opcode::djs_test_falsy,
                            .inner_ok = emit_expr(*inner_expr, source)
                        };
                    case AstOp::ast_op_plus:
                        return OpcodeWithGenFlag {
                            .op = Opcode::djs_numify,
                            .inner_ok = emit_expr(*inner_expr, source)
                        };
                    case AstOp::ast_op_new: {
                        m_has_new_applied = true;

                        /// NOTE: Only ctor calls are done for new exprs e.g `new Foo(1, 2, 3);`
                        const auto within_new_expr_ok = emit_expr(*inner_expr, source);

                        m_has_new_applied = false;

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
                encode_instruction(opcode);
            }

            return true;
        }

        [[nodiscard]] auto emit_logical_expr(AstOp logical_operator, const ExprPtr& lhs, const ExprPtr& rhs, const std::string& source) -> bool {
            /// NOTE: No matter the logical operator, only one temporary remains but by differing rules:
            // 1. AND: IFF LHS is falsy, it's the result. RHS otherwise.
            // 2. OR: IFF LHS is truthy, it's the result. RHS otherwise.
            m_accessing_property = false;

            // 1. Emit LHS evaluation
            if (!emit_expr(*lhs, source)) {
                return false;
            }

            // 2. Emit dud conditional jump for the "OR" / "AND"
            const int pre_logic_check_pos = m_code_blobs.front().size();

            if (logical_operator == AstOp::ast_op_and) {
                encode_instruction(
                    Opcode::djs_jump_else,
                    /// NOTE: backpatch relative jump offset next
                    Arg {.n = -1, .tag = Location::immediate}
                );
            } else if (logical_operator == AstOp::ast_op_or) {
                encode_instruction(
                    Opcode::djs_jump_if,
                    Arg {.n = -1, .tag = Location::immediate}
                );
            }

            // 3. Emit RHS evaluation
            if (!emit_expr(*rhs, source)) {
                return false;
            }

            const int post_logic_check_pos = m_code_blobs.front().size();
            const auto patch_jump_offset = post_logic_check_pos - pre_logic_check_pos;

            encode_instruction(Opcode::djs_nop, {});

            // 4. Patch conditional jump offset which is taken on short-circuited evaluation.
            m_code_blobs.front().at(pre_logic_check_pos).args[0] = patch_jump_offset;

            return true;
        }

        [[nodiscard]] auto emit_member_access(const MemberAccess& member_access, const std::string& source) -> bool {
            const auto& [target_expr, key_expr] = member_access;

            // 1. Emit the target and then the key evaluations. The `djs_get_prop` opcode takes the target and the property key (PropertyHandle<Value>) on top... These two are "popped" and the property reference takes their place in the VM. If the object has calling property, the target object is emitted twice in case of passing 'THIS'.

            m_accessing_property = true;

            // For possible this object on member calls OR the parent object reference itself...
            if (!emit_expr(*target_expr, source)) {
                return false;
            }
            
            // If a property call is enclosing the member access: The callee's parent object is over the copied reference for `this`...
            if (m_has_call) {
                encode_instruction(Opcode::djs_dup);
            }

            if (!emit_expr(*key_expr, source)) {
                return false;
            }

            encode_instruction(Opcode::djs_get_prop);

            // NOTE: IF !m_access_as_lval, the member access dereferences the property reference for a usable temporary: `foo.a (ref Value(1)) + foo.b (ref Value(2))` -> `1 + 2 == 3`
            if (!m_access_as_lval) {
                encode_instruction(Opcode::djs_deref);
            }

            return true;
        }

        [[nodiscard]] auto emit_binary(const Binary& expr, const std::string& source) -> bool {
            const auto& [expr_lhs, expr_rhs, expr_op] = expr;
            m_accessing_property = false;

            // Case 1: emit logical operator expressions if applicable since these have special short-circuiting vs. arithmetic ones...
            if (expr_op == AstOp::ast_op_and || expr_op == AstOp::ast_op_or) {
                return emit_logical_expr(expr_op, expr_lhs, expr_rhs, source);
            }

            // Case 2 (General): arithmtic operators MUST evaluate RHS before LHS in case one operator is right-associative. The only other left-associative ones supported now are commutative: ADD, MUL, MOD...
            if (!emit_expr(*expr_rhs, source)) {
                return false;
            }

            if (!emit_expr(*expr_lhs, source)) {
                return false;
            }

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

            if (m_has_string_ops && expr_op == AstOp::ast_op_plus) {
                encode_instruction(Opcode::djs_strcat);
            } else {
                encode_instruction(*deduced_opcode);
            }

            return true;
        }

        [[nodiscard]] auto emit_assign(const Assign& expr, const std::string& source) -> bool {
            /// NOTE: JS "lvalues" are assignable, so set this flag accordingly for codegen.
            const auto& [lhs_expr, rhs_expr] = expr;
            m_accessing_property = false;
            
            /// NOTE: Only members and local names (non-duped slots) are assignable. This easily prevents stack misalignments by 1.
            if (!std::holds_alternative<Primitive>(lhs_expr->data) && !std::holds_alternative<MemberAccess>(lhs_expr->data)) {
                return false;
            }

            m_access_as_lval = true;

            if (!emit_expr(*lhs_expr, source)) {
                return false;
            }

            m_access_as_lval = false;

            if (!emit_expr(*rhs_expr, source)) {
                return false;
            }

            encode_instruction(Opcode::djs_emplace);

            return true;
        }

        [[nodiscard]] auto emit_call(const Call& expr, const std::string& source) -> bool {
            const auto& [expr_args, expr_callee] = expr;
            auto call_argc = 0;
 
            for (const auto& arg_p : expr_args) {
                if (!emit_expr(*arg_p, source)) {
                    return false;
                }

                ++call_argc;
            }
            
            m_has_call = true;
            m_access_as_lval = true;

            if (!emit_expr(*expr_callee, source)) {
                return false;
            }

            auto has_extra_this_arg = m_has_call && std::holds_alternative<MemberAccess>(expr_callee->data);

            m_access_as_lval = false;

            if (m_immediate_inline_fn_id != -1) {
                encode_instruction(
                    Opcode::djs_call,
                    Arg {.n = static_cast<int16_t>(m_immediate_inline_fn_id), .tag = Location::code_chunk},
                    Arg {.n = static_cast<int16_t>(call_argc), .tag = Location::immediate}
                );
            } else if (!m_has_new_applied) {
                encode_instruction(
                    Opcode::djs_object_call,
                    Arg {.n = static_cast<int16_t>(call_argc), .tag = Location::immediate},
                    Arg {.n = static_cast<int16_t>(has_extra_this_arg), .tag = Location::immediate}
                );
            } else {
                encode_instruction(
                    Opcode::djs_ctor_call,
                    Arg {.n = static_cast<int16_t>(call_argc), .tag = Location::immediate}
                );
            }

            m_has_call = false;

            return true;
        }

        [[nodiscard]] auto emit_expr(const Expr& expr, const std::string& source) -> bool {
            if (auto primitive_p = std::get_if<Primitive>(&expr.data); primitive_p) {
                return emit_primitive(*primitive_p, source);
            } else if (auto object_p = std::get_if<ObjectLiteral>(&expr.data); object_p) {
                /// TODO: maybe add opcode support for object cloning... could be good for instances of prototypes.
                return emit_object_literal(*object_p, source);
            } else if (auto lambda_p = std::get_if<LambdaLiteral>(&expr.data); lambda_p) {
                return emit_lambda(*lambda_p, source);
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

            return false;
        }

        [[nodiscard]] auto emit_expr_stmt(const ExprStmt& stmt, const std::string& source) -> bool {
            const auto& [wrapped_expr] = stmt;
            return emit_expr(*wrapped_expr, source);
        }

        [[nodiscard]] auto emit_var_decl(const VarDecl& stmt, const std::string& source) -> bool {
            const auto& [var_name_token, var_init_expr] = stmt;
            std::string var_name = var_name_token.as_string(source);
            
            if (m_local_maps.back().locals.contains(var_name)) {
                return false;
            }

            auto var_local_slot = record_valued_symbol(var_name, RecordLocalOpt {});

            if (!emit_expr(*var_init_expr, source)) {
                return false;
            }

            if (m_local_maps.size() < 2) {
                m_globals_map[var_name] = *var_local_slot;
                return true;
            } else {
                m_local_maps.back().locals[var_name] = *var_local_slot;
                return true;
            }

            return false;
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
            const auto& [check, truthy_body, falsy_body] = stmt_if;

            if (!emit_expr(*check, source)) {
                return false;
            }

            const int pre_if_jump_pos = m_code_blobs.front().size();
            encode_instruction(
                Opcode::djs_jump_else,
                /// NOTE: backpatch this jump afterward.
                Arg {.n = -1, .tag = Location::immediate},
                /// NOTE: pop off that one collapsed slot with the condition's temporary
                Arg {.n = 1, .tag = Location::immediate}
            );

            if (!emit_stmt(*truthy_body, source)) {
                return false;
            }

            if (!falsy_body) {
                int post_lone_if_jump_pos = m_code_blobs.front().size();
                encode_instruction(Opcode::djs_nop, {});

                m_code_blobs.front().at(pre_if_jump_pos).args[0] = post_lone_if_jump_pos - pre_if_jump_pos;

                return true;
            }

            int skip_else_jump_pos = m_code_blobs.front().size();
            encode_instruction(Opcode::djs_jump, Arg {.n = -1, .tag = Location::immediate});

            int to_else_jump_pos = m_code_blobs.front().size();
            m_code_blobs.front().at(pre_if_jump_pos).args[0] = to_else_jump_pos - pre_if_jump_pos;

            if (!emit_stmt(*falsy_body, source)) {
                return false;
            }

            const int post_else_jump_pos = m_code_blobs.front().size();
            m_code_blobs.front().at(skip_else_jump_pos).args[0] = post_else_jump_pos - skip_else_jump_pos;

            return true;
        }

        [[nodiscard]] auto emit_return(const Return& stmt, const std::string& source) -> bool {
            const auto& [result_expr] = stmt;

            m_access_as_lval = false;

            if (!emit_expr(*result_expr, source)) {
                return false;
            }

            encode_instruction(Opcode::djs_ret);

            return true;
        }

        [[nodiscard]] auto emit_while(const While& loop_by_while, const std::string& source) -> bool {
            const auto& [loop_check, loop_body] = loop_by_while;
            const int loop_begin_pos = m_code_blobs.front().size();

            if (!emit_expr(*loop_check, source)) {
                return false;
            }

            const int loop_exit_jump_pos = m_code_blobs.front().size();
            encode_instruction(Opcode::djs_jump_else, Arg {.n = -1, .tag = Location::immediate}, Arg {.n = 1, .tag = Location::immediate});

            if (!emit_stmt(*loop_body, source)) {
                return false;
            }

            const int loop_repeat_jump_pos = m_code_blobs.front().size();
            encode_instruction(Opcode::djs_jump, Arg {.n = -1, .tag = Location::immediate});

            m_code_blobs.front().at(loop_exit_jump_pos).args[0] = 1 + loop_repeat_jump_pos - loop_exit_jump_pos;
            m_code_blobs.front().at(loop_repeat_jump_pos).args[0] = loop_begin_pos - loop_repeat_jump_pos;

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
            const auto& [fn_params, fn_name, fn_body] = stmt;

            m_local_maps.emplace_back(CodeGenScope {
                .locals = {},
                .next_local_id = 0
            });

            if (record_function_begin(fn_name.as_string(source)) == -1) {
                return false;
            }

            for (const auto& param_token : fn_params) {
                std::string param_name = param_token.as_string(source);

                if (!record_valued_symbol(param_name, RecordLocalOpt {})) {
                    return false;
                }
            }

            if (!emit_stmt(*fn_body, source)) {
                return false;
            }

            m_local_maps.pop_back();

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
        BytecodeGenPass(std::vector<PreloadItem> preloadables, int heap_object_capacity)
        : m_global_consts_map {}, m_globals_map {}, m_local_maps {}, m_heap {heap_object_capacity}, m_consts {}, m_code_blobs {}, m_chunk_offsets {}, m_immediate_inline_fn_id {-1}, m_has_string_ops {false}, m_has_new_applied {false}, m_access_as_lval {false}, m_accessing_property {false}, m_has_call {false} {
            m_local_maps.emplace_back(CodeGenScope {
                .locals = {},
                .next_local_id = 0
            });
            m_code_blobs.emplace_front();

            for (auto& [pre_name, pre_entity, pre_location] : preloadables) {
                switch (pre_location) {
                case Location::constant: {
                    record_valued_symbol(pre_name, std::move(std::get<Value>(pre_entity)));
                } break;
                case Location::heap_obj: {
                    auto& js_object_p = std::get<std::unique_ptr<ObjectBase<Value>>>(pre_entity);
                    if (pre_name.empty()) {
                        m_heap.add_item(m_heap.get_next_id(), std::move(js_object_p));
                    } else {
                        record_valued_symbol(pre_name, std::move(js_object_p));
                    }
                } break;
                default: break;
                }
            }
        }

        [[nodiscard]] auto operator()([[maybe_unused]] const ASTUnit& tu, [[maybe_unused]] const std::vector<std::string>& source_map) -> std::optional<Program> {
            /// TODO: 1. emit all function decls FIRST as per JS function hoisting.
            for (const auto& [src_filename, decl, src_id] : tu) {
                if (std::holds_alternative<FunctionDecl>(decl->data)) {
                    if (!emit_stmt(*decl, source_map.at(src_id))) {
                        std::println(std::cerr, "Compile Error at source '{}' around '{}': unsupported JS construct :(\n", src_filename, source_map.at(src_id).substr(decl->text_begin, decl->text_length));
                        return {};
                    }
                }
            }

            /// TODO: 2. emit all top-level non-function statements as an implicit function that's called right away.
            auto global_func_id = record_function_begin("__js_global__");

            for (const auto& [src_filename, decl, src_id] : tu) {
                if (!std::holds_alternative<FunctionDecl>(decl->data)) {
                    if (!emit_stmt(*decl, source_map.at(src_id))) {
                        std::println(std::cerr, "Compile Error at source '{}' around '{}': unsupported JS construct :(\n", src_filename, source_map.at(src_id).substr(decl->text_begin, decl->text_length));
                        return {};
                    }
                }
            }

            /// NOTE: Place dud offset marker for bytecode dumping to properly end.
            m_chunk_offsets.emplace_back(-1);

            std::vector<Instruction> global_code_buffer {std::move(m_code_blobs.front())};
            m_code_blobs.pop_front();

            return Program {
                .heap_items = std::move(m_heap), // PolyPool<ObjectBase<Value>>
                .consts = std::move(m_consts), // std::vector<Value>
                .code = std::move(global_code_buffer), // std::vector<Instruction>
                .offsets = std::move(m_chunk_offsets), // std::vector<int>
                .entry_func_id = static_cast<int16_t>(global_func_id), // int
            };
        }
    };
}
