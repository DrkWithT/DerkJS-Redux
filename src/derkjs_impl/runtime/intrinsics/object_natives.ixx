module;

#include <utility>
#include <algorithm>
#include <type_traits>
#include <memory>
#include <string>
#include <iostream>
#include <print>

export module runtime.intrinsics.object_natives;

import runtime.value;
import runtime.boolean;
import runtime.number;
import runtime.object;
import runtime.context;

namespace DerkJS::Runtime::Intrinsics {
    /// NOTE: Helps determine boxing JS object types for C++ "primitives"... @see `box_primitive_as_object()` ~ line 574.
    template <typename T>
    struct cxx_primitive_as_js_box_t {
        using type = void;
    };

    template <>
    struct cxx_primitive_as_js_box_t<bool> {
        using type = BooleanBox;
    };

    template <>
    struct cxx_primitive_as_js_box_t<int> {
        using type = NumberBox;
    };

    template <>
    struct cxx_primitive_as_js_box_t<double> {
        using type = NumberBox;
    };

    /// NOTE: Helps determine the appropriate prototype pointer for the primitive boxing object type... @see ~ line 574.
    template <typename T>
    constexpr auto js_primitive_box_proto_tag_v = BasePrototypeID::last; // dud terminator of enum class

    template <>
    constexpr auto js_primitive_box_proto_tag_v<BooleanBox> = BasePrototypeID::boolean;

    template <>
    constexpr auto js_primitive_box_proto_tag_v<NumberBox> = BasePrototypeID::number;

    /// Object.prototype impls:

    /// NOTE: This is a helper for `native_object_create()` ~ `natives.ixx:569`... This should make the logic to box booleans and numbers as objects on the heap before getting a non-owning pointer to "reference" them.
    /// TODO: This may need a refactor to treat all numbers as double-precision floats gahh
    export template <typename PrimitiveType>
    auto box_primitive_as_object(ExternVMCtx* ctx, const Value& argument_value) -> ObjectBase<Value>* {
        using deduced_derkjs_box_t = typename cxx_primitive_as_js_box_t<PrimitiveType>::type;

        constexpr auto boxing_prototype_id = static_cast<unsigned int>(js_primitive_box_proto_tag_v<deduced_derkjs_box_t>);

        ObjectBase<Value>* boxing_prototype_p = ctx->base_protos.at(boxing_prototype_id);

        if constexpr (std::is_same_v<deduced_derkjs_box_t, BooleanBox>) {
            return ctx->heap.add_item(
                ctx->heap.get_next_id(), std::make_unique<deduced_derkjs_box_t>(
                    argument_value.to_boolean(),
                    boxing_prototype_p
                )
            );
        } else if constexpr (std::is_same_v<deduced_derkjs_box_t, NumberBox>) {
            return ctx->heap.add_item(
                ctx->heap.get_next_id(), std::make_unique<deduced_derkjs_box_t>(
                    Value {argument_value.to_num_f64().value_or(0.0)},
                    boxing_prototype_p
                )
            );
        } else {
            return nullptr;
        }
    }

    /// TODO: fix this to wrap primitives in object form / forward objects as-is.
    export auto native_object_ctor(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;

        if (auto argument_value = ctx->stack.at(passed_rsbp + 1).deep_clone(); argc == 1) {
            switch (argument_value.get_tag()) {
            case ValueTag::boolean:
                ctx->stack.at(passed_rsbp - 1) = Value {
                    box_primitive_as_object<bool>(ctx, argument_value)
                };
                break;
            case ValueTag::num_i32:
            case ValueTag::num_f64:
                ctx->stack.at(passed_rsbp - 1) = Value {
                    box_primitive_as_object<double>(ctx, argument_value)
                };
                break;
            case ValueTag::object:
                ctx->stack.at(passed_rsbp - 1) = argument_value;
                break;
            default:
                ctx->stack.at(passed_rsbp - 1) = Value {JSNullOpt {}};
                break;
            }

            return true;
        }

        ctx->status = VMErrcode::bad_operation;
        return false;
    }

    export auto native_object_create(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        ObjectBase<Value>* taken_proto_p = ctx->stack.at(passed_rsbp + 1).to_object();

        if (auto result_p = ctx->heap.add_item(
            ctx->heap.get_next_id(),
            std::make_unique<Object>(taken_proto_p)
        ); result_p) {
            ctx->stack.at(passed_rsbp - 1) = Value {result_p};
            return true;
        }

        ctx->status = VMErrcode::bad_heap_alloc;
        std::println(std::cerr, "Failed to allocate JS object on the heap.");
        return false;
    }

    export auto native_object_has_own_property(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        ObjectBase<Value>* target_this_p = ctx->stack.at(passed_rsbp - 1).to_object();

        if (!target_this_p) {
            ctx->status = VMErrcode::bad_operation;
            std::println(std::cerr, "Object.hasOwnProperty: cannot invoke for non-object.");
            return false;
        }

        const auto& search_key = ctx->stack.at(passed_rsbp + 1);
        const auto& target_items = target_this_p->get_own_prop_pool();

        ctx->stack.at(passed_rsbp - 1) = Value {
            std::ranges::find_if(
                target_items,
                [&search_key] (const auto& prop_entry) -> bool {
                    return search_key == prop_entry.key;
                }
            ) != target_items.end()
        };

        return true;
    }

    export auto native_object_is_prototype_of(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        ObjectBase<Value>* self_maybe_prototype_p = ctx->stack.at(passed_rsbp - 1).to_object();
        ObjectBase<Value>* current_prototype_p = ctx->stack.at(passed_rsbp + 1).to_object()->get_prototype();
        auto found_in_prototype_chain = false;

        while (current_prototype_p != nullptr) {
            if (self_maybe_prototype_p == current_prototype_p) {
                found_in_prototype_chain = true;
                break;
            }

            current_prototype_p = current_prototype_p->get_prototype();
        }

        ctx->stack.at(passed_rsbp - 1) = Value {found_in_prototype_chain};
        return true;
    }

    export auto native_object_get_prototype_of(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;

        if (auto target_object_ptr = ctx->stack.at(passed_rsbp + 1).to_object(); target_object_ptr && argc == 1) {
            ctx->stack.at(passed_rsbp - 1) = Value {
                target_object_ptr->get_prototype()
            };
            return true;
        }

        std::println(std::cerr, "Object.getPrototypeOf: expected object argument (cannot be primitive, null, or undefined.)");
        ctx->status = VMErrcode::bad_operation;
        return false;
    }

    export auto native_object_freeze(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        ObjectBase<Value>* target_this_p = (argc == 0)
            ? ctx->stack.at(passed_rsbp - 1).to_object()
            : ctx->stack.at(passed_rsbp + 1).to_object();

        if (target_this_p != nullptr) {
            target_this_p->freeze();
        }

        return true;
    }
}
