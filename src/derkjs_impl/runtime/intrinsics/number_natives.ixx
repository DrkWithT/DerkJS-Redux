module;

#include <utility>
#include <memory>
#include <string_view>
#include <string>
#include <iostream>
#include <print>

export module runtime.intrinsics.number_natives;

import runtime.value;
import runtime.number;
import runtime.context;

namespace DerkJS::Runtime::Intrinsics {
    export auto native_number_ctor(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;

        ObjectBase<Value>* instance_prototype_p = ctx->stack.at(passed_rsbp).to_object()->get_instance_prototype();
        auto temp_number_ptr = std::make_unique<NumberBox>(
            Value { JSNaNOpt {} },
            instance_prototype_p
        );

        Value arg_value = ctx->stack.at(passed_rsbp + 1).deep_clone();

        if (!temp_number_ptr) {
            ctx->status = VMErrcode::bad_heap_alloc;
            return false;
        }

        auto temp_number_instance_p = temp_number_ptr.get();
        auto temp_number_object_p = ctx->heap.add_item(
            ctx->heap.get_next_id(),
            std::move(temp_number_ptr)
        );

        if (arg_value.get_tag() == ValueTag::num_i32) {
            temp_number_instance_p->get_native_value() = Value { arg_value.to_num_i32().value() };
        } else if (arg_value.get_tag() == ValueTag::num_f64) {
            temp_number_instance_p->get_native_value() = Value { arg_value.to_num_f64().value() };
        } else if (arg_value.get_tag() == ValueTag::null) {
            temp_number_instance_p->get_native_value() = Value {0};
        }

        ctx->stack.at(passed_rsbp - 1) = Value {temp_number_object_p};

        return true;
    }

    export auto native_number_value_of(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        const auto number_this_p = dynamic_cast<const NumberBox*>(
            ctx->stack.at(passed_rsbp - 1).to_object()
        );

        if (!number_this_p) {
            ctx->status = VMErrcode::bad_operation;
            std::println(std::cerr, "Number.valueOf: cannot invoke on non-Number instance.");
            return false;
        }

        ctx->stack.at(passed_rsbp - 1) = number_this_p->get_native_value();

        return true;
    }

    export auto native_number_to_fixed(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        constexpr int min_fractional_digit_n = 0;
        constexpr int max_fractional_digit_n = 20;

        const int passed_rsbp = ctx->rsbp;
        const auto number_this_p = dynamic_cast<const NumberBox*>(
            ctx->stack.at(passed_rsbp - 1).to_object()
        );

        if (!number_this_p) {
            ctx->status = VMErrcode::bad_operation;
            std::println(std::cerr, "Number.toFixed: cannot invoke for non-Number instance.");
            return false;
        }

        const int fractional_places = (argc >= 1) ? ctx->stack.at(passed_rsbp + 1).to_num_i32().value_or(0) : 0 ;

        if (fractional_places < min_fractional_digit_n || fractional_places > max_fractional_digit_n) {
            ctx->status = VMErrcode::bad_operation;
            std::println(std::cerr, "Number.toFixed: invalid fractionalDigits argument.");
            return false;
        }

        auto number_value_opt = number_this_p->get_native_value().to_num_f64();
        std::string result;

        if (!number_value_opt) {
            result.append_range(std::string_view {"NaN"});
        } else if (const auto number_value = *number_value_opt; number_value < 0.0) {
            result.append_range(std::string_view {"-"});
        } else {
            result.append_range(std::to_string(number_value));
        }

        if (const int dot_pos = result.find_first_of('.'); dot_pos != std::string::npos) {
            // s = 100.12345, digits = 3;
            // r = 100.123 <-- s.substr(0, dot_pos + 1 + digits)
            result = result.substr(0, dot_pos + 1 + fractional_places);
        } else {
            result += '.';

            for (int trailing_zeroes = fractional_places; trailing_zeroes > 0; trailing_zeroes--) {
                result += '0';
            }
        }

        return ctx->push_string(result, passed_rsbp);
    }
}