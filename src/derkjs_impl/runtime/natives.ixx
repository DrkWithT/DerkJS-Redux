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
        const int passed_rsbp = ctx->rsbp;
        std::string temp_str = ctx->stack[passed_rsbp].to_string().value_or("");

        try {
            ctx->stack[passed_rsbp] = Value {std::stoi(temp_str)};
        } catch (const std::exception& err) {
            std::println(std::cerr, "parseInt: String is not a valid integer literal.");
            ctx->stack[passed_rsbp] = Value {JSNaNOpt {}};
        }

        return true;
    }

    /// BEGIN string impls:

    [[nodiscard]] auto native_str_charcode_at(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        const auto str_this_p = dynamic_cast<const StringBase*>(
            (ctx->stack[passed_rsbp + argc].get_tag() == ValueTag::val_ref)
                ? ctx->stack[passed_rsbp + argc].get_value_ref()->to_object()
                : ctx->stack[passed_rsbp + argc].to_object()
        );
        const int str_at_pos = ctx->stack[passed_rsbp].to_num_i32().value_or(-1);
        std::string_view chars_view = str_this_p->as_str_view();

        if (const int chars_n = chars_view.length(); str_at_pos < 0 || str_at_pos >= chars_n) {
            ctx->stack[passed_rsbp] = Value {JSNaNOpt {}}; // get undefined for invalid char positions
        } else {
            ctx->stack[passed_rsbp] = Value {static_cast<int>(chars_view[str_at_pos])};
        }

        return true;
    }

    [[nodiscard]] auto native_str_len(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        const auto str_this_p = dynamic_cast<const StringBase*>(ctx->stack[passed_rsbp + argc].to_object());

        if (!str_this_p) {
            std::println(std::cerr, "Cannot get string length of non-string.");
            ctx->stack[passed_rsbp] = Value {JSNaNOpt {}};
            return false;
        }

        const int str_len_i32 = str_this_p->as_str_view().length();

        ctx->stack[passed_rsbp] = Value {str_len_i32};

        return true;
    }

    [[nodiscard]] auto native_str_substr(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        auto str_as_obj_p = ctx->stack[passed_rsbp + argc].to_object();
        const auto str_this_p = dynamic_cast<const StringBase*>(str_as_obj_p);
        const int substr_begin = ctx->stack[passed_rsbp].to_num_i32().value_or(0);
        const int substr_len = ctx->stack[passed_rsbp + 1].to_num_i32().value_or(0);

        std::string_view chars_view = str_this_p->as_str_view().substr(substr_begin, substr_len);
        std::string temp_substr;

        for (auto c : chars_view) {
            temp_substr += c;
        }

        if (auto created_str_p = ctx->heap.add_item(ctx->heap.get_next_id(), DynamicString {str_as_obj_p->get_prototype(), std::move(temp_substr)}); created_str_p) {
            ctx->stack[passed_rsbp] = Value {created_str_p};
            return true;
        } else {
            std::println(std::cerr, "Failed to allocate JS sub-string on the heap.");
            ctx->stack[passed_rsbp] = Value {JSNullOpt {}};
            return false;
        }
    }

    //// BEGIN console impls:

    [[nodiscard]] auto native_console_log(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;

        for (auto passed_arg_local_offset = 0; passed_arg_local_offset < argc; ++passed_arg_local_offset) {
            std::print("{} ", ctx->stack[passed_rsbp + passed_arg_local_offset].to_string().value());
        }

        std::println();

        return true;
    }

    [[nodiscard]] auto native_console_read_line(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {    
        /// NOTE: Show user the passed-in prompt string: It MUST be the 1st argument on the stack.
        const auto passed_rsbp = ctx->rsbp;
        const auto prompt_str_p = ctx->stack[passed_rsbp].to_object();
        ObjectBase<Value>* prompt_str_prototype = prompt_str_p->get_prototype();

        if (!prompt_str_p || prompt_str_p->get_class_name() != "string") {
            ctx->stack[passed_rsbp] = Value {};
            return false;
        }

        std::print("{}", prompt_str_p->as_string());

        std::string temp_line;
        std::getline(std::cin, temp_line);

        ObjectBase<Value>* line_str_p = ctx->heap.add_item(ctx->heap.get_next_id(), DynamicString { prompt_str_prototype, std::move(temp_line)});

        if (!line_str_p) {
            std::println(std::cerr, "Failed to allocate input string on the heap.");
            ctx->stack[passed_rsbp] = Value {};
            return false;
        } else {            
            ctx->stack[passed_rsbp] = Value {line_str_p};
            return true;
        }
    }

    //// BEGIN time impls:

    /// TODO: probably add support for passing an argument which specifies precision: seconds, ms, ns
    [[nodiscard]] auto clock_time_now(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        const std::chrono::duration<double, std::milli> current_time_ms = std::chrono::steady_clock::now().time_since_epoch();

        /// NOTE: the VM's `djs_native_call` opcode will restore RSP -> callee RBP automtically, so the result would have to be placed there for other code.
        ctx->stack[passed_rsbp] = Value {current_time_ms.count()};

        return true;
    }

    // [[nodiscard]] auto native_process_getenv(ExternVMCtx* ctx, PropPool<Value, Value>* props, int argc) -> bool {
    //     return false; // TODO
    // }

    /// BEGIN Array.prototype impls:

    [[nodiscard]] auto native_array_ctor(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;

        // 1. Create blank JS array with the appropriate prototype
        auto temp_array = std::make_unique<Array>(
            ctx->base_protos.at(static_cast<unsigned int>(BasePrototypeID::array))
        );

        // 2. Fill with N temporary arguments OR fill with N undefined values.
        if (argc > 1) {
            for (int temp_item_offset = 0; temp_item_offset < argc; temp_item_offset++) {
                temp_array->items().emplace_back(ctx->stack[passed_rsbp + temp_item_offset]);
            }
        } else if (argc == 1) {
            const int fill_count = ctx->stack[passed_rsbp].to_num_i32().value_or(0);

            for (int temp_item_count = 0; temp_item_count < fill_count; temp_item_count++) {
                temp_array->items().emplace_back(Value {});
            }
        } else {
            ;
        }

        // 3. Record array to VM heap
        ObjectBase<Value>* temp_array_p = ctx->heap.add_item(ctx->heap.get_next_id(), std::move(temp_array));

        if (temp_array_p) {
            // 3. Put reference to array
            ctx->stack[passed_rsbp] = Value {temp_array_p};
            return true;
        } else {
            std::println(std::cerr, "Failed to allocate JS array on the heap.");
            ctx->stack[passed_rsbp] = Value {};
            return false;
        }
    }

    [[nodiscard]] auto native_array_push(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
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

    [[nodiscard]] auto native_array_pop(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
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

    [[nodiscard]] auto native_array_at(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
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

    [[nodiscard]] auto native_array_index_of(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
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
    
    /// SEE: ES5-15.4.4.14
    [[nodiscard]] auto native_array_last_index_of(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        auto array_this_p = dynamic_cast<Array*>(ctx->stack.at(passed_rsbp + argc).to_object());

        if (!array_this_p) {
            std::println(std::cerr, "Array.lastIndexOf: Non-array argument.");
            ctx->status = VMErrcode::bad_operation;
            return false;
        }

        if (array_this_p->items().empty()) {
            ctx->stack.at(passed_rsbp) = Value {-1};
            return true;
        }
        
        const auto& target = ctx->stack.at(passed_rsbp);
        const int self_len = array_this_p->items().size();
        int search_pos = (argc == 2) ? ctx->stack.at(passed_rsbp).to_num_i32().value_or(0) : self_len - 1 ;
        search_pos = (search_pos < 0) ? self_len - std::abs(search_pos) : std::min(search_pos, self_len - 1);

        for (; search_pos >= 0; search_pos--) {
            if (array_this_p->items().at(search_pos) == target) {
                ctx->stack.at(passed_rsbp) = Value {search_pos};
                return true;
            }
        }

        ctx->stack.at(passed_rsbp) = Value {-1};
        return true;
    }

    [[nodiscard]] auto native_array_len(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        auto array_this_p = dynamic_cast<const Array*>(ctx->stack[passed_rsbp + argc].to_object());

        const int array_len_i32 = array_this_p->items().size();

        ctx->stack[passed_rsbp] = Value {array_len_i32};

        return true;
    }

    [[nodiscard]] auto native_array_reverse(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        auto array_this_p = dynamic_cast<Array*>(ctx->stack.at(passed_rsbp + argc).to_object());
        auto& array_items_view = array_this_p->items();

        std::ranges::reverse(array_items_view);

        ctx->stack[passed_rsbp] = Value {array_this_p};

        return true;
    }

    /// SEE: ES5-15.4.4.5
    [[nodiscard]] auto native_array_join(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        auto array_this_p = dynamic_cast<const Array*>(ctx->stack[passed_rsbp + argc].to_object());
        auto temp_str = std::make_unique<DynamicString>(ctx->base_protos[static_cast<unsigned int>(BasePrototypeID::str)], std::string_view {});

        if (array_this_p->items().empty()) {
            auto empty_str_p = ctx->heap.add_item(ctx->heap.get_next_id(), std::move(temp_str));

            if (!empty_str_p) {
                ctx->status = VMErrcode::bad_heap_alloc;
                return false;
            }

            ctx->stack.at(passed_rsbp) = Value {empty_str_p};

            return true;
        }

        std::string delim = (argc == 1) ? ctx->stack.at(passed_rsbp).to_string().value_or(",") : ",";
        
        temp_str->append_back(array_this_p->items().front().to_string().value());

        for (int more_count = 1, self_len = array_this_p->items().size(); more_count < self_len; more_count++) {
            temp_str->append_back(delim);

            if (const auto& next_item = array_this_p->items().at(more_count); next_item.get_tag() != ValueTag::undefined && next_item.get_tag() != ValueTag::null) {
                temp_str->append_back(next_item.to_string().value());
            }
        }

        if (auto filled_str_p = ctx->heap.add_item(ctx->heap.get_next_id(), std::move(temp_str)); filled_str_p) {
            ctx->stack.at(passed_rsbp) = Value {filled_str_p};
            return true;
        } else {
            ctx->status = VMErrcode::bad_heap_alloc;
            return false;
        }
    }

    [[nodiscard]] auto native_array_concat(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        std::println(std::cerr, "Array.concat: Not implemented.");
        return false; // TODO!
    }

    /// Object.prototype impls:

    /// TODO: fix this to wrap primitives in object form / forward objects as-is.
    [[nodiscard]] auto native_object_ctor(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        Value target_arg = ctx->stack.at(passed_rsbp);

        if (auto target_as_object_p = target_arg.to_object(); !target_as_object_p) {
            std::println(std::cerr, "Object.constructor: Non-object arguments to Object ctor are unsupported.");
            ctx->status = VMErrcode::bad_operation;
            return false;
        } else {
            ctx->stack.at(passed_rsbp) = Value {target_as_object_p};
            return true;
        }
    }

    [[nodiscard]] auto native_object_create(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        auto taken_proto_p = ctx->stack.at(passed_rsbp).to_object();

        if (auto result_p = ctx->heap.add_item(ctx->heap.get_next_id(), std::make_unique<Object>(taken_proto_p)); result_p) {
            ctx->stack.at(passed_rsbp) = Value {result_p};
            return true;
        } else {
            std::println(std::cerr, "Failed to allocate JS object on the heap.");
            ctx->stack.at(passed_rsbp) = Value {JSNullOpt {}};
            ctx->status = VMErrcode::bad_heap_alloc;
            return false;
        }
    }

    [[nodiscard]] auto native_object_freeze(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        auto target_object_p = ctx->stack.at(passed_rsbp).to_object();

        if (target_object_p != nullptr) {
            target_object_p->freeze();
        }

        return true;
    }
}
