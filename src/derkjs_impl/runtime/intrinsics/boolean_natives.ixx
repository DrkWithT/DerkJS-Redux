module;

#include <utility>
#include <memory>
#include <iostream>
#include <print>

export module runtime.intrinsics.boolean_natives;

import runtime.boolean;
import runtime.value;
import runtime.context;

namespace DerkJS::Runtime::Intrinsics {
    /// Begin Boolean impls:

    export auto native_boolean_ctor(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        ObjectBase<Value>* instance_prototype_p = ctx->stack.at(passed_rsbp).to_object()->get_instance_prototype();
        const bool native_value = (argc >= 1)
            ? ctx->stack.at(passed_rsbp + 1).is_truthy()
            : false;

        if (auto temp_boolean_p = ctx->heap.add_item(ctx->heap.get_next_id(), std::make_unique<BooleanBox>(
            native_value,
            instance_prototype_p
        )); temp_boolean_p) {
            ctx->stack.at(passed_rsbp - 1) = Value {temp_boolean_p};
            return true;
        }

        ctx->status = VMErrcode::bad_heap_alloc;
        return false;
    }

    export auto native_boolean_value_of(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        const auto boolean_this_p = dynamic_cast<const BooleanBox*>(
            ctx->stack.at(passed_rsbp - 1).to_object()
        );

        if (!boolean_this_p) {
            ctx->status = VMErrcode::bad_operation;
            std::println(std::cerr, "Boolean.valueOf: Invalid this, expected a Boolean.");
            return false;
        }

        ctx->stack.at(passed_rsbp - 1) = boolean_this_p->get_native_value().to_boolean();
        return true;
    }
}
