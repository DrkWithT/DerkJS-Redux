module;

#include <cstddef>
#include <utility>
#include <algorithm>

export module runtime.op_handlers;

import runtime.value;
import runtime.strings;
import runtime.object;
import runtime.arrays;
import runtime.bytecode;
import runtime.gc;
export import runtime.context;

#ifdef __llvm__    
    #if __clang_major__ >= 17
        #define TCO_ATTR [[clang::musttail]]
    #endif
#endif

#ifdef __GNUC__
    #if __GNUC__ > 13
        #define TCO_ATTR [[gnu::musttail]]
    #endif
#endif

#ifndef TCO_ATTR
    #error "Only GCC 13+ or Clang 17+ is supported."
#endif

namespace DerkJS {
    inline void op_nop(ExternVMCtx& ctx);
    inline void op_dup(ExternVMCtx& ctx);
    inline void op_dup_local(ExternVMCtx& ctx);
    inline void op_ref_local(ExternVMCtx& ctx);
    inline void op_store_upval(ExternVMCtx& ctx);
    inline void op_ref_upval(ExternVMCtx& ctx);
    inline void op_put_const(ExternVMCtx& ctx);
    inline void op_deref(ExternVMCtx& ctx);
    inline void op_pop(ExternVMCtx& ctx);
    inline void op_emplace(ExternVMCtx& ctx);
    inline void op_put_this(ExternVMCtx& ctx);
    inline void op_ref_error(ExternVMCtx& ctx);
    inline void op_discard(ExternVMCtx& ctx);
    inline void op_try_del(ExternVMCtx& ctx);
    inline void op_typename(ExternVMCtx& ctx);
    inline void op_put_obj_dud(ExternVMCtx& ctx);
    inline void op_make_arr(ExternVMCtx& ctx);
    inline void op_put_proto_key(ExternVMCtx& ctx);
    inline void op_get_prop(ExternVMCtx& ctx);
    inline void op_put_prop(ExternVMCtx& ctx);
    inline void op_ref_pack(ExternVMCtx& ctx);
    inline void op_numify(ExternVMCtx& ctx);
    inline void op_strcat(ExternVMCtx& ctx);
    inline void op_pre_inc(ExternVMCtx& ctx);
    inline void op_pre_dec(ExternVMCtx& ctx);
    inline void op_mod(ExternVMCtx& ctx);
    inline void op_mul(ExternVMCtx& ctx);
    inline void op_div(ExternVMCtx& ctx);
    inline void op_add(ExternVMCtx& ctx);
    inline void op_sub(ExternVMCtx& ctx);
    inline void op_test_falsy(ExternVMCtx& ctx);
    inline void op_test_strict_eq(ExternVMCtx& ctx);
    inline void op_test_strict_ne(ExternVMCtx& ctx);
    inline void op_test_lt(ExternVMCtx& ctx);
    inline void op_test_lte(ExternVMCtx& ctx);
    inline void op_test_gt(ExternVMCtx& ctx);
    inline void op_test_gte(ExternVMCtx& ctx);
    inline void op_cmp_protos(ExternVMCtx& ctx);
    inline void op_jump_else(ExternVMCtx& ctx);
    inline void op_jump_if(ExternVMCtx& ctx);
    inline void op_jump(ExternVMCtx& ctx);
    inline void op_object_call(ExternVMCtx& ctx);
    inline void op_ctor_call(ExternVMCtx& ctx);
    inline void op_ret(ExternVMCtx& ctx);
    inline void op_throw(ExternVMCtx& ctx);
    inline void op_catch(ExternVMCtx& ctx);
    inline void op_halt(ExternVMCtx& ctx);
    export inline void dispatch_op(ExternVMCtx& ctx);

    using tco_opcode_fn = void(*)(ExternVMCtx&);
    constexpr tco_opcode_fn tco_opcodes[static_cast<std::size_t>(Opcode::last)] = {
        op_nop,
        op_dup, op_dup_local, op_ref_local, op_store_upval, op_ref_upval, op_put_const, op_deref, op_pop, op_emplace,
        op_put_this, op_ref_error, op_discard, op_try_del, op_typename, op_put_obj_dud, op_make_arr, op_put_proto_key, op_get_prop, op_put_prop, op_ref_pack,
        op_numify, op_strcat, op_pre_inc, op_pre_dec,
        op_mod, op_mul, op_div, op_add, op_sub,
        op_test_falsy, op_test_strict_eq, op_test_strict_ne, op_test_lt, op_test_lte, op_test_gt, op_test_gte, op_cmp_protos,
        op_jump_else, op_jump_if, op_jump, op_object_call, op_ctor_call, op_ret,
        op_throw, op_catch,
        op_halt
    };

    inline void op_nop(ExternVMCtx& ctx) {
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_dup(ExternVMCtx& ctx) {
        ctx.stack[ctx.rsp + 1] = ctx.stack[ctx.rsp];
        ctx.rsp++;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_dup_local(ExternVMCtx& ctx) {
        ctx.stack[ctx.rsp + 1] = ctx.stack[ctx.rsbp + ctx.rip_p->args[0]];
        ctx.rsp++;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_ref_local(ExternVMCtx& ctx) {
        auto& target_local_ref = ctx.stack[ctx.rsbp + ctx.rip_p->args[0]];

        if (const auto target_tag = target_local_ref.get_tag(); target_tag == ValueTag::val_ref/* || target_tag == ValueTag::object*/) {
            ctx.stack[ctx.rsp + 1] = target_local_ref;
        } else {
            ctx.stack[ctx.rsp + 1] = Value {&ctx.stack[ctx.rsbp + ctx.rip_p->args[0]]};
        }

        ctx.rsp++;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_store_upval(ExternVMCtx& ctx) {
        if (auto new_upval_p = ctx.frames.back().capture_p->set_property_value(ctx.stack[ctx.rsp], ctx.stack[ctx.rsp - 1]); new_upval_p) {
            new_upval_p->clear_flag<AttrMask::configurable>();
            ctx.rip_p++;
            ctx.rsp--;
        } else {
            ctx.status = VMErrcode::bad_property_access;
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_ref_upval(ExternVMCtx& ctx) {
        if (auto upval_ptr = ctx.frames.back().capture_p->get_property_value(ctx.stack[ctx.rsp], false).ref_value(); upval_ptr) {
            ctx.stack[ctx.rsp] = Value {upval_ptr};
            ctx.rip_p++;
        } else {
            ctx.status = VMErrcode::bad_property_access;
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_put_const(ExternVMCtx& ctx)  {
        ctx.stack[ctx.rsp + 1] = ctx.consts_view[ctx.rip_p->args[0]];
        ctx.rsp++;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_deref(ExternVMCtx& ctx) {
        ctx.stack.at(ctx.rsp) = ctx.stack.at(ctx.rsp).deep_clone();
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_pop(ExternVMCtx& ctx) {
        ctx.rsp -= ctx.rip_p->args[0];
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_emplace(ExternVMCtx& ctx) {
        auto& dest_val_ref = ctx.stack[ctx.rsp - 1];

        if (dest_val_ref.is_assignable_ref()) {
            *dest_val_ref.get_value_ref() = ctx.stack[ctx.rsp];
        }

        ctx.rsp--;
        ctx.rsp--;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_put_this(ExternVMCtx& ctx) {
        ctx.stack.at(ctx.rsp + 1) = ctx.stack.at(ctx.rsbp - 1);
        ctx.rsp++;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_ref_error(ExternVMCtx& ctx) {
        ctx.stack.at(ctx.rsp + 1) = Value {ctx.current_error};
        ctx.rsp++;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_discard(ExternVMCtx& ctx) {
        ctx.stack[ctx.rsp] = Value {JSUndefOpt {}};
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_try_del(ExternVMCtx& ctx) {
        const auto a0 = ctx.rip_p->args[0];

        if (auto& target_value = ctx.stack.at(ctx.rsp); target_value.get_tag() != ValueTag::val_ref) {
            ctx.stack.at(ctx.rsp) = Value {true};
            ctx.rip_p++;
        } else if (target_value.flag<AttrMask::property>()) {
            *target_value.get_value_ref() = Value {JSUndefOpt {}};
            target_value = Value {true};
            ctx.rip_p++;
        } else if (ctx.prepare_error("Invalid delete on non-configurable / non-property name.", std::to_underlying(BuiltInObjects::syntax_error_ctor))) {
            const auto old_vm_frame_n = ctx.ending_frame_depth;
            ctx.ending_frame_depth = ctx.frames.size();

            if (ctx.stack.at(ctx.rsp - 1).to_object()->call_as_ctor(&ctx, 1)) {
                dispatch_op(ctx);
                ctx.ending_frame_depth = old_vm_frame_n;
                ctx.status = ctx.try_recover(ctx.stack.at(ctx.rsp).to_object(), a0 == 1);
            } else {
                ctx.status = VMErrcode::bad_operation;
            }
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_typename(ExternVMCtx& ctx) {
        const auto& temp_value_ref = ctx.stack[ctx.rsp];

        if (auto js_typename_p = ctx.heap.add_item(ctx.heap.get_next_id(), DynamicString {
            ctx.builtins.at(static_cast<unsigned int>(BuiltInObjects::str)),
            Value {ctx.builtins[static_cast<unsigned int>(BuiltInObjects::extra_length_key)]},
            temp_value_ref.get_typename()
        })) {
            // For `typeof` result: typename string.
            ctx.stack[ctx.rsp] = Value {js_typename_p};
            ctx.rip_p++;
        } else {
            ctx.status = VMErrcode::bad_heap_alloc;
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_put_obj_dud(ExternVMCtx& ctx) {
        ctx.gc(ctx.heap, ctx.stack, ctx.rsp);

        auto obj_ref_p = ctx.heap.add_item(ctx.heap.get_next_id(), Object {
            /// NOTE: {}.__proto__ === Object.prototype
            ctx.builtins.at(static_cast<unsigned int>(BuiltInObjects::object))
        });

        if (obj_ref_p) {
            ctx.stack[ctx.rsp + 1] = Value {obj_ref_p};
            ctx.rsp++;
            ctx.rip_p++;
        } else {
            ctx.status = VMErrcode::bad_heap_alloc;
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_make_arr(ExternVMCtx& ctx) {
        ctx.gc(ctx.heap, ctx.stack, ctx.rsp);

        const auto a0 = ctx.rip_p->args[0];
        auto array_p = new Array {
            /// NOTE: [].__proto__ === Array.prototype
            ctx.builtins.at(static_cast<unsigned int>(BuiltInObjects::array)),
            Value { ctx.builtins.at(static_cast<unsigned int>(BuiltInObjects::extra_length_key)) },
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

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_put_proto_key(ExternVMCtx& ctx) {
        ctx.stack[ctx.rsp + 1] = Value {JSProtoKeyOpt {}};

        ctx.rsp++;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_get_prop(ExternVMCtx& ctx) {
        auto& target_ref = ctx.stack.at(ctx.rsp - 1);
        const auto a0 = ctx.rip_p->args[0];
        const auto a1 = ctx.rip_p->args[1];

        if (ObjectBase<Value>* target_obj_p = target_ref.to_object(); target_obj_p) {
            ctx.stack.at(ctx.rsp - 1) = target_obj_p->get_property_value(
                ctx.stack.at(ctx.rsp), // special prototype key from previous opcode `put_proto_key`
                a0
            ).get_value();
            ctx.rsp--;
            ctx.rip_p++;
        } else if (ctx.prepare_error("Invalid property access of undefined / primitive.", std::to_underlying(BuiltInObjects::type_error_ctor))) {
            ctx.status = ctx.try_recover(ctx.stack.at(ctx.rsp).to_object(), a1 == 1);
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_put_prop(ExternVMCtx& ctx) {
        if (
            auto target_object_p = ctx.stack[ctx.rsp - 2].to_object();
            target_object_p != nullptr && target_object_p->set_property_value(
                ctx.stack[ctx.rsp - 1], // property key
                ctx.stack[ctx.rsp]      // property's new value
            ) != nullptr
        ) {
            ctx.rsp--;
            ctx.rsp--;
            ctx.rip_p++;
        } else {
            ctx.status = VMErrcode::bad_property_access;
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_ref_pack(ExternVMCtx& ctx) {
        ctx.stack.at(ctx.rsp + 1) = Value {ctx.frames.back().pack_array_p};
        ctx.rsp++;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_numify(ExternVMCtx& ctx) {
        if (auto num_temp = ctx.stack[ctx.rsp].to_num_f64(); num_temp) {
            ctx.stack[ctx.rsp] = Value {*num_temp};
        } else {
            ctx.stack[ctx.rsp] = Value {JSNaNOpt {}};
        }

        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_strcat(ExternVMCtx& ctx) {
        ctx.gc(ctx.heap, ctx.stack, ctx.rsp);

        /// NOTE: For making TCO possible, just allocate the new string on the heap via raw ptr to avoid non-trivial destructor problems. The heap will manage that anyways.
        auto result_p = new DynamicString {
            ctx.stack[ctx.rsp].to_object()->get_prototype(),
            Value {ctx.builtins[static_cast<unsigned int>(BuiltInObjects::extra_length_key)]},
            ctx.stack[ctx.rsp].to_string()};
        result_p->append_back(ctx.stack[ctx.rsp - 1].to_string());

        if (auto temp_str_p = ctx.heap.add_item(ctx.heap.get_next_id(), result_p); !temp_str_p) {
            ctx.status = VMErrcode::bad_heap_alloc;
        } else {
            ctx.stack[ctx.rsp - 1] = Value {temp_str_p};
            ctx.rsp--;
            ctx.rip_p++;
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_pre_inc(ExternVMCtx& ctx) {
        ctx.stack.at(ctx.rsp) = ctx.stack.at(ctx.rsp).increment().deep_clone();

        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_pre_dec(ExternVMCtx& ctx) {
        ctx.stack.at(ctx.rsp) = ctx.stack.at(ctx.rsp).decrement().deep_clone();

        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_mod(ExternVMCtx& ctx) {
        std::swap_ranges(reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp) + sizeof(Value), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp - 1));
        ctx.stack[ctx.rsp - 1] %= ctx.stack[ctx.rsp];
        ctx.rsp--;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_mul(ExternVMCtx& ctx) {
        ctx.stack[ctx.rsp - 1] *= ctx.stack[ctx.rsp]; // commutativity of the TIMES operator allows avoidance of std::swap
        ctx.rsp--;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_div(ExternVMCtx& ctx) {
        std::swap_ranges(reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp) + sizeof(Value), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp - 1));
        ctx.stack[ctx.rsp - 1] /= ctx.stack[ctx.rsp];
        ctx.rsp--;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_add(ExternVMCtx& ctx) {
        ctx.stack[ctx.rsp - 1] += ctx.stack[ctx.rsp]; // commutativity of the PLUS operator allows avoidance of std::swap
        ctx.rsp--;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_sub(ExternVMCtx& ctx) {
        std::swap_ranges(reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp) + sizeof(Value), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp - 1));
        ctx.stack[ctx.rsp - 1] -= ctx.stack[ctx.rsp];
        ctx.rsp--;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_test_falsy(ExternVMCtx& ctx) {
        ctx.stack[ctx.rsp + 1] = ctx.stack[ctx.rsp].is_falsy();
        ctx.rsp++;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_test_strict_eq(ExternVMCtx& ctx) {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] == ctx.stack[ctx.rsp];
        ctx.rsp--;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_test_strict_ne(ExternVMCtx& ctx) {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] != ctx.stack[ctx.rsp];
        ctx.rsp--;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_test_lt(ExternVMCtx& ctx) {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] > ctx.stack[ctx.rsp]; // IF x < y THEN y > x
        ctx.rsp--;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_test_lte(ExternVMCtx& ctx) {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] >= ctx.stack[ctx.rsp]; // IF x <= y THEN y >= x
        ctx.rsp--;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_test_gt(ExternVMCtx& ctx) {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] < ctx.stack[ctx.rsp]; // IF x > y THEN y < x
        ctx.rsp--;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_test_gte(ExternVMCtx& ctx) {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] <= ctx.stack[ctx.rsp]; // IF x >= y THEN y <= x
        ctx.rsp--;
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_cmp_protos(ExternVMCtx& ctx) {
        if (auto lhs_as_obj_p = ctx.stack.at(ctx.rsp - 1).to_object(); !lhs_as_obj_p) {
            ctx.stack.at(ctx.rsp - 1) = Value {false};
            ctx.rsp--;
            ctx.rip_p++;
        } else if (auto rhs_as_obj_p = ctx.stack.at(ctx.rsp).to_object(); !rhs_as_obj_p) {
            ctx.status = VMErrcode::bad_operation;
        } else {
            ctx.stack.at(ctx.rsp - 1) = Value {
                /// NOTE: This check computes `LHS.__proto__ === RHS.prototype` for `instanceof`.
                lhs_as_obj_p->get_prototype() == rhs_as_obj_p->get_instance_prototype()
            };
            ctx.rsp--;
            ctx.rip_p++;
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_jump_else(ExternVMCtx& ctx) {
        if (!ctx.stack[ctx.rsp]) {
            ctx.rip_p += ctx.rip_p->args[0];
        } else {
            ctx.rsp--;
            ctx.rip_p++;
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_jump_if(ExternVMCtx& ctx) {
        if (ctx.stack[ctx.rsp]) {
            ctx.rip_p += ctx.rip_p->args[0];
        } else {
            ctx.rsp--;
            ctx.rip_p++;
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_jump(ExternVMCtx& ctx) {
        ctx.rip_p += ctx.rip_p->args[0];

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_object_call(ExternVMCtx& ctx) {
        const auto a0 = ctx.rip_p->args[0];
        const auto a1 = ctx.rip_p->args[1];

        if (auto callable_ptr = ctx.stack.at(ctx.rsp - a0).to_object(); callable_ptr != nullptr && callable_ptr->call(&ctx, a0, a1)) {
            ; // ok
        } else {
            ctx.status = VMErrcode::bad_operation;
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_ctor_call(ExternVMCtx& ctx) {
        const auto a0 = ctx.rip_p->args[0];
        if (auto callable_ptr = ctx.stack.at(ctx.rsp - a0).to_object(); callable_ptr != nullptr && callable_ptr->call_as_ctor(&ctx, a0)) {
            ;
        } else {
            ctx.status = VMErrcode::bad_operation;
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_ret(ExternVMCtx& ctx) {
        const auto& [caller_ret_ip, caller_addr, caller_capture_p, pack_object_p, callee_sbp, caller_sbp, calling_flags] = ctx.frames.back();

        if (const auto a0 = ctx.rip_p->args[0]; a0 == 0) {
            ctx.stack.at(callee_sbp - 1) = ctx.stack.at(ctx.rsp);
        } else if (calling_flags & std::to_underlying(CallFlags::is_ctor)) {
            /// NOTE: The initialized `this` object from the ctor must be at CALLEE_RSBP - 1.
            // ctx.stack.at(callee_sbp - 1) = Value {...};
        } else {
            ctx.stack.at(callee_sbp - 1) = Value {JSUndefOpt {}};
        }

        ctx.rsp = callee_sbp - 1;
        ctx.rsbp = caller_sbp;
        ctx.rip_p = caller_ret_ip;
        ctx.frames.pop_back();

        if (ctx.frames.size() <= ctx.ending_frame_depth) {
            ctx.status = VMErrcode::ok;
            return;
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    // TODO: refactor to call ErrorXYZ ctor, using IDs to select a built-in Error ctor to invoke with the stack top.
    inline void op_throw(ExternVMCtx& ctx) {
        const auto a0 = ctx.rip_p->args[0];

        //? NOTEs: index ctx.builtins for the Error ctor with an `a1` as index of the built-in object pointer array.
        if (auto thrown_value_as_obj = ctx.stack.at(ctx.rsp).to_object(); thrown_value_as_obj != nullptr && thrown_value_as_obj->get_class_name() == "Error") {
            ctx.status = ctx.try_recover(thrown_value_as_obj, a0 == 1);
        } else if (auto error_ctor_ptr = ctx.builtins.at(std::to_underlying(BuiltInObjects::error_ctor)); error_ctor_ptr->call_as_ctor(&ctx, 1)) {
            ctx.status = ctx.try_recover(ctx.stack.at(ctx.rsp).to_object(), a0 == 1);
        } else {
            ctx.status = VMErrcode::bad_operation;
        }

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_catch(ExternVMCtx& ctx) {
        ctx.rip_p++;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    inline void op_halt(ExternVMCtx& ctx) {
        ctx.status = VMErrcode::vm_abort;

        TCO_ATTR
        return dispatch_op(ctx);
    }

    export inline void dispatch_op(ExternVMCtx& ctx) {
        if (ctx.status != VMErrcode::pending /* || ctx.dispatch_allowance == 0*/) {
            return;
        }

        TCO_ATTR
        return tco_opcodes[ctx.rip_p->op](ctx);
    }
}
