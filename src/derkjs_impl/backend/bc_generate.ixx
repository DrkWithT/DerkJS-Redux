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

export import frontend.ast;
export import runtime.strings;
export import runtime.callables;
export import runtime.value;
export import runtime.bytecode;

namespace DerkJS::Backend {
    export struct PreloadItem {
        std::string lexeme; // empty strings are for hidden items e.g function object in console.log
        std::variant<std::unique_ptr<ObjectBase<Value>>, Value> entity;
        Location location;
    };

    export struct FindGlobalConstsOpt {};
    export struct FindKeyConstOpt {};
    export struct FindLocalsOpt {};
    export struct FindCaptureOpt {};

    export struct RecordLocalOpt {};
    export struct RecordSpecialThisOpt {};

    export struct CodeGenScope {
        std::flat_map<std::string, Arg> locals; // Map names to variables.
        std::vector<int> callee_self_refs;      // Positions of self-calls by name.
        int next_local_id;                      // Tracks next stack slot for a local variable value.
        int block_level;                        // Tracks depth of currently emitted block statement.
    };

    export struct ActiveLoop {
        std::vector<int> exits; // locations of any immediate, break jumps
        std::vector<int> repeats; // locations of any immediate, continue jumps
    };

    /// NOTE: Forward declaration of bytecode compiler state.
    export class BytecodeEmitterContext;

    /**
     * @brief Interface for bytecode compiler components, each helping to compile a JS expression.
     * 
     * @tparam EmitterCtx 
     * @tparam ExprNodeT 
     */
    export template <typename ExprNodeT>
    class ExprEmitterBase {
    public:
        virtual ~ExprEmitterBase() = default;

        /// NOTE: should take an `Expr`, type aliasing an `ExprNode<...>`, but std::get<'SpecificeExprType'>.
        virtual auto emit(BytecodeEmitterContext& context, const ExprNodeT& node, const std::string& source) -> bool = 0;
    };

    export template <typename StmtNodeT>
    class StmtEmitterBase {
    public:
        virtual ~StmtEmitterBase() = default;

        /// NOTE: should take a `Stmt`, type aliasing a `StmtNode<...>`, but std::get<'SpecificeStmtType'>.
        virtual auto emit(BytecodeEmitterContext& context, const StmtNodeT& node, const std::string& source) -> bool = 0;
    };

    export class BytecodeEmitterContext {
    public:
        std::array<std::unique_ptr<ExprEmitterBase<Expr>>, static_cast<std::size_t>(ExprNodeTag::last)> m_expr_emitters;

        std::array<std::unique_ptr<StmtEmitterBase<Stmt>>, static_cast<std::size_t>(StmtNodeTag::last)> m_stmt_emitters;

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

        int m_member_depth;

        // Whether AST visitation is in a callable- Used in the check for implicit returns within functions.
        bool m_in_callable;

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

                if (auto heap_dyn_str_p = m_heap.add_item(m_heap.get_next_id(), std::make_unique<DynamicString>(
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::str)],
                    Value {m_base_prototypes[static_cast<unsigned int>(BasePrototypeID::extra_length_key)]},
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

                if (auto heap_dyn_str_p = m_heap.add_item(m_heap.get_next_id(), std::make_unique<DynamicString>(
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::str)],
                    Value {m_base_prototypes[static_cast<unsigned int>(BasePrototypeID::extra_length_key)]},
                    std::forward<Item>(item)
                )); heap_dyn_str_p) {
                    m_consts.emplace_back(Value {heap_dyn_str_p});
                    m_key_consts_map[symbol] = next_global_ref_loc;
                    m_heap.update_tenure_count();
                } else {
                    return {};
                }

                return next_global_ref_loc;
            } else if constexpr (std::is_same_v<plain_item_type, std::unique_ptr<ObjectBase<Value>>> && std::is_same_v<RecordOpt, FindKeyConstOpt>) {
                // 3.2: special case - preload key strings like 'length'
                const int16_t next_global_ref_const_id = m_consts.size();
                Arg next_global_ref_loc {
                    .n = next_global_ref_const_id,
                    .tag = Location::constant,
                    .is_str_literal = true,
                    .from_closure = false,
                };

                m_consts.emplace_back(Value {m_heap.add_item(m_heap.get_next_id(), std::move(item))});
                m_key_consts_map[symbol] = next_global_ref_loc;
                m_heap.update_tenure_count();

                return next_global_ref_loc;
            } else if constexpr (std::is_same_v<plain_item_type, std::string> && std::is_same_v<RecordOpt, FindCaptureOpt>) {
                // function captured variable-name case
                const int16_t next_global_ref_const_id = m_consts.size();

                if (auto existing_capture_key_loc = lookup_symbol(symbol, FindKeyConstOpt {}); existing_capture_key_loc) {
                    existing_capture_key_loc->from_closure = true;
                    return existing_capture_key_loc;
                } else if (auto heap_dyn_str_p = m_heap.add_item(m_heap.get_next_id(), std::make_unique<DynamicString>(
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::str)],
                    Value {m_base_prototypes[static_cast<unsigned int>(BasePrototypeID::extra_length_key)]},
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
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::boolean)] = heap_native_object_p;
                } else if (symbol == "Number::prototype") {
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::number)] = heap_native_object_p;
                } else if (symbol == "String::prototype") {
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::str)] = heap_native_object_p;
                } else if (symbol == "Object::prototype") {
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::object)] = heap_native_object_p;
                } else if (symbol == "Array::prototype") {
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::array)] = heap_native_object_p;
                } else if (symbol == "Function::prototype") {
                    m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::function)] = heap_native_object_p;
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

        BytecodeEmitterContext()
        : m_global_consts_map {}, m_key_consts_map {}, m_base_prototypes {}, m_local_maps {}, m_heap {}, m_consts {}, m_code_blobs {}, m_callee_name {}, m_chunk_offsets {}, m_member_depth {0}, m_in_callable {false}, m_has_string_ops {false}, m_has_new_applied {false}, m_access_as_lval {false}, m_accessing_property {false}, m_has_call {false}, m_prepass_vars {true} {}

        void add_expr_emitter(ExprNodeTag expr_type_tag, std::unique_ptr<ExprEmitterBase<Expr>> helper) noexcept {
            const auto expr_helper_index = static_cast<std::size_t>(expr_type_tag);

            m_expr_emitters[expr_helper_index] = std::move(helper);
        }

        void add_stmt_emitter(StmtNodeTag stmt_type_tag, std::unique_ptr<StmtEmitterBase<Stmt>> helper) noexcept {
            const auto stmt_helper_index = static_cast<std::size_t>(stmt_type_tag);

            m_stmt_emitters[stmt_helper_index] = std::move(helper);
        }

        [[nodiscard]] auto emit_expr(const Expr& expr, const std::string& source) -> bool {
            const auto& [expr_data, expr_src_id, expr_src_begin, expr_src_length, expr_type_id] = expr;

            if (auto& expr_emitter = m_expr_emitters.at(static_cast<std::size_t>(expr_type_id)); expr_emitter) {
                return expr_emitter->emit(*this, expr, source);
            }

            std::println(std::cerr, "Emitter for expr-type ID {} not found.", static_cast<std::size_t>(expr_type_id));

            return false;
        }

        [[nodiscard]] auto emit_stmt(const Stmt& stmt, const std::string& source) -> bool {
            const auto& [stmt_data, stmt_src_id, stmt_src_begin, stmt_src_length, stmt_type_id] = stmt;

            if (auto& stmt_emitter = m_stmt_emitters.at(static_cast<std::size_t>(stmt_type_id)); stmt_emitter) {
                return stmt_emitter->emit(*this, stmt, source);
            }

            std::println(std::cerr, "Emitter for stmt-type ID {} not found.", static_cast<std::size_t>(stmt_type_id));

            return false;
        }

        [[nodiscard]] auto compile_script(std::vector<PreloadItem> preloadables, int heap_object_capacity, const ASTUnit& tu, const std::vector<std::string>& source_map) -> std::optional<Program> {
            // 1.1: Setup heap of desired capacity for future VM use.
            m_heap = PolyPool<ObjectBase<Value>> {heap_object_capacity};

            // 1.2: Record fundamental primitive constants once to avoid extra work.
            record_symbol("undefined", Value {}, FindGlobalConstsOpt {});
            record_symbol("null", Value {JSNullOpt {}}, FindGlobalConstsOpt {});
            record_symbol("NaN", Value {JSNaNOpt {}}, FindGlobalConstsOpt {});
            record_symbol("true", Value {true}, FindGlobalConstsOpt {});
            record_symbol("false", Value {false}, FindGlobalConstsOpt {});

            // 2.1: Record global native objects to avoid extra hassle later.
            for (auto& [pre_name, pre_entity, pre_location] : preloadables) {
                switch (pre_location) {
                case Location::constant: {
                    record_symbol(pre_name, std::move(std::get<Value>(pre_entity)), FindGlobalConstsOpt {});
                } break;
                case Location::heap_obj: {
                    auto& js_object_p = std::get<std::unique_ptr<ObjectBase<Value>>>(pre_entity);
                    if (pre_name.empty()) {
                        m_heap.add_item(m_heap.get_next_id(), std::move(js_object_p));
                        m_heap.update_tenure_count();
                    } else {
                        pre_record_object(pre_name, std::move(js_object_p));
                    }
                } break;
                case Location::key_str: {
                    record_symbol(pre_name, std::move(std::get<std::unique_ptr<ObjectBase<Value>>>(pre_entity)), FindKeyConstOpt {});
                } break;
                default: break;
                }
            }

            // 2.2: Add "length" for array accessor name.
            const int length_key_const_id = lookup_symbol("length", FindKeyConstOpt {})->n;
            m_base_prototypes[static_cast<std::size_t>(BasePrototypeID::extra_length_key)] = m_consts.at(length_key_const_id).to_object();

            // 3. Prepare initial mapping of symbols & code buffer to build.
            m_local_maps.emplace_back(CodeGenScope {
                .locals = {},
                .callee_self_refs = {},
                .next_local_id = 0,
                .block_level = -1
            });
            m_code_blobs.emplace_front();

            // 4.1: emit all top-level non-function statements as an implicit function that's called right away.
            constexpr auto global_func_id = 0; // implicit main function begins at offset 0
            m_chunk_offsets.emplace_back(0);

            // 4.2: emit all vars (especially function declaration syntax sugar) FIRST as per JS hoisting.
            for (const auto& [src_filename, decl, src_id] : tu) {
                if (!emit_stmt(*decl, source_map.at(src_id))) {
                    std::println(std::cerr, "Compile Error at source '{}' for unsupported JS construct:\nSnippet:\n'{}'\n\n", src_filename, source_map.at(src_id).substr(decl->text_begin, decl->text_length / 2));
                    return {};
                }
            }

            m_prepass_vars = false;

            for (const auto& [src_filename, decl, src_id] : tu) {
                if (!emit_stmt(*decl, source_map.at(src_id))) {
                    std::println(std::cerr, "Compile Error at source '{}' around '{}': unsupported JS construct :(\n", src_filename, source_map.at(src_id).substr(decl->text_begin, decl->text_length / 2));
                    return {};
                }
            }

            // 5: place implicit `return undefined` in top-level.
            encode_instruction(Opcode::djs_put_const, lookup_symbol("undefined", FindGlobalConstsOpt {}).value());
            encode_instruction(Opcode::djs_ret);

            // 6: Place dud offset marker for bytecode dumping to properly end.
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
