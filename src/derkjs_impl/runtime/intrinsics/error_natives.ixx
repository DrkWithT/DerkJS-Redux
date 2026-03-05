module;

#include <utility>
#include <memory>

export module runtime.intrinsics.error_natives;

import runtime.value;
import runtime.errors;
import runtime.context;

namespace DerkJS::Runtime::Intrinsics {
    export auto native_error_ctor(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        const auto& message_value = ctx->stack.at(passed_rsbp + 1);

        if (auto error_object_p = ctx->heap.add_item(
            ctx->heap.get_next_id(),
            std::make_unique<NativeError>(
                ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::error)),
                Value {ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::extra_msg_key))},
                message_value,
                Value {ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::extra_name_key))}
            )
        ); error_object_p) {
            ctx->stack.at(passed_rsbp - 1) = Value {error_object_p};
            return true;
        }

        ctx->status = VMErrcode::bad_heap_alloc;
        return false;
    }
}