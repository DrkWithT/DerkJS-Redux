module;

#include <type_traits>
#include <utility>

#include <optional>
#include <string>
#include <forward_list>
#include <array>
#include <vector>
#include <variant>
#include <flat_map>
#include <iostream>
#include <print>

export module backend.bc_generate;

import frontend.ast;
import runtime.strings;
import runtime.callables;
import runtime.value;
import runtime.bytecode;

export namespace DerkJS {
    struct PreloadItem {
        std::string lexeme; // empty strings are for hidden items e.g function object in console.log
        std::variant<std::unique_ptr<ObjectBase<Value>>, Value> entity;
        Location location;
    };

    struct FindGlobalConstsOpt {};
    struct FindKeyConstOpt {};
    struct FindLocalsOpt {};
    struct FindCaptureOpt {};

    struct RecordLocalOpt {};
    struct RecordSpecialThisOpt {};

    struct OpcodeDelta {
        int offset;      // opcode's magnitude of RSP increment or decrement
        bool implicit;   // if opcode's effect on RSP follows delta / outside calculation
    };

    struct CodeGenScope {
        std::flat_map<std::string, Arg> locals; // Map names to variables.
        std::vector<int> callee_self_refs;      // Positions of self-calls by name.
        int next_local_id;                      // Tracks next stack slot for a local variable value.
    };

    struct ActiveLoop {
        std::vector<int> exits; // locations of any immediate, break jumps
        std::vector<int> repeats; // locations of any immediate, continue jumps
    };

    class BytecodeGenPass {
    private:
        static constexpr Arg dud_arg {
            .n = 0,
            .tag = Location::immediate
        };

        
        // 1st priority lookup: maps primitive constants / native object names of top-level
        std::flat_map<std::string, Arg> m_global_consts_map;
        
        // 2nd priority lookup: maps property names and outside variable names
        std::flat_map<std::string, Arg> m_key_consts_map;
        
        // Pre-Tracked prototypes:
        std::array<ObjectBase<Value>*, static_cast<std::size_t>(BasePrototypeID::last)> m_base_prototypes;

        // 3rd priority lookup: maps local variable names to locations
        std::vector<CodeGenScope> m_local_maps;

        // filled with interned strings -> object-refs in consts -> stack temporaries
        PolyPool<ObjectBase<Value>> m_heap;

        // filled with primitive literals & interned string refs
        std::vector<Value> m_consts;

        std::forward_list<ActiveLoop> m_active_loops;

        // stack of bytecode buffers, accounting for arbitrary nesting of lambdas
        std::forward_list<std::vector<Instruction>> m_code_blobs;

        // Track currently emitting function by name & find any self-references here.
        std::string m_callee_name;

        // filled with global function IDs -> absolute offsets into the bytecode blob
        std::vector<int> m_chunk_offsets;

        /// SEE: `m_callee_name` is associated with this field.
        int m_callee_lambda_id;

        int m_member_depth;

        // Whether an addition has any string operands or not.
        bool m_has_string_ops;

        // Whether the parent expression is something like `new Foo(...)`.
        bool m_has_new_applied;

        // Whether an object member access is assignable or read-from.
        bool m_access_as_lval;

        // Whether an object's property is being accessed from the parent.
        bool m_accessing_property;

        bool m_has_call;

        // Whether to emit any `var a = ...;` declaration as an undefined stub first. Otherwise, the `var a = ...;` declarations are treated as assignments.
        bool m_prepass_vars;

        // Overload for lookup of globals before locals
        [[nodiscard]] auto lookup_symbol(const std::string& symbol) -> std::optional<Arg> {
            return lookup_symbol(symbol, FindGlobalConstsOpt {})
                .or_else([&symbol, this]() { return lookup_symbol(symbol, FindLocalsOpt {}); });
        }

        // Overload for lookup of global constants only
        [[nodiscard]] auto lookup_symbol(const std::string& symbol, [[maybe_unused]] FindGlobalConstsOpt opt) -> std::optional<Arg> {
            if (auto global_consts_opt = m_global_consts_map.find(symbol); global_consts_opt != m_global_consts_map.end()) {
                return global_consts_opt->second;
            }

            return {};
        }

        // Overload for lookup of property name strings only
        [[nodiscard]] auto lookup_symbol(const std::string& symbol, [[maybe_unused]] FindKeyConstOpt opt) -> std::optional<Arg> {
            if (auto global_key_str_opt = m_key_consts_map.find(symbol); global_key_str_opt != m_key_consts_map.end()) {
                return global_key_str_opt->second;
            }

            return {};
        }

        // Overload for lookup of locals only
        [[nodiscard]] auto lookup_symbol(const std::string& symbol, [[maybe_unused]] FindLocalsOpt opt) -> std::optional<Arg> {
            if (auto fn_local_opt = m_local_maps.back().locals.find(symbol); fn_local_opt != m_local_maps.back().locals.end()) {
                return fn_local_opt->second;
            }

            return {};
        }

        [[nodiscard]] auto lookup_symbol(const std::string& symbol, [[maybe_unused]] FindCaptureOpt opt) -> std::optional<Arg> {
            if (auto fn_local_opt = m_local_maps.back().locals.find(symbol); fn_local_opt != m_local_maps.back().locals.end()) {
                return Arg {
                    .n = fn_local_opt->second.n,
                    .tag = fn_local_opt->second.tag,
                    .is_str_literal = fn_local_opt->second.is_str_literal,
                    .from_closure = true
                };
            }

            return {};
        }

        template <typename Item, typename RecordOpt>
        [[maybe_unused]] auto record_symbol(const std::string& symbol, Item&& item, [[maybe_unused]] RecordOpt opt) -> std::optional<Arg> {
            using plain_item_type = std::remove_cvref_t<Item>;

            if (auto existing_symbol_mapping = lookup_symbol(symbol, opt); existing_symbol_mapping) {
                return existing_symbol_mapping;
            }

            if constexpr (std::is_same_v<plain_item_type, Value> && std::is_same_v<RecordOpt, FindGlobalConstsOpt>) {
                // 1a. global primitive case
                const int16_t next_global_const_id = m_consts.size();
                Arg next_global_loc {
                    .n = next_global_const_id,
                    .tag = Location::constant,
                    .is_str_literal = false,
                    .from_closure = false
                };

                m_consts.emplace_back(std::forward<Item>(item));
                m_global_consts_map[symbol] = next_global_loc;

                return next_global_loc;
            } else if constexpr (std::is_same_v<plain_item_type, std::string> && std::is_same_v<RecordOpt, FindGlobalConstsOpt>) {
                // 2. global string literal case
                const int16_t next_global_ref_const_id = m_consts.size();
                Arg next_global_ref_loc {
                    .n = next_global_ref_const_id,
                    .tag = Location::constant,
                    .is_str_literal = true,
                    .from_closure = false,
                };

                auto string_prototype_const_id = lookup_symbol("String::prototype", FindGlobalConstsOpt {})->n;

                if (auto heap_dyn_str_p = m_heap.add_item(m_heap.get_next_id(), std::make_unique<DynamicString>(
                    m_consts.at(string_prototype_const_id).to_object(),
                    std::forward<Item>(item)
                )); heap_dyn_str_p) {
                    m_consts.emplace_back(Value {heap_dyn_str_p});
                    m_global_consts_map[symbol] = next_global_ref_loc;
                    m_heap.update_tenure_count();
                } else {
                    return {};
                }

                return next_global_ref_loc;
            } else if constexpr (std::is_same_v<plain_item_type, std::string> && std::is_same_v<RecordOpt, FindKeyConstOpt>) {
                // 3. property key string case
                const int16_t next_global_ref_const_id = m_consts.size();
                Arg next_global_ref_loc {
                    .n = next_global_ref_const_id,
                    .tag = Location::constant,
                    .is_str_literal = true,
                    .from_closure = false,
                };

                auto string_prototype_const_id = lookup_symbol("String", FindGlobalConstsOpt {})->n;

                if (auto heap_dyn_str_p = m_heap.add_item(m_heap.get_next_id(), std::make_unique<DynamicString>(
                    m_consts.at(string_prototype_const_id).to_object(),
                    std::forward<Item>(item)
                )); heap_dyn_str_p) {
                    m_consts.emplace_back(Value {heap_dyn_str_p});
                    m_key_consts_map[symbol] = next_global_ref_loc;
                    m_heap.update_tenure_count();
                } else {
                    return {};
                }

                return next_global_ref_loc;
            } else if constexpr (std::is_same_v<plain_item_type, std::string> && std::is_same_v<RecordOpt, FindCaptureOpt>) {
                // function captured variable-name case
                const int16_t next_global_ref_const_id = m_consts.size();
                auto string_prototype_const_id = lookup_symbol("String", FindGlobalConstsOpt {})->n;

                if (auto existing_capture_key_loc = lookup_symbol(symbol, FindKeyConstOpt {}); existing_capture_key_loc) {
                    existing_capture_key_loc->from_closure = true;
                    return existing_capture_key_loc;
                } else if (auto heap_dyn_str_p = m_heap.add_item(m_heap.get_next_id(), std::make_unique<DynamicString>(
                    m_consts.at(string_prototype_const_id).to_object(),
                    std::forward<Item>(item)
                )); heap_dyn_str_p) {
                    m_consts.emplace_back(Value {heap_dyn_str_p});
                    m_key_consts_map[symbol] = Arg {
                        .n = next_global_ref_const_id,
                        .tag = Location::constant,
                        .is_str_literal = true,
                        .from_closure = false, // the variable name may also mirror a string literal, so keep the actual symbol's flag false!
                    };
                    m_heap.update_tenure_count();

                    auto temp_as_capture_key = m_key_consts_map.at(symbol);
                    temp_as_capture_key.from_closure = true;

                    return temp_as_capture_key;
                }

                return {};
            } else if constexpr (std::is_same_v<plain_item_type, RecordLocalOpt> && std::is_same_v<RecordOpt, FindLocalsOpt>) {
                // 4. local variable name case
                const int16_t next_local_id = m_local_maps.back().next_local_id;
                Arg next_local_loc {
                    .n = next_local_id,
                    .tag = Location::local
                };

                m_local_maps.back().locals[symbol] = next_local_loc;
                ++m_local_maps.back().next_local_id;

                return next_local_loc;
            }

            return {};
        }

        [[maybe_unused]] auto pre_record_object(const std::string& symbol, std::unique_ptr<ObjectBase<Value>> object_p) -> bool {
            // 1a, 1b. For global & interned string references as constants:
            const int16_t next_global_ref_const_id = m_consts.size();
            Arg next_global_ref_loc {
                .n = next_global_ref_const_id,
                .tag = Location::constant
            };

            if (auto heap_native_object_p = m_heap.add_item(m_heap.get_next_id(), std::move(object_p)); heap_native_object_p) {
                m_consts.emplace_back(Value {heap_native_object_p});
                m_global_consts_map[symbol] = next_global_ref_loc;
                m_heap.update_tenure_count();

                if (symbol == "Boolean::prototype") {
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::boolean)] = heap_object_p;
                } else if (symbol == "Number::prototype") {
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::number)] = heap_object_p;
                } else if (symbol == "String::prototype") {
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::str)] = heap_object_p;
                } else if (symbol == "Object::prototype") {
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::object)] = heap_object_p;
                } else if (symbol == "Array::prototype") {
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::array)] = heap_object_p;
                } else if (symbol == "Function::prototype") {
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::function)] = heap_object_p;
                } else {
                    return false;
                }

                return true;
            }

            return false;
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
            const auto& [pmt_token, pmt_is_key] = expr;
            std::string atom_lexeme = pmt_token.as_string(source);
            const TokenTag expr_token_tag = pmt_token.tag;

            auto primitive_locator = ([&, this] () -> std::optional<Arg> {
                switch (expr_token_tag) {
                case TokenTag::keyword_this:
                    m_has_string_ops = false;
                    return Arg { .n = -2, .tag = Location::heap_obj, .is_str_literal = false, .from_closure = false };
                case TokenTag::keyword_undefined:
                    m_has_string_ops = false;
                    m_accessing_property = false;
                    return lookup_symbol("undefined", FindGlobalConstsOpt {});
                case TokenTag::keyword_null:
                    m_has_string_ops = false;
                    m_accessing_property = false;
                    return lookup_symbol("null", FindGlobalConstsOpt {});
                case TokenTag::keyword_true: case TokenTag::keyword_false:
                    m_has_string_ops = false;
                    m_accessing_property = false;
                    return lookup_symbol(atom_lexeme, FindGlobalConstsOpt {});
                case TokenTag::literal_int:
                    m_has_string_ops = false;
                    m_accessing_property = false;
                    return record_symbol(atom_lexeme, Value {std::stoi(atom_lexeme)}, FindGlobalConstsOpt {});
                case TokenTag::literal_real:
                    m_has_string_ops = false;
                    m_accessing_property = false;
                    return record_symbol(atom_lexeme, Value {std::stod(atom_lexeme)}, FindGlobalConstsOpt {});
                case TokenTag::literal_string: {
                    m_has_string_ops = true;
                    return record_symbol(atom_lexeme, atom_lexeme, FindGlobalConstsOpt {});
                }
                case TokenTag::keyword_prototype: {
                    m_has_string_ops = false;
                    return Arg { .n = -3, .tag = Location::heap_obj, .is_str_literal = false, .from_closure = false };
                }
                case TokenTag::identifier: {
                    m_has_string_ops = false;

                    // Case 1: property keys are always constant strings.
                    if (m_callee_name == atom_lexeme && !m_accessing_property &&  m_has_call) {
                        return Arg {.n = -1, .tag = Location::end, .is_str_literal = false, .from_closure = false};
                    } else if (pmt_is_key) {
                        return record_symbol(atom_lexeme, atom_lexeme, FindKeyConstOpt {});
                    } else {
                        // Case 2.1: The name is for a global native / local variable.
                        return lookup_symbol(atom_lexeme)
                        // Case 2.2: The name is a non-local variable BUT non-global.
                        .or_else([&atom_lexeme, this] () {
                            return record_symbol(atom_lexeme, atom_lexeme, FindCaptureOpt {});
                        });
                    }
                }
                default: return {};
                }
            })();

            if (!primitive_locator) {
                return false;
            } else if (const auto primitive_locate_type = primitive_locator->tag; primitive_locate_type == Location::constant) {
                encode_instruction(Opcode::djs_put_const, *primitive_locator);

                if (primitive_locator->from_closure) {
                    encode_instruction(Opcode::djs_ref_upval);

                    if (!m_access_as_lval) {
                        encode_instruction(Opcode::djs_deref);
                    }
                }
            } else if (const auto local_var_opcode = (m_access_as_lval && !m_accessing_property) ? Opcode::djs_ref_local : Opcode::djs_dup_local; primitive_locate_type == Location::local) {
                encode_instruction(local_var_opcode, *primitive_locator);
            } else if (primitive_locate_type == Location::heap_obj && primitive_locator->n == -2) {
                encode_instruction(Opcode::djs_put_this);
            } else if (primitive_locate_type == Location::heap_obj && primitive_locator->n == -3) {
                encode_instruction(Opcode::djs_put_proto_key);
            } else if (primitive_locate_type == Location::end && primitive_locator->n == -1) {
                // Put dud instruction where self-load should be before a `djs_object_call`.
                m_local_maps.back().callee_self_refs.emplace_back(static_cast<int>(m_code_blobs.front().size()));
                encode_instruction(Opcode::djs_nop);
            }

            return true;
        }

        [[nodiscard]] auto emit_object_literal(const ObjectLiteral& object, const std::string& source) -> bool {
            encode_instruction(Opcode::djs_put_obj_dud);
            m_accessing_property = false;

            for (const auto& [prop_name_token, prop_init_expr] : object.fields) {
                std::string prop_name = prop_name_token.as_string(source);

                auto prop_name_locator = record_symbol(prop_name, prop_name, FindKeyConstOpt {});

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

        [[nodiscard]] auto emit_array_literal(const ArrayLiteral& array, const std::string& source) -> bool {
            const auto& [items] = array;

            // 1. Push each JS array item via evaluation.
            int item_count = 0;

            for (const auto& item_expr : items) {
                if (!emit_expr(*item_expr, source)) {
                    return false;
                }

                ++item_count;
            }

            // 2. Invoke this special opcode. Now there's a new array for use. :)
            encode_instruction(
                Opcode::djs_put_arr_dud,
                Arg {.n = static_cast<int16_t>(item_count), .tag = Location::immediate}
            );

            return true;
        }

        [[nodiscard]] auto emit_lambda(const LambdaLiteral& lambda, const std::string& source) -> bool {
            const auto& [lambda_params, lambda_body] = lambda;
            m_accessing_property = false;

            // 1. Begin lambda code emission, but let's note that this nested "code scope" place a new buffer as the currently filling one before it's done & craps out a Lambda object... Which gets moved into the bytecode `Program` later.
            m_local_maps.emplace_back(CodeGenScope {
                .locals = {},
                .callee_self_refs = {},
                .next_local_id = 0
            });
            m_code_blobs.emplace_front();

            for (const auto& param_token : lambda_params) {
                std::string param_name = param_token.as_string(source);

                if (!record_symbol(param_name, RecordLocalOpt {}, FindLocalsOpt {})) {
                    return false;
                }
            }

            const auto old_prepass_vars_flag = m_prepass_vars;
            m_prepass_vars = true;

            if (!emit_stmt(*lambda_body, source)) {
                return false;
            }

            m_prepass_vars = false;

            if (!emit_stmt(*lambda_body, source)) {
                return false;
            }

            // 2. Record the Lambda's code as a wrapper object into the VM heap, preloaded.
            const int16_t next_global_ref_const_id = m_consts.size();
            Arg next_global_ref_loc {
                .n = next_global_ref_const_id,
                .tag = Location::constant
            };

            // 3. Patch bytecode NOP stubs where self-calls should be.
            for (const auto& dud_self_call_offsets = m_local_maps.back().callee_self_refs; const auto call_stub_pos : dud_self_call_offsets) {
                m_code_blobs.front().at(call_stub_pos) = Instruction {
                    .args = {next_global_ref_const_id, 0},
                    .op = Opcode::djs_put_const
                };
            }

            Lambda temp_callable {
                std::move(m_code_blobs.front()),
                m_heap.add_item(m_heap.get_next_id(), std::make_unique<Object>(nullptr, Object::flag_prototype_v))
            };
            
            if (auto lambda_object_ptr = m_heap.add_item(m_heap.get_next_id(), std::move(temp_callable)); lambda_object_ptr) {
                // As per any DerkJS object, the real thing is owned by the VM heap but is referenced by many non-owning raw pointers to its base: `ObjectBase<Value>*`.
                m_consts.emplace_back(Value {lambda_object_ptr});
            } else {
                return false;
            }

            m_code_blobs.pop_front();
            m_local_maps.pop_back();
            m_prepass_vars = old_prepass_vars_flag;
            m_callee_name.clear();

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

            // 1. Emit the target and then the key evaluations. The `djs_get_prop` opcode takes the target and the property key (PropertyHandle<Value>) on top... These two are "popped" and the property reference takes their place in the VM. If the top callee expr has a calling property, the target object is emitted again (member-access-depth == 1).
            m_accessing_property = true;
            m_member_depth++;

            // For possible this object on member calls OR the parent object reference itself...
            if (!emit_expr(*target_expr, source)) {
                return false;
            }

            // If a property call is enclosing the member access: The callee's parent object is over the copied reference for `this`...
            if (m_has_call && m_member_depth <= 1) {
                encode_instruction(Opcode::djs_dup);
            }

            if (!emit_expr(*key_expr, source)) {
                return false;
            }

            m_member_depth--;

            /// NOTE: If assignment (lvalues apply) pass `1` for defaulting flag so the RHS goes somewhere legitimate.
            encode_instruction(Opcode::djs_get_prop, Arg {.n = static_cast<int16_t>(!m_has_call && m_access_as_lval), .tag = Location::immediate});

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
            m_member_depth = 0;

            if (!emit_expr(*expr_lhs, source)) {
                return false;
            }
            m_member_depth = 0;

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
            m_member_depth = 0;

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
            m_member_depth = 0;

            if (!m_has_new_applied) {
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
                m_has_new_applied = false;
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
            } else if (auto array_p = std::get_if<ArrayLiteral>(&expr.data); array_p) {
                return emit_array_literal(*array_p, source);
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
            if (m_prepass_vars) {
                return true;
            }

            const auto& [wrapped_expr] = stmt;
            return emit_expr(*wrapped_expr, source);
        }

        [[nodiscard]] auto emit_var_decl(const VarDecl& stmt, const std::string& source) -> bool {
            const auto& [var_name_token, var_init_expr] = stmt;
            std::string var_name = var_name_token.as_string(source);
            m_callee_name = var_name;

            auto var_local_slot = record_symbol(var_name, RecordLocalOpt {}, FindLocalsOpt {});
            
            // 1. When hoisting the var declaration, just set it to undefined 1st.
            if (const auto placeholder_undefined = lookup_symbol("undefined", FindGlobalConstsOpt {}).value(); m_prepass_vars) {
                encode_instruction(Opcode::djs_put_const, placeholder_undefined);
                m_local_maps.back().locals[var_name] = *var_local_slot;
                return true;
            }

            // 2. In the 2nd pass, define the filler var in function scope. Although this is a var decl, it's been defined as undefined before, so treat the initialization as a var-assignment.
            encode_instruction(Opcode::djs_ref_local, *var_local_slot);

            if (!emit_expr(*var_init_expr, source)) {
                return false;
            }

            // 2b. Capture variable in environment in case its used as a closure.
            encode_instruction(Opcode::djs_put_const, record_symbol(var_name, var_name, FindKeyConstOpt {}).value()); // variable name is the capturing Object key!
            encode_instruction(Opcode::djs_store_upval);
            m_member_depth = 0;

            encode_instruction(Opcode::djs_emplace);

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
            const auto& [check, truthy_body, falsy_body] = stmt_if;

            /// NOTE: emit all var decls in these bodies
            if (m_prepass_vars) {
                if (!emit_stmt(*truthy_body, source)) {
                    return false;
                }

                if (falsy_body && !emit_stmt(*falsy_body, source)) {
                    return false;
                }

                return true;
            }

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
            if (m_prepass_vars) {
                return true;
            }

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

            if (m_prepass_vars) {
                if (!emit_stmt(*loop_body, source)) {
                    return false;
                }

                return true;
            }

            const int loop_begin_pos = m_code_blobs.front().size();

            m_active_loops.push_front(ActiveLoop {
                .exits = {},
                .repeats = {}
            });

            if (!emit_expr(*loop_check, source)) {
                return false;
            }

            const int loop_exit_jump_pos = m_code_blobs.front().size();
            encode_instruction(Opcode::djs_jump_else, Arg {.n = -1, .tag = Location::immediate}, Arg {.n = 1, .tag = Location::immediate});

            if (!emit_stmt(*loop_body, source)) {
                return false;
            }

            // 2. Patch main loop jumps!
            const int loop_repeat_jump_pos = m_code_blobs.front().size();
            encode_instruction(Opcode::djs_jump, Arg {.n = -1, .tag = Location::immediate});
            const int loop_exit_pos = loop_repeat_jump_pos + 1;

            m_code_blobs.front().at(loop_exit_jump_pos).args[0] = 1 + loop_repeat_jump_pos - loop_exit_jump_pos;
            m_code_blobs.front().at(loop_repeat_jump_pos).args[0] = loop_begin_pos - loop_repeat_jump_pos;

            // 3. Patch breaks, continues...
            const auto& [wloop_exits, wloop_repeats] = m_active_loops.front();

            for (auto wloop_break_pos : wloop_exits) {
                m_code_blobs.front().at(wloop_break_pos).args[0] = loop_exit_pos - wloop_break_pos;
            }

            for (auto wloop_repeat_pos : wloop_repeats) {
                m_code_blobs.front().at(wloop_repeat_pos).args[0] = loop_begin_pos - wloop_repeat_pos;
            }

            m_active_loops.pop_front();

            return true;
        }

        [[nodiscard]] auto emit_break() -> bool {
            if (m_prepass_vars) {
                return true;
            }

            const int current_code_pos = m_code_blobs.front().size();

            encode_instruction(Opcode::djs_jump, Arg {
                .n = -1,
                .tag = Location::immediate
            });
            m_active_loops.front().exits.push_back(current_code_pos);

            return true;
        }

        [[nodiscard]] auto emit_continue() -> bool {
            if (m_prepass_vars) {
                return true;
            }

            const int current_code_pos = m_code_blobs.front().size();

            encode_instruction(Opcode::djs_jump, Arg {
                .n = -1,
                .tag = Location::immediate
            });
            m_active_loops.front().repeats.push_back(current_code_pos);

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
            } else if (auto loop_break_p = std::get_if<Break>(&stmt.data); loop_break_p) {
                return emit_break();
            } else if (auto loop_continue_p = std::get_if<Continue>(&stmt.data); loop_continue_p) {
                return emit_continue();
            } else if (auto block_p = std::get_if<Block>(&stmt.data); block_p) {
                return emit_block(*block_p, source);
            }

            return false;
        }

    public:
        BytecodeGenPass(std::vector<PreloadItem> preloadables, int heap_object_capacity)
        : m_global_consts_map {}, m_key_consts_map {}, m_base_prototypes {}, m_local_maps {}, m_heap {heap_object_capacity}, m_consts {}, m_code_blobs {}, m_callee_name {}, m_chunk_offsets {}, m_callee_lambda_id {-1}, m_member_depth {0}, m_has_string_ops {false}, m_has_new_applied {false}, m_access_as_lval {false}, m_accessing_property {false}, m_has_call {false}, m_prepass_vars {true} {
            // 1. Record fundamental primitive constants once to avoid extra work.
            record_symbol("undefined", Value {}, FindGlobalConstsOpt {});
            record_symbol("null", Value {JSNullOpt {}}, FindGlobalConstsOpt {});
            record_symbol("true", Value {true}, FindGlobalConstsOpt {});
            record_symbol("false", Value {false}, FindGlobalConstsOpt {});

            // 2. Record global native objects to avoid extra hassle later.
            for (auto& [pre_name, pre_entity, pre_location] : preloadables) {
                switch (pre_location) {
                case Location::constant: {
                    record_symbol(pre_name, std::move(std::get<Value>(pre_entity)), FindGlobalConstsOpt {});
                } break;
                case Location::heap_obj: {
                    auto& js_object_p = std::get<std::unique_ptr<ObjectBase<Value>>>(pre_entity);
                    if (pre_name.empty()) {
                        m_heap.add_item(m_heap.get_next_id(), std::move(js_object_p));
                    } else {
                        pre_record_object(pre_name, std::move(js_object_p));
                    }

                    m_heap.update_tenure_count();
                } break;
                default: break;
                }
            }

            // 3. Prepare initial mapping of symbols & code buffer to build.
            m_local_maps.emplace_back(CodeGenScope {
                .locals = {},
                .next_local_id = 0
            });
            m_code_blobs.emplace_front();
        }

        [[nodiscard]] auto operator()(const ASTUnit& tu, const std::vector<std::string>& source_map) -> std::optional<Program> {
            /// 1. emit all top-level non-function statements as an implicit function that's called right away.
            constexpr auto global_func_id = 0; // implicit main function begins at offset 0
            m_chunk_offsets.emplace_back(0);

            /// 2. emit all vars (especially function declaration syntax sugar) FIRST as per JS hoisting.
            for (const auto& [src_filename, decl, src_id] : tu) {
                if (!emit_stmt(*decl, source_map.at(src_id))) {
                    std::println(std::cerr, "Compile Error at source '{}' around '{}': unsupported JS construct :(\n", src_filename, source_map.at(src_id).substr(decl->text_begin, decl->text_length));
                    return {};
                }
            }

            m_prepass_vars = false;

            for (const auto& [src_filename, decl, src_id] : tu) {
                if (!emit_stmt(*decl, source_map.at(src_id))) {
                    std::println(std::cerr, "Compile Error at source '{}' around '{}': unsupported JS construct :(\n", src_filename, source_map.at(src_id).substr(decl->text_begin, decl->text_length));
                    return {};
                }
            }

            /// NOTE: Place dud offset marker for bytecode dumping to properly end.
            m_chunk_offsets.emplace_back(-1);

            std::vector<Instruction> global_code_buffer {std::move(m_code_blobs.front())};
            m_code_blobs.pop_front();

            return Program {
                .heap_items = std::move(m_heap), // PolyPool<ObjectBase<Value>>
                .base_protos = std::move(m_base_prototypes),
                .consts = std::move(m_consts), // std::vector<Value>
                .code = std::move(global_code_buffer), // std::vector<Instruction>
                .offsets = std::move(m_chunk_offsets), // std::vector<int>
                .entry_func_id = static_cast<int16_t>(global_func_id), // int
            };
        }
    };
}
