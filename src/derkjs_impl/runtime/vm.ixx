module;

#include <cstdint>
#include <type_traits>
#include <utility>

#include <algorithm>
#include <vector>

export module runtime.vm;

import runtime.objects;
import runtime.value;
import runtime.strings;
import runtime.bytecode;

export namespace DerkJS {
    /// NOTE: This scoped enum enables instantiation of a specialized VM that either does loop-switch or tail-call-friendly control flow when running opcodes.
    enum class DispatchPolicy : uint8_t {
        loop_switch,
        tco,
    };

    /**
     * @brief Provides the bytecode VM's call frame type. The full specializations by `DispatchPolicy` are meant for use.
     * @see `DispatchPolicy` for more information about how opcodes would be dispatched.
     * @tparam Dp 
     */
    template <DispatchPolicy Dp>
    struct CallFrame;

    template <>
    struct CallFrame<DispatchPolicy::loop_switch> {
        int m_callee_sbp;
        int m_caller_sbp;
        int m_caller_id;
        int m_caller_ret_ip;

        constexpr CallFrame() noexcept
        : m_callee_sbp {0}, m_caller_sbp {0}, m_caller_id {-1}, m_caller_ret_ip {-1} {}

        constexpr CallFrame(int16_t callee_sbp, int16_t caller_sbp, int16_t caller_id, int16_t caller_ret_ip) noexcept
        : m_callee_sbp {callee_sbp}, m_caller_sbp {caller_sbp}, m_caller_id {caller_id}, m_caller_ret_ip {caller_ret_ip} {}

        explicit operator bool(this auto&& self) noexcept {
            return self.m_caller_ip < 0 || self.m_caller_ret_ip < 0;
        }
    };

    template <>
    struct CallFrame<DispatchPolicy::tco> {
        const Instruction* m_caller_ret_ip;
        ObjectBase<Value>* m_this_p;
        int m_callee_sbp;
        int m_caller_sbp;
    };

    #ifdef __clang__
        #ifdef __llvm__
            #if __clang_major__ >= 20
                /// NOTE: Compile-time flag to check if TCO is enabled for the build- This is only enabled on LLVM Clang 20+ to be safe.
                constexpr bool is_tco_enabled_v = true;
            #else
                /// NOTE: Compile-time flag to check if TCO is enabled for the build- This is only enabled on LLVM Clang 20+ to be safe.
                constexpr bool is_tco_enabled_v = false;
            #endif
        #else
            /// NOTE: Compile-time flag to check if TCO is enabled for the build- This is only enabled on LLVM Clang 20+ to be safe.
            constexpr bool is_tco_enabled_v = false;
        #endif
    #else
        /// NOTE: Compile-time flag to check if TCO is enabled for the build- This is only enabled on LLVM Clang 20+ to be safe.
        constexpr bool is_tco_enabled_v = false;
    #endif

    /// NOTE: This type decouples the internal state of the bytecode VM. It's used when the `DispatchPolicy::tco` specialization is used with a opcode dispatch table and dispatch TCO'd function internally apply in `VM<DispatchPolicy::tco>`. However, this is only enabled on LLVM Clang 20+ to be safe.
    struct ExternVMCtx {
        using call_frame_type = CallFrame<DispatchPolicy::tco>;

        PolyPool<ObjectBase<Value>> heap;
        std::vector<Value> stack;
        std::vector<call_frame_type> frames;

        Value* consts_view;

        /// NOTE: holds base of bytecode blob, starts at position 0 to access offsets from.
        const Instruction* code_bp;

        /// NOTE: holds base of an index array mapping function IDs to their code starts.
        const int* fn_table_bp;

        /// NOTE: holds direct pointer to an `Instruction`.
        const Instruction* rip_p;

        /// NOTE: holds stack base pointer for call locals
        int16_t rsbp;

        /// NOTE: holds stack top pointer
        int16_t rsp;

        bool has_err;

        ExternVMCtx(Program& prgm, std::size_t stack_length_limit, std::size_t call_frame_limit)
        : heap (std::move(prgm.heap_items)), stack {}, frames {}, consts_view {prgm.consts.data()}, code_bp {prgm.code.data()}, fn_table_bp {prgm.offsets.data()}, rip_p {prgm.code.data() + prgm.offsets[prgm.entry_func_id]}, rsbp {0}, rsp {-1}, has_err {false} {
            stack.reserve(stack_length_limit);
            stack.resize(stack_length_limit);
            frames.reserve(call_frame_limit);

            if (auto global_this_p = heap.add_item(heap.get_next_id(), std::make_unique<Object>(nullptr)); !global_this_p) {
                has_err = true;
            } else {
                frames.emplace_back(call_frame_type {
                    .m_caller_ret_ip = nullptr,
                    .m_this_p = global_this_p, // TODO: add globalThis & working global prop assignments...
                    .m_callee_sbp = 0,
                    .m_caller_sbp = 0,
                });
            }
        }
    };

    /**
     * @brief Provides the general bytecode VM type. Actual specializations via DispatchPolicy are meant to be used.
     * 
     * @tparam Dp Determines how bytecode opcodes are dispatched, either by loop-switch or tail-call optimization-friendly recursion. Tail call optimization (TCO) allows a slight speedup in the VM over a typical switch-loop by avoiding some overhead of creating / unwinding call frames.
     */
    template <DispatchPolicy Dp>
    class VM;

    template <>
    class VM<DispatchPolicy::loop_switch> {
    private:
        using call_frame_type = CallFrame<DispatchPolicy::loop_switch>;

        // GCHeap<Object<Value>> m_heap;
        std::vector<Value> m_stack;
        std::vector<call_frame_type> m_frames;

        Value* m_consts_view;

        /// NOTE: holds base of bytecode blob, starts at position 0 to access offsets from.
        const Instruction* m_code_bp;

        /// NOTE: holds base of an index array mapping function IDs to their code starts.
        const int* m_fn_table_bp;

        /// NOTE: holds direct pointer to an `Instruction`.
        const Instruction* m_rip_p;

        std::size_t m_frames_free;

        /// NOTE: holds stack base pointer for call locals
        int16_t m_rsbp;

        /// NOTE: holds stack top pointer
        int16_t m_rsp;

        /// NOTE: holds current bytecode chunk ID (caller's code)
        int16_t m_rcid;

        /// NOTE: indicates an error condition during execution, returning from the VM invocation
        ExitStatus m_status;

        void op_dup(int16_t temp_offset) noexcept {
            const auto& src_value = m_stack[m_rsbp + temp_offset];

            ++m_rsp;
            m_stack[m_rsp] = src_value;

            ++m_rip_p;
        }

        void op_push_const(int16_t const_id) noexcept {
            ++m_rsp;
            m_stack[m_rsp] = m_consts_view[const_id];

            ++m_rip_p;
        }

        void op_pop(int16_t pop_n) noexcept {
            lazy_pop_values(pop_n);
            ++m_rip_p;
        }

        void op_emplace(int16_t local_n) noexcept {
            m_stack[m_rsbp + local_n] = m_stack[m_rsp];
            --m_rsp;
            ++m_rip_p;
        }

        void op_dup() noexcept {
            m_status = ExitStatus::opcode_err;
        }

        void op_mod() noexcept {
            m_stack[m_rsp] %= m_stack[m_rsp - 1];
            std::swap(m_stack[m_rsp - 1], m_stack[m_rsp]);
            --m_rsp;

            ++m_rip_p;
        }

        void op_mul() noexcept {
            m_stack[m_rsp] *= m_stack[m_rsp - 1];
            std::swap(m_stack[m_rsp - 1], m_stack[m_rsp]);
            --m_rsp;

            ++m_rip_p;
        }

        void op_div() noexcept {
            // E: 4 / 2: --> 4 / 2: 
            // T: LHS: 10    LHS: 5
            // B: RHS: 2     RHS: (bubble down LHS here)
            m_stack[m_rsp] /= m_stack[m_rsp - 1];
            std::swap(m_stack[m_rsp - 1], m_stack[m_rsp]);
            --m_rsp;

            ++m_rip_p;
        }

        void op_add() noexcept {
            m_stack[m_rsp] += m_stack[m_rsp - 1];
            std::swap(m_stack[m_rsp - 1], m_stack[m_rsp]);
            --m_rsp;

            ++m_rip_p;
        }

        void op_sub() noexcept {
            m_stack[m_rsp] -= m_stack[m_rsp - 1];
            std::swap(m_stack[m_rsp - 1],  m_stack[m_rsp]);
            --m_rsp;

            ++m_rip_p;
        }

        void op_test_falsy() noexcept {
            const auto& tested_value = m_stack[m_rsp];

            ++m_rsp;
            m_stack[m_rsp] = tested_value.is_falsy();
            ++m_rip_p;
        }

        void op_test_strict_eq() noexcept {
            const auto& lhs = m_stack[m_rsp];

            --m_rsp;
            m_stack[m_rsp] = lhs == m_stack[m_rsp];
            ++m_rip_p;
        }

        void op_test_strict_ne() noexcept {
            const auto& lhs = m_stack[m_rsp];
            
            --m_rsp;
            m_stack[m_rsp] = lhs != m_stack[m_rsp];
            ++m_rip_p;
        }

        void op_test_lt() {
            const auto& lhs = m_stack[m_rsp];
            
            --m_rsp;
            m_stack[m_rsp] = lhs < m_stack[m_rsp];
            ++m_rip_p;
        }

        void op_test_lte() {
            const auto& lhs = m_stack[m_rsp];

            --m_rsp;
            m_stack[m_rsp] = lhs <= m_stack[m_rsp];
            ++m_rip_p;
        }

        void op_test_gt() {
            const auto& lhs = m_stack[m_rsp];

            --m_rsp;
            m_stack[m_rsp] = lhs < m_stack[m_rsp];
            ++m_rip_p;
        }

        void op_test_gte() {
            const auto& lhs = m_stack[m_rsp];

            --m_rsp;
            m_stack[m_rsp] = lhs >= m_stack[m_rsp];
            ++m_rip_p;
        }

        void op_jump_else(int16_t jmp_offset, int16_t on_truthy_pop_n) noexcept {
            if (!m_stack[m_rsp]) {
                m_rip_p += jmp_offset;
            } else {
                m_rsp -= on_truthy_pop_n;
                ++m_rip_p;
            }
        }

        void op_jump_if(int16_t jmp_offset, int16_t on_falsy_pop_n) noexcept {
            if (m_stack[m_rsp]) {
                m_rip_p += jmp_offset;
            } else {
                m_rsp -= on_falsy_pop_n;
                ++m_rip_p;
            }
        }

        void op_jump(int16_t jmp_offset) noexcept {
            m_rip_p += jmp_offset;
        }

        void op_call(int16_t chunk_id, int16_t argc) {
            const int16_t new_callee_sbp = m_rsp - argc + 1;
            const int16_t old_caller_sbp = m_rsbp;
            const int16_t old_caller_id = m_rcid;
            const int16_t old_caller_ret_ip = m_rip_p - m_code_bp + 1;

            m_rsbp = new_callee_sbp;
            m_rcid = chunk_id;
            m_rip_p = m_code_bp + m_fn_table_bp[chunk_id];

            m_frames.emplace_back(call_frame_type {
                new_callee_sbp,
                old_caller_sbp,
                old_caller_id,
                old_caller_ret_ip
            });
            --m_frames_free;
        }

        void op_ret() {
            const auto [callee_sbp, caller_sbp, caller_id, caller_ret_ip] = m_frames.back();

            m_frames.pop_back();
            ++m_frames_free;

            m_stack[callee_sbp] = m_stack[m_rsp];

            m_rsbp = caller_sbp;
            m_rsp = callee_sbp;
            m_rcid = caller_id;
            m_rip_p = m_code_bp + caller_ret_ip;
        }

        void op_halt(int16_t exit_code) noexcept {
            m_status = static_cast<ExitStatus>(exit_code);
            ++m_rip_p;
        }

    public:
        explicit VM(Program& prgm, std::size_t stack_length_limit, std::size_t call_frame_limit)
        : m_stack {}, m_frames {}, m_consts_view {}, m_code_bp {}, m_fn_table_bp {}, m_rip_p {}, m_frames_free {call_frame_limit}, m_rsbp {}, m_rsp {}, m_rcid {-1}, m_status {ExitStatus::ok} {
            m_stack.reserve(stack_length_limit);
            m_stack.resize(stack_length_limit);

            m_consts_view = prgm.consts.data();
            m_code_bp = prgm.code.data();
            m_fn_table_bp = prgm.offsets.data();

            m_rsbp = 0;
            m_rsp = -1;
            m_rcid = prgm.entry_func_id;
            
            m_frames.emplace_back(call_frame_type {
                0,
                0,
                -1,
                -1
            });
            --m_frames_free;

            if (m_rcid >= 0) {
                m_rip_p = m_code_bp + m_fn_table_bp[m_rcid];
                m_status = ExitStatus::ok;
            } else {
                m_rip_p = nullptr;
                m_status = ExitStatus::setup_err;
            }
        }

        [[nodiscard]] auto get_value(Arg op_arg) noexcept -> Value* {
            const auto [arg_n, arg_tag] = op_arg;
            
            switch (arg_tag) {
                case Location::constant: return m_consts_view + arg_n;
                case Location::heap_obj: {
                    m_status = ExitStatus::heap_err;
                    return nullptr;
                }
                case Location::temp: return m_stack.data() + m_rsbp + arg_n;
                default: {
                    m_status = ExitStatus::stack_err;
                    return nullptr;
                }
            }
        }

        template <typename V> requires (std::is_same_v<std::remove_cvref_t<V>, Value>)
        void push_value(V&& value) {
            ++m_rsp;
            m_stack[m_rsp] = std::forward<V>(value); // 30 profile cost
        }

        void lazy_pop_values(int16_t n) noexcept {
            m_rsp -= n;

            if (m_rsp < 0) {
                m_status = ExitStatus::stack_err;
            }
        }

        [[nodiscard]] auto peek_local_value(int offset) noexcept -> Value& {
            return m_stack[m_rsbp + offset];
        }

        [[nodiscard]] auto operator()() -> ExitStatus {
            while (!m_frames.empty() && m_status == ExitStatus::ok) {
                const auto& [next_argv, next_opcode] = *m_rip_p;

                switch (next_opcode) {
                case Opcode::djs_nop:
                    ++m_rip_p;
                    break;
                case Opcode::djs_dup:
                    op_dup(next_argv[0]);
                    break;
                case Opcode::djs_put_const:
                    op_push_const(next_argv[0]);
                    break;
                case Opcode::djs_deref:
                    m_status = ExitStatus::opcode_err;
                    break;
                case Opcode::djs_pop:
                    op_pop(next_argv[0]);
                    break;
                case Opcode::djs_emplace:
                    op_emplace(next_argv[0]);
                    break;
                case Opcode::djs_put_obj_dud:
                    m_status = ExitStatus::opcode_err;
                    break;
                case Opcode::djs_get_prop:
                    m_status = ExitStatus::opcode_err;
                    break;
                case Opcode::djs_put_prop:
                    m_status = ExitStatus::opcode_err;
                    break;
                case Opcode::djs_del_prop:
                    m_status = ExitStatus::opcode_err;
                    break;
                case Opcode::djs_mod:
                    op_mod();
                    break;
                case Opcode::djs_mul:
                    op_mul();
                    break;
                case Opcode::djs_div:
                    op_div();
                    break;
                case Opcode::djs_add:
                    op_add();
                    break;
                case Opcode::djs_sub:
                    op_sub();
                    break;
                case Opcode::djs_test_falsy:
                    op_test_falsy();
                    break;
                case Opcode::djs_test_strict_eq:
                    op_test_strict_eq();
                    break;
                case Opcode::djs_test_strict_ne:
                    op_test_strict_ne();
                    break;
                case Opcode::djs_test_lt:
                    op_test_lt();
                    break;
                case Opcode::djs_test_lte:
                    op_test_lte();
                    break;
                case Opcode::djs_test_gt:
                    op_test_gt();
                    break;
                case Opcode::djs_test_gte:
                    op_test_gte();
                    break;
                case Opcode::djs_jump_else:
                    op_jump_else(next_argv[0], next_argv[1]);
                    break;
                case Opcode::djs_jump_if:
                    op_jump_if(next_argv[0], next_argv[1]);
                    break;
                case Opcode::djs_jump:
                    op_jump(next_argv[0]);
                    break;
                case djs_call:
                    op_call(next_argv[0], next_argv[1]);
                    break;
                case djs_ret:
                    op_ret();
                    break;
                case djs_halt:
                default:
                    op_halt(next_argv[0]);
                    break;
                }
            }

            return m_status;
        }
    };



    [[nodiscard]] inline auto op_nop(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_dup(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_put_const(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_deref(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_pop(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_emplace(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_put_this(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_put_obj_dud(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_get_prop(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_put_prop(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_del_prop(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_numify(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_strcat(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_mod(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_mul(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_div(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_add(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_sub(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_test_falsy(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_test_strict_eq(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_test_strict_ne(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_test_lt(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_test_lte(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_test_gt(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_test_gte(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_jump_else(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_jump_if(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_jump(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_call(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_object_call(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_ctor_call(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_ret(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto op_halt(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;
    [[nodiscard]] inline auto dispatch_op(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool;

    using tco_call_frame_type = CallFrame<DispatchPolicy::tco>;
    using tco_opcode_fn = bool(*)(ExternVMCtx&, int16_t, int16_t);
    constexpr tco_opcode_fn tco_opcodes[static_cast<std::size_t>(Opcode::last)] = {
        op_nop,
        op_dup, op_put_const, op_deref, op_pop, op_emplace,
        op_put_this, op_put_obj_dud, op_get_prop, op_put_prop, op_del_prop,
        op_numify, op_strcat,
        op_mod, op_mul, op_div, op_add, op_sub,
        op_test_falsy, op_test_strict_eq, op_test_strict_ne, op_test_lt, op_test_lte, op_test_gt, op_test_gte,
        op_jump_else, op_jump_if, op_jump,
        op_call, op_object_call, op_ctor_call, op_ret,
        op_halt
    };

    [[nodiscard]] inline auto op_nop(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_dup(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ctx.stack[ctx.rsp + 1] = ctx.stack[ctx.rsbp + a0];
        ++ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_put_const(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool  {
        ctx.stack[ctx.rsp + 1] = ctx.consts_view[a0];
        ++ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_deref(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        if (auto& ref_value_ref = ctx.stack[ctx.rsp]; ref_value_ref.get_tag() != ValueTag::val_ref) {
            return false;
        } else {
            ctx.stack[ctx.rsp] = ref_value_ref.deep_clone();
        }

        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_pop(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ctx.rsp -= a0;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_emplace(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        if (auto& dest_val_ref = ctx.stack[ctx.rsbp + a0]; dest_val_ref.get_tag() != ValueTag::val_ref) {
            // Case 1: the target is a primitive Value on the stack.
            dest_val_ref = ctx.stack[ctx.rsp];
        } else if (auto dest_val_ref_p = dest_val_ref.get_value_ref(); dest_val_ref_p) {
            // Case 2: the target is a valid Value reference on the stack.
            *dest_val_ref_p = ctx.stack[ctx.rsp];
        } else {
            return false;
        }

        --ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_put_this(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ctx.stack[ctx.rsp + 1] = Value {ctx.frames.back().m_this_p};
        ++ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_put_obj_dud(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        if (const auto obj_id = ctx.heap.get_next_id(); !ctx.heap.add_item(obj_id, Object {nullptr})) {
            return false;
        } else {
            ctx.stack[ctx.rsp + 1] = Value {ctx.heap.get_item(obj_id)};
        }

        ++ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_get_prop(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        if (auto& src_val_ref = ctx.stack[ctx.rsp - 1]; src_val_ref.get_tag() != ValueTag::object) {
            return false;
        } else if (auto src_obj_p = src_val_ref.to_object(); !src_obj_p) {
            return false;
        } else if (auto src_prop_value_p = src_obj_p->get_property_value(PropertyHandle<Value> {src_obj_p, ctx.stack[ctx.rsp], PropertyHandleTag::key, static_cast<uint8_t>(PropertyHandleFlag::writable)}); !src_prop_value_p) {
            return false;
        } else {
            ctx.stack[ctx.rsp - 1] = Value {src_prop_value_p};
            --ctx.rsp;
            ++ctx.rip_p;
        }

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_put_prop(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        if (auto obj_p = ctx.stack[ctx.rsbp + a0].to_object(); !obj_p) {
            return false;
        } else if (!obj_p->set_property_value(PropertyHandle<Value> { obj_p, ctx.stack[ctx.rsp - 1], PropertyHandleTag::key, static_cast<uint8_t>(PropertyHandleFlag::writable)}, ctx.stack[ctx.rsp])) {
            return false;
        }

        ctx.rsp -= 2;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_del_prop(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        return false; // TODO!
    }

    [[nodiscard]] inline auto op_numify(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        if (auto num_temp = ctx.stack[ctx.rsp].to_num_f64(); num_temp) {
            ctx.stack[ctx.rsp] = Value {*num_temp};
        } else {
            ctx.stack[ctx.rsp] = Value {JSNaNOpt {}};
        }

        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_strcat(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        DynamicString result {ctx.stack[ctx.rsp].to_string().value()};
        result.append_back(ctx.stack[ctx.rsp - 1].to_string().value());

        if (auto temp_str_p = ctx.heap.add_item(ctx.heap.get_next_id(), std::move(result)); !temp_str_p) {
            return false;
        } else {
            ctx.stack[ctx.rsp - 1] = Value {temp_str_p};
            --ctx.rsp;
            ++ctx.rip_p;
        }

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_mod(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        std::swap_ranges(reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp) + sizeof(Value), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp - 1));
        ctx.stack[ctx.rsp - 1] %= ctx.stack[ctx.rsp];
        --ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_mul(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ctx.stack[ctx.rsp - 1] *= ctx.stack[ctx.rsp]; // commutativity of the TIMES operator allows avoidance of std::swap
        --ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_div(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        std::swap_ranges(reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp) + sizeof(Value), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp - 1));
        ctx.stack[ctx.rsp - 1] /= ctx.stack[ctx.rsp];
        --ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_add(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ctx.stack[ctx.rsp - 1] += ctx.stack[ctx.rsp]; // commutativity of the PLUS operator allows avoidance of std::swap
        --ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_sub(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        std::swap_ranges(reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp) + sizeof(Value), reinterpret_cast<std::byte*>(ctx.stack.data() + ctx.rsp - 1));
        ctx.stack[ctx.rsp - 1] -= ctx.stack[ctx.rsp];
        --ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_test_falsy(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ctx.stack[ctx.rsp + 1] = ctx.stack[ctx.rsp].is_falsy();
        ++ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_test_strict_eq(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] == ctx.stack[ctx.rsp];
        --ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_test_strict_ne(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] != ctx.stack[ctx.rsp];
        --ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_test_lt(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] > ctx.stack[ctx.rsp]; // IF x < y THEN y > x
        --ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_test_lte(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] >= ctx.stack[ctx.rsp]; // IF x <= y THEN y >= x
        --ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_test_gt(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] < ctx.stack[ctx.rsp]; // IF x > y THEN y < x
        --ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_test_gte(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ctx.stack[ctx.rsp - 1] = ctx.stack[ctx.rsp - 1] <= ctx.stack[ctx.rsp]; // IF x >= y THEN y <= x
        --ctx.rsp;
        ++ctx.rip_p;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_jump_else(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        if (!ctx.stack[ctx.rsp]) {
            ctx.rip_p += a0;
        } else {
            ctx.rsp -= a1;
            ++ctx.rip_p;
        }

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    inline auto op_jump_if(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        if (ctx.stack[ctx.rsp]) {
            ctx.rip_p += a0;
        } else {
            ctx.rsp -= a1;
            ++ctx.rip_p;
        }

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_jump(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ctx.rip_p += a0;

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_call(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        auto caller_this_p = ctx.frames.back().m_this_p;
        const int16_t new_callee_sbp = ctx.rsp - a1 + 1;
        const int16_t old_caller_sbp = ctx.rsbp;
        const auto caller_ret_ip = ctx.rip_p + 1;

        ctx.rip_p = ctx.code_bp + ctx.fn_table_bp[a0];
        ctx.rsbp = new_callee_sbp;

        ctx.frames.emplace_back(tco_call_frame_type {
            caller_ret_ip,
            caller_this_p,
            new_callee_sbp,
            old_caller_sbp,
        });

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_object_call(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        auto& callable_value = ctx.stack[ctx.rsp];

        if (const auto val_tag = callable_value.get_tag(); val_tag == ValueTag::val_ref && callable_value.get_value_ref()->get_tag() == ValueTag::object) {
            ctx.has_err = !callable_value.get_value_ref()->to_object()->call(&ctx, a0);
        } else if (val_tag == ValueTag::object) {
            ctx.has_err = !callable_value.to_object()->call(&ctx, a0);
        } else {
            return false;
        }

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_ctor_call(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        auto& callable_value = ctx.stack[ctx.rsp];

        if (const auto val_tag = callable_value.get_tag(); val_tag == ValueTag::val_ref && callable_value.get_value_ref()->get_tag() == ValueTag::object) {
            ctx.has_err = !callable_value.get_value_ref()->to_object()->call_as_ctor(&ctx, a0);
        } else if (val_tag == ValueTag::object) {
            ctx.has_err = !callable_value.to_object()->call_as_ctor(&ctx, a0);
        } else {
            return false;
        }

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_ret(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        const auto& [caller_ret_ip, callee_sbp, caller_sbp] = ctx.frames.back();

        ctx.stack[callee_sbp] = ctx.stack[ctx.rsp];

        ctx.rsp = callee_sbp;
        ctx.rsbp = caller_sbp;
        ctx.rip_p = caller_ret_ip;

        ctx.frames.pop_back();

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto op_halt(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        ctx.has_err = a0 ^ 0; // 1. A non-zero flag is an errorneous exit.

        return dispatch_op(ctx, ctx.rip_p->args[0], ctx.rip_p->args[1]);
    }

    [[nodiscard]] inline auto dispatch_op(ExternVMCtx& ctx, int16_t a0, int16_t a1) -> bool {
        if (ctx.frames.empty() + ctx.has_err) {
            return ctx.has_err;
        }

        return tco_opcodes[ctx.rip_p->op](ctx, a0, a1);
    }

    template <>
    class VM<DispatchPolicy::tco> {
    public:
        static_assert(is_tco_enabled_v, "TCO is not enabled for this compiler toolchain (version).");

        /// NOTE: This SHOULD NOT be modified directly!
        ExternVMCtx m_ctx;

        explicit VM(Program& prgm, std::size_t stack_length_limit, std::size_t call_frame_limit)
        : m_ctx {prgm, stack_length_limit, call_frame_limit} {}

        [[nodiscard]] auto peek_final_result() const noexcept -> const Value& {
            return m_ctx.stack[0];
        }

        [[nodiscard]] inline auto operator()() -> bool {
            return dispatch_op(m_ctx, m_ctx.rip_p->args[0], m_ctx.rip_p->args[1]);
        }
    };
}
