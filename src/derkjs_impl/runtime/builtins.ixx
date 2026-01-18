module;

#include <string>
#include <chrono>
#include <iostream>
#include <print>

export module runtime.builtins;

import runtime.objects;
import runtime.value;
import runtime.strings;
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
        const auto passed_rbsp = ctx->rsbp;
        std::print("{}", ctx->stack[passed_rbsp].to_string().value());

        std::string temp_line;
        std::getline(std::cin, temp_line);

        if (auto line_str_p = ctx->heap.add_item(ctx->heap.get_next_id(), DynamicString {std::move(temp_line)}); !line_str_p) {
            return false;
        } else {
            ctx->stack[passed_rbsp] = Value {line_str_p};
            return true;
        }
    }

    //// BEGIN time impls:

    /// TODO: probably add support for passing an argument which specifies precision: seconds, ms, ns
    [[nodiscard]] auto clock_time_now(ExternVMCtx* ctx, [[maybe_unused]] PropPool<PropertyHandle<Value>, Value>* props, int argc) -> bool {
        const std::chrono::duration<double, std::milli> current_time_ms = std::chrono::steady_clock::now().time_since_epoch();

        /// NOTE: the VM's `djs_native_call` opcode will restore RSP -> callee RBP automtically, so the result would have to be placed there for other code.
        ctx->stack[ctx->rsbp] = Value {current_time_ms.count()};

        return true;
    }
}
