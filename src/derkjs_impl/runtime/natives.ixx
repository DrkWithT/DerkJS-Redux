module;

#include <utility>
#include <memory>
#include <string>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <print>

export module runtime.natives;

import runtime.objects;
import runtime.arrays;
import runtime.strings;
import runtime.value;
import runtime.vm;

export namespace DerkJS {
    //// BEGIN console impls:

    [[nodiscard]] auto native_console_log(ExternVMCtx* ctx, [[maybe_unused]] PropPool<PropertyHandle<Value>, Value>* props, int argc) -> bool {
        const int vm_rsbp = ctx->rsbp;

        for (auto passed_arg_local_offset = 0; passed_arg_local_offset < argc; ++passed_arg_local_offset) {
            std::print("{} ", ctx->stack[vm_rsbp + passed_arg_local_offset].to_string().value());
        }

        std::println();

        return true;
    }

    [[nodiscard]] auto native_console_read_line(ExternVMCtx* ctx, [[maybe_unused]] PropPool<PropertyHandle<Value>, Value>* props, int argc) -> bool {    
        /// NOTE: Show user the passed-in prompt string: It MUST be the 1st argument on the stack.
        const auto passed_rsbp = ctx->rsbp;
        std::print("{}", ctx->stack[passed_rsbp].to_string().value());

        std::string temp_line;
        std::getline(std::cin, temp_line);

        ObjectBase<Value>* line_str_p = ctx->heap.add_item(ctx->heap.get_next_id(), DynamicString {std::move(temp_line)});

        if (!line_str_p) {
            ctx->stack[passed_rsbp] = Value {};
            return false;
        }

        ctx->stack[passed_rsbp] = Value {line_str_p};

        return true;
    }

    //// BEGIN time impls:

    /// TODO: probably add support for passing an argument which specifies precision: seconds, ms, ns
    [[nodiscard]] auto clock_time_now(ExternVMCtx* ctx, [[maybe_unused]] PropPool<PropertyHandle<Value>, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        const std::chrono::duration<double, std::milli> current_time_ms = std::chrono::steady_clock::now().time_since_epoch();

        /// NOTE: the VM's `djs_native_call` opcode will restore RSP -> callee RBP automtically, so the result would have to be placed there for other code.
        ctx->stack[passed_rsbp] = Value {current_time_ms.count()};

        return true;
    }

    [[nodiscard]] auto native_process_exit(ExternVMCtx* ctx, PropPool<PropertyHandle<Value>, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        const auto exit_status = ctx->stack.at(passed_rsbp).to_num_i32().value_or(1);

        ctx->stack[0] = Value {exit_status};
        ctx->rip_p = nullptr;

        return true;
    }

    // [[nodiscard]] auto native_process_getenv(ExternVMCtx* ctx, PropPool<PropertyHandle<Value>, Value>* props, int argc) -> bool {
    //     return false; // TODO
    // }

    /// BEGIN Array.prototype.xyz impls:

    [[nodiscard]] auto native_array_ctor(ExternVMCtx* ctx, [[maybe_unused]] PropPool<PropertyHandle<Value>, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        // 1. Consume & bind Array prototype object to a blank array
        auto temp_array = std::make_unique<Array>(ctx->stack[ctx->rsp].to_object());

        // 2. Fill with N temporary arguments
        for (int temp_item_offset = 0; temp_item_offset < argc; temp_item_offset++) {
            temp_array->items().emplace_back(ctx->stack[passed_rsbp + temp_item_offset]);
        }

        ObjectBase<Value>* temp_array_p = ctx->heap.add_item(ctx->heap.get_next_id(), std::move(temp_array));

        if (!temp_array_p) {
            ctx->stack[passed_rsbp] = Value {};
            return false;
        }

        // 3. Put reference to new array
        ctx->stack[passed_rsbp] = Value {temp_array_p};

        return true;
    }

    [[nodiscard]] auto native_array_push(ExternVMCtx* ctx, [[maybe_unused]] PropPool<PropertyHandle<Value>, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        auto array_this_p = dynamic_cast<Array*>(ctx->stack.at(passed_rsbp + argc).to_object()); // consume an array object by reference off the stack for mutation

        for (int temp_item_offset = 0; temp_item_offset < argc; temp_item_offset++) {
            array_this_p->items().emplace_back(ctx->stack[passed_rsbp + temp_item_offset]);
        }

        /// NOTE: By MDN, Array.prototype.push returns a new length.
        ctx->stack[passed_rsbp] = Value {
            static_cast<int>(array_this_p->items().size())
        };

        return true;
    }

    [[nodiscard]] auto native_array_pop(ExternVMCtx* ctx, [[maybe_unused]] PropPool<PropertyHandle<Value>, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        auto array_this_p = dynamic_cast<Array*>(ctx->stack.at(passed_rsbp + argc).to_object());

        /// NOTE: By MDN, Array.prototype.pop returns the last element if possible, but this implementation returns undefined otherwise.
        if (array_this_p->items().empty()) {
            ctx->stack[passed_rsbp] = Value {};
        } else {
            ctx->stack[passed_rsbp] = array_this_p->items().back();
            array_this_p->items().pop_back();
        }

        return true;
    }

    [[nodiscard]] auto native_array_at(ExternVMCtx* ctx, [[maybe_unused]] PropPool<PropertyHandle<Value>, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        auto array_this_p = dynamic_cast<Array*>(ctx->stack.at(passed_rsbp + argc).to_object());
        auto actual_index_opt = ctx->stack[passed_rsbp].to_num_i32();
        const int items_n = array_this_p->items().size(); 

        /// NOTE: For now, just return undefined on non-numeric indices.
        if (!actual_index_opt) {
            ctx->stack[passed_rsbp] = Value {};
            return false;
        }

        /// NOTE: By MDN, Array.prototype.at returns from the 0th position for unsigned indices, BUT negative indices offset backwards from N (the length).
        if (auto actual_index = *actual_index_opt; actual_index >= 0 && actual_index < items_n) {
            ctx->stack[passed_rsbp] = array_this_p->items().at(actual_index);
        } else if (actual_index < 0 && items_n > 0) {
            ctx->stack[passed_rsbp] = array_this_p->items().at(items_n + actual_index);
        } else {
            // Handle negative index on empty array
            ctx->stack[passed_rsbp] = Value {};
        } 

        return true;
    }

    [[nodiscard]] auto native_array_index_of(ExternVMCtx* ctx, [[maybe_unused]] PropPool<PropertyHandle<Value>, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        auto array_this_p = dynamic_cast<Array*>(ctx->stack.at(passed_rsbp + argc).to_object());
        const auto& array_items_view = *array_this_p->get_seq_items();
        const auto& target_item = ctx->stack.at(passed_rsbp);
        auto found_pos = -1;

        for (int pos = 0; const auto& item_value : array_items_view) {
            if (item_value == target_item) {
                found_pos = pos;
                break;
            }

            pos++;
        }

        ctx->stack.at(passed_rsbp) = Value {found_pos};

        return true;
    }

    [[nodiscard]] auto native_array_reverse(ExternVMCtx* ctx, [[maybe_unused]] PropPool<PropertyHandle<Value>, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        auto array_this_p = dynamic_cast<Array*>(ctx->stack.at(passed_rsbp + argc).to_object());
        auto& array_items_view = *array_this_p->get_seq_items();

        std::ranges::reverse(array_items_view);

        ctx->stack[passed_rsbp] = Value {array_this_p};

        return true;
    }
}
