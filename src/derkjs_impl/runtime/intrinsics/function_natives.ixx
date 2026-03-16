module;

#include <utility>
#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include <iostream>
#include <print>

export module runtime.intrinsics.function_natives;

import runtime.value;
import runtime.object;
import runtime.op_handlers;

namespace DerkJS::Runtime::Intrinsics {
    /// Function.prototype impls.

    export auto native_function_dud(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, [[maybe_unused]] int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;

        ctx->stack.at(passed_rsbp - 1) = Value {JSUndefOpt {}};

        return true;
    }

    export auto native_function_ctor(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        std::span<Value> passed_args {ctx->stack.begin() + passed_rsbp + 1, ctx->stack.begin() + passed_rsbp + 1 + argc};

        auto callable_object_p = ctx->compile_proc(
            ctx->lexer_p, ctx->parser_p, ctx->compile_state_p, &ctx->heap,
            passed_args
        );

        if (!callable_object_p) {
            ctx->status = VMErrcode::bad_operation;
            std::println(std::cerr, "Failed to allocate Function object on the heap.");
            return false;
        }

        ctx->stack.at(passed_rsbp - 1) = Value { callable_object_p };

        return true;
    }

    export auto native_function_call(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
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
        /// NOTE: For the change in ES3, an undefined / null thisArg must default to globalThis, accessible in DerkJS via ctx->stack.at(0). TODO: remove implicit globalThis for null / undefined thisArg...
        // if (const auto this_arg_tag = ctx->stack.at(passed_rsbp).get_tag(); this_arg_tag == ValueTag::undefined || this_arg_tag == ValueTag::null) {
        //     ctx->stack.at(passed_rsbp) = ctx->stack.at(0);
        // }

        if (!maybe_callable_p->call(ctx, argc - 1, true)) {
            std::println(std::cerr, "Function.call: Cannot invoke non-function object.");
            ctx->status = VMErrcode::bad_operation;
            return false;
        }

        dispatch_op(*ctx);

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