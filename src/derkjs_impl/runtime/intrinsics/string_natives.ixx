module;

#include <utility>
#include <algorithm>
#include <memory>
#include <string_view>
#include <string>
#include <iostream>
#include <print>

export module runtime.intrinsics.string_natives;

import runtime.value;
import runtime.strings;
import runtime.context;

namespace DerkJS::Runtime::Intrinsics {
    /// NOTE: See ES5 - 15.5.2.1
    export auto native_str_ctor(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;

        if (argc <= 0) {
            return ctx->push_string(
                std::string {},
                passed_rsbp
            );
        } else {
            return ctx->push_string(
                ctx->stack.at(passed_rsbp + 1).to_string(),
                passed_rsbp
            );
        }
    }

    export auto native_str_charcode_at(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        const auto str_this_p = dynamic_cast<const StringBase*>(
            ctx->stack.at(passed_rsbp - 1).to_object()
        );
        const int str_at_pos = ctx->stack.at(passed_rsbp + 1).to_num_i32().value_or(-1);
        std::string_view chars_view = str_this_p->as_str_view();

        if (const int chars_n = chars_view.length(); str_at_pos < 0 || str_at_pos >= chars_n) {
            ctx->stack.at(passed_rsbp - 1) = Value {JSNaNOpt {}}; // get NaN for invalid char positions
        } else {
            ctx->stack.at(passed_rsbp - 1) = Value {static_cast<int>(chars_view[str_at_pos])};
        }

        return true;
    }

    // ES5 15.5.4.15 - Create a substring with start & end indices of an original string.
    export auto native_str_substring(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        const auto str_this_p = dynamic_cast<const StringBase*>(
            ctx->stack.at(passed_rsbp - 1).to_object()
        );

        /// NOTE: normalize start and end substring bounds as 0 minimum TO (N - 1) maximum
        const int old_length = str_this_p->as_str_view().length();
        const int final_start = std::min(
            std::max(
                ctx->stack.at(passed_rsbp + 1).to_num_i32().value_or(0), // start index (NaNs -> 0)
                0
            ),
            old_length
        );
        const int final_end = std::min(
            std::max(
                // end index (NaNs -> 0)
                (argc > 1) ? ctx->stack.at(passed_rsbp + 2).to_num_i32().value_or(0) : old_length,
                0
            ),
            old_length
        );
        const int from_index = std::min(final_start, final_end);
        const int to_index = std::max(final_start, final_end);

        if (auto substring_p = ctx->heap.add_item(
            ctx->heap.get_next_id(),
            std::make_unique<DynamicString>(
                ctx->builtins.at(static_cast<unsigned int>(BuiltInObjects::str)),
                ctx->builtins.at(static_cast<unsigned int>(BuiltInObjects::extra_length_key)),
                std::string_view {str_this_p->as_str_view().substr(from_index, to_index - from_index)}
            )
        )) {
            ctx->stack.at(passed_rsbp - 1) = Value {substring_p};
            return true;
        } else {
            ctx->status = VMErrcode::bad_heap_alloc;
            return false;
        }
    }

    // ES5 - 15.5.4.20: remove fringe spaces.
    export auto native_str_trim(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        const auto str_this_p = dynamic_cast<const StringBase*>(
            ctx->stack.at(passed_rsbp - 1).to_object()
        );

        std::string old_source;
        old_source.append_range(str_this_p->as_str_view());

        const int first_no_space_index = old_source.find_first_not_of(' ');
        const int last_no_space_index = old_source.find_last_not_of(' ');

        if (auto trimmed_string_p = ctx->heap.add_item(ctx->heap.get_next_id(), std::make_unique<DynamicString>(
            ctx->builtins.at(static_cast<unsigned int>(BuiltInObjects::str)),
            ctx->builtins.at(static_cast<unsigned int>(BuiltInObjects::extra_length_key)),
            old_source.substr(first_no_space_index, last_no_space_index - first_no_space_index + 1)
        )); trimmed_string_p) {
            ctx->stack.at(passed_rsbp - 1) = Value {trimmed_string_p};
            return true;
        }

        ctx->status = VMErrcode::bad_heap_alloc;

        return false;
    }

    export auto native_str_substr(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        const auto str_this_p = dynamic_cast<const StringBase*>(
            ctx->stack.at(passed_rsbp - 1).to_object()
        );
        const int substr_begin = ctx->stack.at(passed_rsbp + 1).to_num_i32().value_or(0);
        const int substr_len = ctx->stack.at(passed_rsbp + 2).to_num_i32().value_or(0);

        std::string temp_substr;
        temp_substr.append_range(
            str_this_p->as_str_view().substr(substr_begin, substr_len)
        );

        if (auto created_str_p = ctx->heap.add_item(
            ctx->heap.get_next_id(),
            DynamicString {
                ctx->builtins.at(static_cast<unsigned int>(BuiltInObjects::str)),
                Value {ctx->builtins.at(static_cast<unsigned int>(BuiltInObjects::extra_length_key))},
                temp_substr
            }); created_str_p) {
            ctx->stack.at(passed_rsbp - 1) = Value {created_str_p};
            return true;
        }

        ctx->status = VMErrcode::bad_heap_alloc;
        std::println(std::cerr, "Failed to allocate JS sub-string on the heap.");
    
        return false;
    }
}