module;

#include <utility>
#include <string>
#include <string_view>
#include <cmath>

export module runtime.intrinsics.conversions;

import runtime.value;
import runtime.context;

namespace DerkJS::Runtime::Intrinsics {
    // NOTE: can be used in String.prototype.charAt:
    export auto native_to_int32(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        const auto& v = ctx->stack.at(passed_rsbp + 1);

        if (const auto v_tag = v.get_tag(); v_tag == ValueTag::num_i32) {
            ctx->stack.at(passed_rsbp - 1) = Value { v.to_num_i32().value() };
        } else if (auto temp_dbl = v.to_num_f64().value(); v_tag == ValueTag::num_f64) {
            temp_dbl = ((temp_dbl < 0.0) ? -1.0 : 1.0) * std::floorf(std::fabs(temp_dbl));

            ctx->stack.at(passed_rsbp - 1) = Value { static_cast<int>(temp_dbl) };
        } else {
            ctx->stack.at(passed_rsbp - 1) = Value { 0 };
        }

        return true;
    }
}
