module;

#include <cstdint>
#include <utility>
#include <algorithm>
#include <vector>

export module runtime.vm;

import runtime.value;
import runtime.arrays;
import runtime.strings;
import runtime.gc;
import runtime.bytecode;

namespace DerkJS {
    export enum class VMErrcode : uint8_t {
        pending,
        bad_property_access,
        bad_operation,
        bad_heap_alloc,
        vm_abort,
        ok,
        last
    };

    /**
     * @brief Provides the bytecode VM's call frame type. The full specializations by `DispatchPolicy` are meant for use.
     * @see `DispatchPolicy` for more information about how opcodes would be dispatched.
     * @tparam Dp 
     */
    struct CallFrame {
        const Instruction* m_caller_ret_ip;
        ObjectBase<Value>* this_p;
        ObjectBase<Value>* caller_addr;
        ObjectBase<Value>* capture_p;
        int m_callee_sbp;
        int m_caller_sbp;
        uint8_t m_flags;

        template <CallFlags F>
        friend constexpr auto derkjs_call_flag(const CallFrame& frame) noexcept -> bool {
            if constexpr (F == CallFlags::is_ctor) {
                return frame.m_flags & std::to_underlying(F);
            }

            return false;
        }
    };

    #ifdef __clang__
        #ifdef __llvm__
            #if __clang_major__ >= 20
                /// NOTE: Compile-time flag to check if TCO is enabled for the build- This is only enabled on LLVM Clang 20+ to be safe.
                constexpr bool is_tco_enabled_v = true;
            #else
                /// NOTE: Compile-time flag to check if TCO is enabled for the build- This is only enabled on LLVM Clang 20+ to be safe.
                constexpr bool is_tco_enabled_v = false;
            #endif
        #else
            /// NOTE: Compile-time flag to check if TCO is enabled for the build- This is only enabled on LLVM Clang 20+ to be safe.
            constexpr bool is_tco_enabled_v = false;
        #endif
    #else
        /// NOTE: Compile-time flag to check if TCO is enabled for the build- This is only enabled on LLVM Clang 20+ to be safe.
        constexpr bool is_tco_enabled_v = false;
    #endif

    /// NOTE: This type decouples the internal state of the bytecode VM. It's used when the `DispatchPolicy::tco` specialization is used with a opcode dispatch table and dispatch TCO'd function internally apply in `VM<DispatchPolicy::tco>`. However, this is only enabled on LLVM Clang 20+ to be safe.
    export struct ExternVMCtx {
        using call_frame_type = CallFrame;

        GC gc;
        PolyPool<ObjectBase<Value>> heap;
        std::array<ObjectBase<Value>*, static_cast<std::size_t>(BasePrototypeID::last)> base_protos;
        std::vector<Value> stack;
        std::vector<CallFrame> frames;

        Value* consts_view;

        /// NOTE: holds base of bytecode blob, starts at position 0 to access offsets from.
        const Instruction* code_bp;

        /// NOTE: holds base of an index array mapping function IDs to their code starts.
        const int* fn_table_bp;

        /// NOTE: holds direct pointer to an `Instruction`.
        const Instruction* rip_p;

        /// NOTE: holds stack base pointer for call locals
        int16_t rsbp;

        /// NOTE: holds stack top pointer
        int16_t rsp;

        VMErrcode status;

        ExternVMCtx(Program& prgm, std::size_t stack_length_limit, std::size_t call_frame_limit, std::size_t gc_heap_threshold)
        : gc {gc_heap_threshold}, heap (std::move(prgm.heap_items)), base_protos(std::move(prgm.base_protos)), stack {}, frames {}, consts_view {prgm.consts.data()}, code_bp {prgm.code.data()}, fn_table_bp {prgm.offsets.data()}, rip_p {prgm.code.data() + prgm.offsets[prgm.entry_func_id]}, rsbp {0}, rsp {-1}, status {VMErrcode::pending} {
            stack.reserve(stack_length_limit);
            stack.resize(stack_length_limit);
            frames.reserve(call_frame_limit);

            if (auto global_this_p = heap.add_item(heap.get_next_id(), std::make_unique<Object>(nullptr)); !global_this_p) {
                status = VMErrcode::bad_heap_alloc;
            } else {
                frames.emplace_back(CallFrame {
                    .m_caller_ret_ip = nullptr,
                    .this_p = global_this_p, // TODO: add globalThis & working global prop assignments...
                    .caller_addr = nullptr,
                    .capture_p = heap.add_item(heap.get_next_id(), std::make_unique<Object>(nullptr)),
                    .m_callee_sbp = 0,
                    .m_caller_sbp = 0,
                });
            }
        }
    };

    inline void op_nop(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_dup(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_dup_local(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_ref_local(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_store_upval(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_ref_upval(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_put_const(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_deref(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_pop(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_emplace(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_put_this(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_discard(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_typename(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_put_obj_dud(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_make_arr(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_put_proto_key(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_get_prop(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_put_prop(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_del_prop(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_numify(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_strcat(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_mod(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_mul(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_div(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_add(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_sub(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_test_falsy(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_test_strict_eq(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_test_strict_ne(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_test_lt(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_test_lte(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_test_gt(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_test_gte(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_jump_else(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_jump_if(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_jump(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_object_call(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_ctor_call(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_ret(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void op_halt(ExternVMCtx& ctx, int16_t a0, int16_t a1);
    inline void dispatch_op(ExternVMCtx& ctx, int16_t a0, int16_t a1);

    using tco_opcode_fn = void(*)(ExternVMCtx&, int16_t, int16_t);
    constexpr tco_opcode_fn tco_opcodes[static_cast<std::size_t>(Opcode::last)] = {
        op_nop,
        op_dup, op_dup_local, op_ref_local, op_store_upval, op_ref_upval, op_put_const, op_deref, op_pop, op_emplace,
        op_put_this, op_discard, op_typename, op_put_obj_dud, op_make_arr, op_put_proto_key, op_get_prop, op_put_prop, op_del_prop,
        op_numify, op_strcat,
        op_mod, op_mul, op_div, op_add, op_sub,
        op_test_falsy, op_test_strict_eq, op_test_strict_ne, op_test_lt, op_test_lte, op_test_gt, op_test_gte,
        op_jump_else, op_jump_if, op_jump, op_object_call, op_ctor_call, op_ret,
        op_halt
    };

    inline void op_nop(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_dup(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp + 1] = ctx.stack[ctx.rsp];
        ctx.rsp++;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_dup_local(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp + 1] = ctx.stack[ctx.rsbp + a0];
        ctx.rsp++;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_ref_local(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        auto& target_local_ref = ctx.stack[ctx.rsbp + a0];

        if (const auto target_tag = target_local_ref.get_tag(); target_tag == ValueTag::val_ref/* || target_tag == ValueTag::object*/) {
            ctx.stack[ctx.rsp + 1] = target_local_ref;
        } else {
            ctx.stack[ctx.rsp + 1] = Value {&ctx.stack[ctx.rsbp + a0]};
        }

        ctx.rsp++;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_store_upval(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        if (auto new_upval_p = &ctx.frames.back().capture_p->get_property_value(ctx.stack[ctx.rsp], true); new_upval_p) {
            *new_upval_p = ctx.stack[ctx.rsp - 1];
            ctx.rip_p++;
            ctx.rsp--;
        } else {
            ctx.status = VMErrcode::bad_property_access;
        }

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_ref_upval(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp] = *ctx.frames.back().capture_p->get_property_value(ctx.stack[ctx.rsp], false);

        if (ctx.stack[ctx.rsp].get_tag() == ValueTag::undefined) {
            ctx.status = VMErrcode::bad_property_access;
        }

        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_put_const(ExternVMCtx& ctx, int16_t a0, int16_t a1)  {
        ctx.stack[ctx.rsp + 1] = ctx.consts_view[a0];
        ctx.rsp++;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_deref(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        if (auto& ref_value_ref = ctx.stack[ctx.rsp]; ref_value_ref.get_tag() == ValueTag::val_ref || ref_value_ref.get_tag() == ValueTag::object) {
            ctx.stack[ctx.rsp] = ref_value_ref.deep_clone();
            ctx.rip_p++;
        } else {
            ctx.status = VMErrcode::vm_abort;
        }

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_pop(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.rsp -= a0;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_emplace(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        auto& dest_val_ref = ctx.stack[ctx.rsp - 1];

        if (dest_val_ref.is_assignable_ref()) {
            *dest_val_ref.get_value_ref() = ctx.stack[ctx.rsp];
        }

        ctx.rsp--;
        ctx.rsp--;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_put_this(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp + 1] = Value {ctx.frames.back().this_p};
        ctx.rsp++;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_discard(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp] = Value {};
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_typename(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        const auto& temp_value_ref = ctx.stack[ctx.rsp];

        if (auto js_typename_p = ctx.heap.add_item(ctx.heap.get_next_id(), DynamicString {
            ctx.base_protos.at(static_cast<unsigned int>(BasePrototypeID::str)),
            Value {ctx.base_protos[static_cast<unsigned int>(BasePrototypeID::extra_length_key)]},
            temp_value_ref.get_typename()
        })) {
            // For `typeof` result: typename string.
            ctx.stack[ctx.rsp] = Value {js_typename_p};
            ctx.rip_p++;
        } else {
            ctx.status = VMErrcode::bad_heap_alloc;
        }

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_put_obj_dud(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.gc(ctx.heap, ctx.stack, ctx.rsp);

        auto obj_ref_p = ctx.heap.add_item(ctx.heap.get_next_id(), Object {nullptr});

        if (obj_ref_p) {
            ctx.stack[ctx.rsp + 1] = Value {obj_ref_p};
            ctx.rsp++;
            ctx.rip_p++;
        } else {
            ctx.status = VMErrcode::bad_heap_alloc;
        }

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_make_arr(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.gc(ctx.heap, ctx.stack, ctx.rsp);

        auto array_p = new Array {
            ctx.base_protos.at(static_cast<unsigned int>(BasePrototypeID::array)),
            Value {
                ctx.base_protos.at(static_cast<unsigned int>(BasePrototypeID::extra_length_key))
            },
            Value {a0}
        };
        
        if (array_p) {
            for (int next_array_item_pos = ctx.rsp - a0 + 1, end_next_array_items = ctx.rsp; next_array_item_pos <= end_next_array_items; next_array_item_pos++) {
                array_p->items().emplace_back(ctx.stack.at(next_array_item_pos));
            }

            ctx.rsp -= (a0 - 1);
            // Manage raw array_p ptr with heap... An internal slot with a unique_ptr will wrap it.
            ctx.stack[ctx.rsp] = Value {
                ctx.heap.add_item(ctx.heap.get_next_id(), array_p)
            };
            ctx.rip_p++;
        } else {
            ctx.status = VMErrcode::bad_heap_alloc;
        }

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_put_proto_key(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp + 1] = Value {JSProtoKeyOpt {}};

        ctx.rsp++;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_get_prop(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        auto& target_ref = ctx.stack[ctx.rsp - 1];

        if (ObjectBase<Value>* target_obj_p = target_ref.to_object(); target_obj_p) {
            ctx.stack[ctx.rsp - 1] = *target_obj_p->get_property_value(
                ctx.stack[ctx.rsp], // special prototype key from previous opcode `put_proto_key`
                a0
            );
            ctx.rsp--;
            ctx.rip_p++;
        } else {
            ctx.status = VMErrcode::bad_property_access;
        }

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_put_prop(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        if (auto target_object_p = ctx.stack[ctx.rsp - 2].to_object(); target_object_p) {
            target_object_p->set_property_value(
                ctx.stack[ctx.rsp - 1], // property key
                ctx.stack[ctx.rsp]      // property's new value
            );
            ctx.rsp--;
            ctx.rsp--;
            ctx.rip_p++;
        } else {
            ctx.status = VMErrcode::bad_property_access;
        }

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_del_prop(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.status = VMErrcode::bad_operation; // TODO
        return;
    }

    inline void op_numify(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        if (auto num_temp = ctx.stack[ctx.rsp].to_num_f64(); num_temp) {
            ctx.stack[ctx.rsp] = Value {*num_temp};
        } else {
            ctx.stack[ctx.rsp] = Value {JSNaNOpt {}};
        }

        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_strcat(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.gc(ctx.heap, ctx.stack, ctx.rsp);

        /// NOTE: For making TCO possible, just allocate the new string on the heap via raw ptr to avoid non-trivial destructor problems. The heap will manage that anyways.
        auto result_p = new DynamicString {
            ctx.stack[ctx.rsp].to_object()->get_prototype(),
            Value {ctx.base_protos[static_cast<unsigned int>(BasePrototypeID::extra_length_key)]},
            ctx.stack[ctx.rsp].to_string().value()};
        result_p->append_back(ctx.stack[ctx.rsp - 1].to_string().value());

        if (auto temp_str_p = ctx.heap.add_item(ctx.heap.get_next_id(), result_p); !temp_str_p) {
            ctx.status = VMErrcode::bad_heap_alloc;
        } else {
            ctx.stack[ctx.rsp - 1] = Value {temp_str_p};
            ctx.rsp--;
            ctx.rip_p++;
        }

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_mod(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        std::swap_ranges(reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp) + sizeof(Value), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp - 1));
        ctx.stack[ctx.rsp - 1] %= ctx.stack[ctx.rsp];
        ctx.rsp--;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_mul(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp - 1] *= ctx.stack[ctx.rsp]; // commutativity of the TIMES operator allows avoidance of std::swap
        ctx.rsp--;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_div(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        std::swap_ranges(reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp) + sizeof(Value), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp - 1));
        ctx.stack[ctx.rsp - 1] /= ctx.stack[ctx.rsp];
        ctx.rsp--;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_add(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp - 1] += ctx.stack[ctx.rsp]; // commutativity of the PLUS operator allows avoidance of std::swap
        ctx.rsp--;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_sub(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        std::swap_ranges(reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp) + sizeof(Value), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp - 1));
        ctx.stack[ctx.rsp - 1] -= ctx.stack[ctx.rsp];
        ctx.rsp--;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_test_falsy(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp + 1] = ctx.stack[ctx.rsp].is_falsy();
        ctx.rsp++;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_test_strict_eq(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] == ctx.stack[ctx.rsp];
        ctx.rsp--;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_test_strict_ne(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] != ctx.stack[ctx.rsp];
        ctx.rsp--;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_test_lt(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] > ctx.stack[ctx.rsp]; // IF x < y THEN y > x
        ctx.rsp--;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_test_lte(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] >= ctx.stack[ctx.rsp]; // IF x <= y THEN y >= x
        ctx.rsp--;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_test_gt(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] < ctx.stack[ctx.rsp]; // IF x > y THEN y < x
        ctx.rsp--;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_test_gte(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] <= ctx.stack[ctx.rsp]; // IF x >= y THEN y <= x
        ctx.rsp--;
        ctx.rip_p++;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_jump_else(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        if (!ctx.stack[ctx.rsp]) {
            ctx.rip_p += a0;
        } else {
            ctx.rsp--;
            ctx.rip_p++;
        }

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_jump_if(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        if (ctx.stack[ctx.rsp]) {
            ctx.rip_p += a0;
        } else {
            ctx.rsp--;
            ctx.rip_p++;
        }

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_jump(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.rip_p += a0;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_object_call(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        auto& callable_value = ctx.stack[ctx.rsp];

        if (const auto val_tag = callable_value.get_tag(); val_tag == ValueTag::val_ref && callable_value.get_value_ref()->get_tag() == ValueTag::object && callable_value.get_value_ref()->to_object()->call(&ctx, a0, a1)) {
            ; // ok
        } else if (val_tag == ValueTag::object && callable_value.to_object()->call(&ctx, a0, a1)) {
            ; // ok
        } else {
            ctx.status = VMErrcode::bad_operation;
        }

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_ctor_call(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        auto& callable_value = ctx.stack[ctx.rsp];

        if (const auto val_tag = callable_value.get_tag(); val_tag == ValueTag::val_ref && callable_value.get_value_ref()->get_tag() == ValueTag::object && callable_value.get_value_ref()->to_object()->call_as_ctor(&ctx, a0)) {
            ; // ok
        } else if (val_tag == ValueTag::object && callable_value.to_object()->call_as_ctor(&ctx, a0)) {
            ; // ok
        } else {
            ctx.status = VMErrcode::bad_operation;
        }

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_ret(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        const auto& [caller_ret_ip, caller_this_p, caller_addr, caller_capture_p, callee_sbp, caller_sbp, calling_flags] = ctx.frames.back();

        if (a0 == 0) {
            ctx.stack[callee_sbp] = ctx.stack[ctx.rsp];
        } else if (calling_flags & std::to_underlying(CallFlags::is_ctor)) {
            ctx.stack[callee_sbp] = Value {caller_this_p};
        } else {
            ctx.stack[callee_sbp] = Value {};
        }

        ctx.rsp = callee_sbp;
        ctx.rsbp = caller_sbp;
        ctx.rip_p = caller_ret_ip;

        ctx.frames.pop_back();

        if (ctx.frames.empty()) {
            ctx.status = VMErrcode::ok;
            return;
        }

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void op_halt(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        ctx.status = VMErrcode::vm_abort;

        [[clang::musttail]]
        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline void dispatch_op(ExternVMCtx& ctx, int16_t a0, int16_t a1) {
        if (ctx.status != VMErrcode::pending) {
            return;
        }

        [[clang::musttail]]
        return tco_opcodes[ctx.rip_p->op](ctx, a0, a1);
    }

    /**
     * @brief Provides the general bytecode VM type. Actual specializations via DispatchPolicy are meant to be used.
     * 
     * @tparam Dp Determines how bytecode opcodes are dispatched, either by loop-switch or tail-call optimization-friendly recursion. Tail call optimization (TCO) allows a slight speedup in the VM over a typical switch-loop by avoiding some overhead of creating / unwinding call frames.
     */
    export class VM {
    public:
        static_assert(is_tco_enabled_v, "TCO is not enabled for this compiler toolchain.");

        /// NOTE: This SHOULD NOT be modified directly!
        ExternVMCtx m_ctx;

        explicit VM(Program& prgm, std::size_t stack_length_limit, std::size_t call_frame_limit, std::size_t gc_threshold)
        : m_ctx {prgm, stack_length_limit, call_frame_limit, gc_threshold} {}

        [[nodiscard]] auto peek_final_result() const noexcept -> const Value& {
            return m_ctx.stack[0];
        }

        [[nodiscard]] auto peek_status(this auto&& self) noexcept -> VMErrcode {
            return self.m_ctx.status;
        }

        inline void operator()() {
            return dispatch_op(m_ctx, m_ctx.rip_p->args[0], m_ctx.rip_p->args[1]);
        }
    };
}
