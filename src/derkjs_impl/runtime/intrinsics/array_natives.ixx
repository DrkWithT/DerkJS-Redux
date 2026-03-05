module;

#include <utility>
#include <memory>
#include <string>
#include <string_view>
#include <sstream>
#include <iostream>
#include <print>

export module runtime.intrinsics.array_natives;

import runtime.value;
import runtime.arrays;
import runtime.context;

namespace DerkJS::Runtime::Intrinsics {
    /// BEGIN Array.prototype impls:

    export auto native_array_ctor(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        ObjectBase<Value>* instance_prototype_p = ctx->stack.at(passed_rsbp).to_object()->get_instance_prototype();
        int maybe_fill_count = ctx->stack.at(passed_rsbp + 1).to_num_i32().value_or(0);

        if (argc == 0) {
            maybe_fill_count = 0;
        } else if (argc == 1) {
            maybe_fill_count = std::abs(maybe_fill_count);
        }

        // 1. Create blank JS array with the appropriate prototype
        auto temp_array = std::make_unique<Array>(
            instance_prototype_p,
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

    export auto native_array_push(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
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
    export auto native_array_join(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        std::ostringstream sout;

        const int passed_rsbp = ctx->rsbp;
        auto array_this_p = dynamic_cast<Array*>(
            ctx->stack.at(passed_rsbp - 1).to_object()
        );

        if (!array_this_p->items().empty()) {
            std::string delim = (argc == 1) ? ctx->stack.at(passed_rsbp + 1).to_string() : ",";

            sout << array_this_p->items().front().to_string();

            for (int more_count = 1, self_len = array_this_p->items().size(); more_count < self_len; more_count++) {
                sout << delim;

                if (const auto& next_item = array_this_p->items().at(more_count); next_item.get_tag() != ValueTag::undefined && next_item.get_tag() != ValueTag::null) {
                    sout << next_item.to_string();
                }
            }
        }

        return ctx->push_string(sout.str(), passed_rsbp);
    }
}