module;

#include <utility>
#include <memory>
#include <string>
#include <string_view>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <print>

export module runtime.natives;

import runtime.arrays;
import runtime.strings;
import runtime.value;
import runtime.vm;

export namespace DerkJS {
    //// BEGIN global functions:

    [[nodiscard]] auto native_parse_int(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        constexpr auto min_radix = 2;
        constexpr auto max_radix = 16;
        constexpr auto default_radix = 10;

        const int passed_rsbp = ctx->rsbp;
        std::string temp_str = ctx->stack.at(passed_rsbp + 1).to_string().value_or("");
        auto radix_n = (argc == 2) ? static_cast<std::size_t>(ctx->stack.at(passed_rsbp + 2).to_num_i32().value_or(10)) : default_radix;

        if (radix_n < min_radix || radix_n > max_radix) {
            radix_n = default_radix;
        }

        try {
            ctx->stack.at(passed_rsbp - 1) = Value {std::stoi(temp_str, nullptr, radix_n)};
        } catch (const std::exception& err) {
            std::println(std::cerr, "parseInt: String is not a valid integer literal.");
            ctx->stack.at(passed_rsbp - 1) = Value {JSNaNOpt {}};
        }

        return true;
    }

    /// BEGIN string impls:

    /// From: ES5 - 15.5.2.1
    [[nodiscard]] auto native_str_ctor(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;

        if (auto temp_str_p = ctx->heap.add_item(ctx->heap.get_next_id(), std::make_unique<DynamicString>(
            ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::str)),
            ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::extra_length_key)),
            ctx->stack.at(passed_rsbp + 1).to_string().value()
        )); temp_str_p) {
            ctx->stack.at(passed_rsbp - 1) = Value {temp_str_p};
            return true;
        } else {
            ctx->status = VMErrcode::bad_heap_alloc;   
            return false;
        }
    }

    [[nodiscard]] auto native_str_charcode_at(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
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
    [[nodiscard]] auto native_str_substring(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
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
                ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::str)),
                ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::extra_length_key)),
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
    [[nodiscard]] auto native_str_trim(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        const auto str_this_p = dynamic_cast<const StringBase*>(
            ctx->stack.at(passed_rsbp - 1).to_object()
        );

        std::string old_source;
        old_source.append_range(str_this_p->as_str_view());

        const int first_no_space_index = old_source.find_first_not_of(' ');
        const int last_no_space_index = old_source.find_last_not_of(' ');

        if (auto trimmed_string_p = ctx->heap.add_item(ctx->heap.get_next_id(), std::make_unique<DynamicString>(
            ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::str)),
            ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::extra_length_key)),
            old_source.substr(first_no_space_index, last_no_space_index - first_no_space_index + 1)
        )); trimmed_string_p) {
            ctx->stack.at(passed_rsbp - 1) = Value {trimmed_string_p};
            return true;
        }

        ctx->status = VMErrcode::bad_heap_alloc;

        return false;
    }

    [[nodiscard]] auto native_str_substr(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
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
                ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::str)),
                Value {ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::extra_length_key))},
                temp_substr
            }); created_str_p) {
            ctx->stack.at(passed_rsbp - 1) = Value {created_str_p};
            return true;
        }

        ctx->status = VMErrcode::bad_heap_alloc;
        std::println(std::cerr, "Failed to allocate JS sub-string on the heap.");
    
        return false;
    }

    //// BEGIN console impls:

    [[nodiscard]] auto native_console_log(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;

        for (auto passed_arg_local_offset = 0; passed_arg_local_offset < argc; ++passed_arg_local_offset) {
            std::print(
                "{} ",
                ctx->stack.at(passed_rsbp + passed_arg_local_offset + 1).to_string().value()
            );
        }

        std::println();

        ctx->stack.at(passed_rsbp - 1) = Value {};

        return true;
    }

    [[nodiscard]] auto native_console_read_line(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {    
        /// NOTE: Show user the passed-in prompt string: It MUST be the 1st argument on the stack.
        const auto passed_rsbp = ctx->rsbp;
        const auto& prompt_value = ctx->stack.at(passed_rsbp + 1);

        std::print("{}", prompt_value.to_string().value());

        std::string temp_line;
        std::getline(std::cin, temp_line);

        ObjectBase<Value>* line_str_p = ctx->heap.add_item(
            ctx->heap.get_next_id(),
            DynamicString {
                ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::str)),
                Value {ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::extra_length_key))},
                temp_line
            }
        );

        if (!line_str_p) {
            ctx->status = VMErrcode::bad_heap_alloc;
            std::println(std::cerr, "Failed to allocate input string on the heap.");
            return false;
        } else {
            ctx->stack.at(passed_rsbp - 1) = Value {line_str_p};
            return true;
        }
    }

    //// BEGIN time impls:

    /// TODO: probably add support for passing an argument which specifies precision: seconds, ms, ns
    [[nodiscard]] auto clock_time_now(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        const std::chrono::duration<double, std::milli> current_time_ms = std::chrono::steady_clock::now().time_since_epoch();

        /// NOTE: the VM's `djs_native_call` opcode will restore RSP -> callee RBP automtically, so the result would have to be placed there for other code.
        ctx->stack.at(passed_rsbp - 1) = Value {current_time_ms.count()};

        return true;
    }

    // [[nodiscard]] auto native_process_getenv(ExternVMCtx* ctx, PropPool<Value, Value>* props, int argc) -> bool {
    //     return false; // TODO
    // }

    /// BEGIN Array.prototype impls:

    [[nodiscard]] auto native_array_ctor(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        int maybe_fill_count = ctx->stack.at(passed_rsbp + 1).to_num_i32().value_or(0);

        if (argc == 0) {
            maybe_fill_count = 0;
        } else if (argc == 1) {
            maybe_fill_count = std::abs(maybe_fill_count);
        }

        // 1. Create blank JS array with the appropriate prototype
        auto temp_array = std::make_unique<Array>(
            ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::array)),
            Value {ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::extra_length_key))},
            Value {(argc > 1) ? argc : maybe_fill_count}
        );

        // 2. Fill with N temporary arguments OR fill with N undefined values.
        if (argc > 1) {
            for (int temp_item_offset = 0; temp_item_offset < argc; temp_item_offset++) {
                temp_array->items().emplace_back(ctx->stack.at(passed_rsbp + 1 + temp_item_offset));
            }
        } else if (argc == 1) {
            for (int temp_item_count = 0; temp_item_count < maybe_fill_count; temp_item_count++) {
                temp_array->items().emplace_back(Value {});
            }
        }

        // 3. Record array to VM heap
        ObjectBase<Value>* temp_array_p = ctx->heap.add_item(ctx->heap.get_next_id(), std::move(temp_array));

        if (temp_array_p) {
            ctx->stack.at(passed_rsbp - 1) = Value {temp_array_p};
            return true;
        }

        ctx->status = VMErrcode::bad_heap_alloc;
        std::println(std::cerr, "Failed to allocate JS array on the heap.");

        return false;
    }

    [[nodiscard]] auto native_array_push(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        auto array_this_p = dynamic_cast<Array*>(
            ctx->stack.at(passed_rsbp - 1).to_object()
        );
        Value* array_length_p = array_this_p->get_length();

        for (int temp_item_offset = 0; temp_item_offset < argc; temp_item_offset++) {
            array_this_p->items().emplace_back(ctx->stack.at(passed_rsbp + 1 + temp_item_offset));
            array_length_p->increment();
        }

        /// NOTE: By MDN, Array.prototype.push returns the updated length.
        ctx->stack.at(passed_rsbp - 1) = array_length_p->deep_clone();

        return true;
    }

    /// SEE: ES5-15.4.4.5
    [[nodiscard]] auto native_array_join(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        auto array_this_p = dynamic_cast<Array*>(
            ctx->stack.at(passed_rsbp - 1).to_object()
        );
        auto temp_str = std::make_unique<DynamicString>(
            ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::str)),
            Value {ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::extra_length_key))},
            std::string_view {}
        );

        if (!array_this_p->items().empty()) {
            std::string delim = (argc == 1) ? ctx->stack.at(passed_rsbp + 1).to_string().value_or(",") : ",";

            temp_str->append_back(array_this_p->items().front().to_string().value());

            for (int more_count = 1, self_len = array_this_p->items().size(); more_count < self_len; more_count++) {
                temp_str->append_back(delim);

                if (const auto& next_item = array_this_p->items().at(more_count); next_item.get_tag() != ValueTag::undefined && next_item.get_tag() != ValueTag::null) {
                    temp_str->append_back(next_item.to_string().value());
                }
            }
        }

        if (auto filled_str_p = ctx->heap.add_item(ctx->heap.get_next_id(), std::move(temp_str)); filled_str_p) {
            ctx->stack.at(passed_rsbp - 1) = Value {filled_str_p};
            return true;
        }

        ctx->status = VMErrcode::bad_heap_alloc;
        return false;
    }

    [[nodiscard]] auto native_array_concat(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        std::println(std::cerr, "Array.concat: Not implemented.");
        return false; // TODO!
    }

    /// Object.prototype impls:

    /// TODO: fix this to wrap primitives in object form / forward objects as-is.
    [[nodiscard]] auto native_object_ctor(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        Value target_arg = ctx->stack.at(passed_rsbp + 1);

        if (auto target_as_object_p = target_arg.to_object(); target_as_object_p) {   
            ctx->stack.at(passed_rsbp - 1) = Value {target_as_object_p};
            return true;
        }

        ctx->status = VMErrcode::bad_operation;
        std::println(std::cerr, "Object.constructor: Non-object arguments to Object ctor are unsupported.");

        return false;
    }

    [[nodiscard]] auto native_object_create(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        auto taken_proto_p = ctx->stack.at(passed_rsbp + 1).to_object();

        if (auto result_p = ctx->heap.add_item(ctx->heap.get_next_id(), std::make_unique<Object>(taken_proto_p)); result_p) {
            ctx->stack.at(passed_rsbp - 1) = Value {result_p};
            return true;
        }

        ctx->status = VMErrcode::bad_heap_alloc;
        std::println(std::cerr, "Failed to allocate JS object on the heap.");

        return false;
    }

    [[nodiscard]] auto native_object_freeze(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        ObjectBase<Value>* target_this_p = (argc == 0)
            ? ctx->stack.at(passed_rsbp - 1).to_object()
            : ctx->stack.at(passed_rsbp + 1).to_object();

        if (target_this_p != nullptr) {
            target_this_p->freeze();
        }

        return true;
    }

    /// Function.prototype impls.

    [[nodiscard]] auto native_function_call(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        ObjectBase<Value>* maybe_callable_p = ctx->stack.at(passed_rsbp - 1).to_object();
        const auto old_vm_frame_n = ctx->ending_frame_depth;
        const auto old_vm_status = ctx->status;

        if (!maybe_callable_p) {
            std::println(std::cerr, "Function.call: Cannot invoke invalid object reference.");
            ctx->status = VMErrcode::bad_operation;
            return false;
        }

        /// NOTE: See below diagram & mandate `thisArg` for now!
        // foo.call(self, 1) becomes:
        // STACK pre-call: <ref:foo> <ref:foo.call> <passed-thisArg> <args...>
        // NEXT: swap ref:foo with foo.call
        // NEXT: swap ref:foo with passed-thisArg
        // STACK for call: <ref:foo.call> <passed-thisArg> <ref:foo> <args...>
        std::swap(ctx->stack.at(passed_rsbp - 1), ctx->stack.at(passed_rsbp));
        std::swap(ctx->stack.at(passed_rsbp), ctx->stack.at(passed_rsbp + 1));
        
        /// NOTE: ensure exit of just the callback when its frame is popped...
        ctx->ending_frame_depth = ctx->frames.size();

        if (!maybe_callable_p->call(ctx, argc - 1, true)) {
            std::println(std::cerr, "Function.call: Cannot invoke non-function object.");
            ctx->status = VMErrcode::bad_operation;
            return false;
        }

        dispatch_op(*ctx, ctx->rip_p->args[0], ctx->rip_p->args[1]);

        if (ctx->status != VMErrcode::ok) {
            std::println(std::cerr, "Function.call: fatal error in callback body.");
            return false;
        }

        /// NOTE: Restore old exiting frame depth for VM.
        ctx->ending_frame_depth = old_vm_frame_n;
        ctx->status = old_vm_status;
        ctx->stack.at(passed_rsbp - 1) = ctx->stack.at(passed_rsbp);

        return true;
    }
}
