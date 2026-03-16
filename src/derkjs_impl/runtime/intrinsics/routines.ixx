module;

#include <utility>
#include <stdexcept>
#include <string>
#include <chrono>
#include <iostream>
#include <print>

export module runtime.intrinsics.routines;

import runtime.value;
import runtime.strings;
import runtime.context;

namespace DerkJS::Runtime::Intrinsics {
    //// BEGIN global functions:
    
    export auto native_parse_int(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        constexpr auto min_radix = 2;
        constexpr auto max_radix = 16;
        constexpr auto default_radix = 10;
        
        const int passed_rsbp = ctx->rsbp;
        std::string temp_str = ctx->stack.at(passed_rsbp + 1).to_string();
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
    
    export auto native_parse_float(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        std::string temp_str = ctx->stack.at(passed_rsbp + 1).to_string();
        
        try {
            ctx->stack.at(passed_rsbp - 1) = Value {std::stod(temp_str)};
        } catch (const std::exception& err) {
            std::println(std::cerr, "parseFloat: string is not a valid floating-point literal.");
            ctx->stack.at(passed_rsbp - 1) = Value {JSNaNOpt {}};
        }
        
        return true;
    }

    //// BEGIN console impls:

    export auto native_print(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const int passed_rsbp = ctx->rsbp;
        const char passed_terminator = static_cast<char>(ctx->stack.at(passed_rsbp + 2).to_num_i32().value_or(32) & 0xff);

        std::print("{}{}", ctx->stack.at(passed_rsbp + 1).to_string(), passed_terminator);
        ctx->stack.at(passed_rsbp - 1) = Value {JSUndefOpt {}};

        return true;
    }

    export auto native_read_line(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        /// NOTE: Show user the passed-in prompt string: It MUST be the 1st argument on the stack.
        const auto passed_rsbp = ctx->rsbp;
        const auto& prompt_value = ctx->stack.at(passed_rsbp + 1);

        std::print("{}", prompt_value.to_string());

        std::string temp_line;
        std::getline(std::cin, temp_line);

        ObjectBase<Value>* line_str_p = ctx->heap.add_item(
            ctx->heap.get_next_id(),
            DynamicString {
                ctx->builtins.at(static_cast<unsigned int>(BuiltInObjects::str)),
                Value {ctx->builtins.at(static_cast<unsigned int>(BuiltInObjects::extra_length_key))},
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

    export auto clock_time_now(ExternVMCtx* ctx, [[maybe_unused]] PropPool<Value, Value>* props, int argc) -> bool {
        const auto passed_rsbp = ctx->rsbp;
        const std::chrono::duration<double, std::milli> current_time_ms = std::chrono::steady_clock::now().time_since_epoch();

        /// NOTE: the VM's `djs_native_call` opcode will restore RSP -> callee RBP automtically, so the result would have to be placed there for other code.
        ctx->stack.at(passed_rsbp - 1) = Value {current_time_ms.count()};

        return true;
    }
}