module;

#include <cstdint>
#include <utility>
#include <memory>
#include <string>
#include <array>
#include <vector>
#include <span>
#include <span>

export module runtime.context;

import runtime.value;
import runtime.strings;
import runtime.object;
import runtime.bytecode;
import runtime.gc;

namespace DerkJS {
    export enum class VMErrcode : uint8_t {
        pending,
        bad_property_access,
        bad_operation,
        bad_heap_alloc,
        vm_abort,
        uncaught_error,
        ok,
        last
    };

    /**
     * @brief Provides the bytecode VM's call frame type. The full specializations by `DispatchPolicy` are meant for use.
     * @see `DispatchPolicy` for more information about how opcodes would be dispatched.
     * @tparam Dp 
     */
    export struct CallFrame {
        const Instruction* m_caller_ret_ip;
        ObjectBase<Value>* caller_addr;
        ObjectBase<Value>* capture_p;
        ObjectBase<Value>* pack_array_p;
        int m_callee_sbp;
        int m_caller_sbp;
        uint8_t m_flags;

        template <CallFlags F>
        friend constexpr auto derkjs_call_flag(const CallFrame& frame) noexcept -> bool {
            if constexpr (F == CallFlags::is_ctor) {
                return frame.m_flags & std::to_underlying(F);
            }

            return false;
        }
    };

    /// NOTE: This type decouples the internal state of the bytecode VM.
    export struct ExternVMCtx {
        using call_frame_type = CallFrame;
        using runtime_object_ptr = ObjectBase<Value>*;
        using compile_snippet_fn = runtime_object_ptr (*) (void*, void*, void*, void*, const std::span<Value>&);

        GC gc;
        PolyPool<ObjectBase<Value>> heap;
        std::array<ObjectBase<Value>*, static_cast<std::size_t>(BuiltInObjects::last)> builtins;
        std::vector<Value> stack;
        std::vector<CallFrame> frames;

        void* lexer_p;
        void* parser_p;
        void* compile_state_p;
        compile_snippet_fn compile_proc;

        Value* consts_view;

        /// NOTE: holds a non-owning pointer to a currently thrown exception object e.g `ErrorXYZ`.
        ObjectBase<Value>* current_error;

        /// NOTE: holds base of bytecode blob, starts at position 0 to access offsets from.
        const Instruction* code_bp;

        /// NOTE: holds base of an index array mapping function IDs to their code starts.
        const int* fn_table_bp;

        /// NOTE: holds direct pointer to an `Instruction`.
        const Instruction* rip_p;

        std::size_t ending_frame_depth;

        /// NOTE: holds stack base pointer for call locals
        int16_t rsbp;

        /// NOTE: holds stack top pointer
        int16_t rsp;

        // int16_t dispatch_allowance;

        VMErrcode status;

        ExternVMCtx(Program& prgm, std::size_t stack_length_limit, std::size_t call_frame_limit, std::size_t gc_heap_threshold, void* lexer_ptr, void* parser_ptr, void* compile_state_ptr, compile_snippet_fn compile_proc_ptr)
        : gc {gc_heap_threshold}, heap (std::move(prgm.heap_items)), builtins(std::move(prgm.builtins)), stack {}, frames {}, lexer_p {lexer_ptr}, parser_p {parser_ptr}, compile_state_p {compile_state_ptr}, compile_proc {compile_proc_ptr}, consts_view {prgm.consts.data()}, current_error {nullptr}, code_bp {prgm.code.data()}, fn_table_bp {prgm.offsets.data()}, rip_p {prgm.code.data() + prgm.offsets[prgm.entry_func_id]}, ending_frame_depth {0}, rsbp {-1}, rsp {-1}, /*dispatch_allowance {100},*/ status {VMErrcode::pending} {
            stack.reserve(stack_length_limit);
            stack.resize(stack_length_limit);
            frames.reserve(call_frame_limit);

            if (auto global_this_p = heap.add_item(heap.get_next_id(), std::make_unique<Object>(nullptr)); !global_this_p) {
                status = VMErrcode::bad_heap_alloc;
            } else {
                frames.emplace_back(CallFrame {
                    .m_caller_ret_ip = nullptr,
                    .caller_addr = nullptr,
                    .capture_p = global_this_p,
                    .pack_array_p = nullptr,
                    .m_callee_sbp = 1,
                    .m_caller_sbp = -1,
                });
                // 2. Push globalThis for implicit main code...
                ++rsp;
                stack.at(rsp) = Value {global_this_p};
                // 3. Push undefined as dud for implicit main function- Which cannot directly self call anyways.
                ++rsp;
                stack.at(rsp) = Value {JSUndefOpt {}};
                rsbp = rsp;
            }
        }

        /// NOTE: Only use this to push an exception object for opcodes corresponding to JS operators e.g TypeError on an invalid `djs_get_prop`. Returns `true` if the error object was allocated AND the error ctor ID was valid.
        [[nodiscard]] auto prepare_error(std::string msg, std::uint8_t builtin_id) -> bool {
            rsp++;
            stack.at(rsp) = Value {JSUndefOpt {}}; // thisArg placeholder BELOW callee

            rsp++;
            stack.at(rsp) = Value { builtins.at(builtin_id) };

            if (auto msg_string_p = heap.add_item(heap.get_next_id(), new DynamicString {
                builtins.at(static_cast<unsigned int>(BuiltInObjects::str)),
                Value {builtins.at(static_cast<unsigned int>(BuiltInObjects::extra_length_key))},
                std::move(msg)
            }); msg_string_p != nullptr) {
                rsp++;
                stack.at(rsp) = Value {msg_string_p};
            } else {
                status = VMErrcode::bad_heap_alloc;
                return false;
            }

            return true;
        }

        /// NOTE: Only use this for "throwing" exceptions in the VM!
        [[nodiscard]] auto try_recover(ObjectBase<Value>* error_ptr, bool is_throw_within_try) -> VMErrcode {
            current_error = error_ptr;

            auto search_for_catch = is_throw_within_try;
            auto peek_rip_p = rip_p;

            for (; !frames.empty() && peek_rip_p != nullptr; peek_rip_p++) {
                if (search_for_catch && peek_rip_p->op == Opcode::djs_catch) {
                    rip_p = peek_rip_p;
                    return VMErrcode::pending;
                }

                if (const auto& [caller_ret_ip, caller_addr, caller_capture_p, pack_array_ptr, callee_sbp, caller_sbp, calling_flags] = frames.back(); peek_rip_p->op == Opcode::djs_ret) {
                    rsbp = caller_sbp;
                    rsp = callee_sbp;
                    rip_p = caller_ret_ip;
                    peek_rip_p = caller_ret_ip;

                    frames.pop_back();

                    search_for_catch = true;
                }
            }

            return VMErrcode::uncaught_error;
        }

        [[nodiscard]] auto push_string(const std::string& s, const int passed_rsbp) noexcept -> bool {
            if (auto temp_str_p = heap.add_item(heap.get_next_id(), std::make_unique<DynamicString>(
                stack.at(passed_rsbp).to_object()->get_instance_prototype(),
                builtins.at(static_cast<unsigned int>(BuiltInObjects::extra_length_key)),
                s
            )); temp_str_p) {
                stack.at(passed_rsbp - 1) = Value {temp_str_p};
                return true;
            }

            status = VMErrcode::bad_heap_alloc;   
            return false;
        }
    };
}