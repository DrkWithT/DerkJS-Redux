module;

#include <cstdint>
#include <utility>
#include <algorithm>
#include <vector>

export module runtime.vm;

import runtime.value;
import runtime.bytecode;
export import runtime.op_handlers;

namespace DerkJS {
    /**
     * @brief Provides the general bytecode VM type.
     */
    export class VM {
    public:
        /// NOTE: Only modify this from DerkJS native functions.
        ExternVMCtx m_ctx;

        explicit VM(Program& prgm, std::size_t stack_length_limit, std::size_t call_frame_limit, std::size_t gc_threshold, void* lexer_ptr, void* parser_ptr, void* compile_state_ptr, ExternVMCtx::compile_snippet_fn compile_proc_ptr)
        : m_ctx {prgm, stack_length_limit, call_frame_limit, gc_threshold, lexer_ptr, parser_ptr, compile_state_ptr, compile_proc_ptr} {
            //? Mark all contiguous object slots in the fresh heap as permanently tenured to prevent GC sweeps of built-ins or string constants.
            m_ctx.heap.tenure_items();
        }
∂
        [[nodiscard]] auto peek_final_result() const noexcept -> const Value& {
            return m_ctx.stack[0];
        }

        [[nodiscard]] auto peek_status(this auto&& self) noexcept -> VMErrcode {
            return self.m_ctx.status;
        }

        [[nodiscard]] auto peek_leftover_error() const noexcept -> Value {
            return Value {m_ctx.current_error};
        }

        inline void run() {
            return dispatch_op(m_ctx);
        }
    };
}
