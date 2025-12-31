module;

#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>
#include <iostream>

export module runtime.vm;

import runtime.value;
import runtime.bytecode;

export namespace DerkJS {
    struct CallFrame {
        int16_t m_callee_sbp;
        int16_t m_caller_sbp;
        int16_t m_caller_id;
        int16_t m_caller_ret_ip;

        constexpr CallFrame() noexcept
        : m_callee_sbp {0}, m_caller_sbp {0}, m_caller_id {-1}, m_caller_ret_ip {-1} {}

        constexpr CallFrame(int16_t callee_sbp, int16_t caller_sbp, int16_t caller_id, int16_t caller_ret_ip) noexcept
        : m_callee_sbp {callee_sbp}, m_caller_sbp {caller_sbp}, m_caller_id {caller_id}, m_caller_ret_ip {caller_ret_ip} {}

        explicit operator bool(this auto&& self) noexcept {
            return self.m_caller_ip < 0 || self.m_caller_ret_ip < 0;
        }
    };

    class VM {
    private:
        // GCHeap<Object<Value>> m_heap;
        std::vector<Value> m_stack;
        std::vector<CallFrame> m_frames;

        Value* m_consts_view;
        Chunk* m_chunk_view;
        const Chunk* m_chunk_p;
        std::size_t m_frames_free;

        /// NOTE: holds stack base pointer for call locals
        int16_t m_rsbp;

        /// NOTE: holds stack top pointer
        int16_t m_rsp;

        /// NOTE: holds current instruction pointer
        int16_t m_rip;

        /// NOTE: holds current bytecode chunk ID (caller's code)
        int16_t m_rcid;

        /// NOTE: indicates an error condition during execution, returning from the VM invocation
        ExitStatus m_status;

        [[nodiscard]] auto is_done() const noexcept -> bool {
            return m_frames.empty() || m_status != ExitStatus::ok;
        }

        void op_push(Arg src) {
            if (auto value_p = get_value(src); !value_p) {
                m_status = ExitStatus::stack_err;
                return;
            } else {
                push_value(*value_p);
                ++m_rip;
            }
        }

        void op_pop(Arg pop_n) noexcept {
            lazy_pop_values(pop_n.n);
            ++m_rip;
        }

        void op_dup([[maybe_unused]] Arg src) {
            m_status = ExitStatus::opcode_err;
        }

        void op_mod() {
            auto rhs_v = pop_off_value();
            auto lhs_v = pop_off_value();

            if (m_status != ExitStatus::ok) {
                return;
            }

            push_value(lhs_v % rhs_v);

            ++m_rip;
        }

        void op_mul() {
            auto lhs_v = pop_off_value();
            auto rhs_v = pop_off_value();

            if (m_status != ExitStatus::ok) {
                return;
            }

            push_value(lhs_v * rhs_v);

            ++m_rip;
        }

        void op_div() {
            auto lhs_v = pop_off_value();
            auto rhs_v = pop_off_value();

            if (m_status != ExitStatus::ok) {
                return;
            }

            push_value(lhs_v / rhs_v);

            ++m_rip;
        }

        void op_add() {
            auto lhs_v = pop_off_value();
            auto rhs_v = pop_off_value();

            if (m_status != ExitStatus::ok) {
                return;
            }

            push_value(lhs_v + rhs_v);

            ++m_rip;
        }

        void op_sub() {
            auto lhs_v = pop_off_value();
            auto rhs_v = pop_off_value();

            if (m_status != ExitStatus::ok) {
                return;
            }

            push_value(lhs_v - rhs_v);

            ++m_rip;
        }

        void op_test(Arg src) {
            if (auto src_value_p = get_value(src); !src_value_p) {
                return;
            } else {
                push_value(Value {src_value_p->is_truthy()});
                ++m_rip;
            }
        }

        void op_call(Arg chunk_id, Arg argc) {
            const int16_t new_callee_sbp = m_rsp - argc.n + 1;
            const int16_t old_caller_sbp = m_rsbp;
            const int16_t old_caller_id = m_rcid;
            const int16_t old_caller_ret_ip = m_rip + 1;

            m_rsbp = new_callee_sbp;
            m_rip = 0;
            m_rcid = chunk_id.n;
            m_chunk_p = m_chunk_view + chunk_id.n;

            m_frames.emplace_back(CallFrame {
                new_callee_sbp,
                old_caller_sbp,
                old_caller_id,
                old_caller_ret_ip
            });
            --m_frames_free;
        }

        void op_native_call([[maybe_unused]] Arg native_id) {
            m_status = ExitStatus::opcode_err;
        }

        void op_ret() {
            const auto [callee_sbp, caller_sbp, caller_id, caller_ret_ip] = m_frames.back();

            m_frames.pop_back();
            ++m_frames_free;

            Value result_v = pop_off_value();

            m_rsp = callee_sbp;
            m_stack[callee_sbp] = std::move(result_v);

            m_rcid = caller_id;
            m_rip = caller_ret_ip;
            m_rsbp = caller_sbp;
            m_chunk_p = m_chunk_view + caller_id;
        }

        void op_halt(Arg exit_code) {
            m_status = static_cast<ExitStatus>(exit_code.n);
            ++m_rip;
        }

    public:
        VM(Program& prgm, std::size_t stack_length_limit, std::size_t call_frame_limit)
        : m_stack {}, m_frames {}, m_consts_view {}, m_chunk_view {}, m_chunk_p {}, m_frames_free {call_frame_limit}, m_rsbp {}, m_rsp {}, m_rip {0}, m_rcid {-1}, m_status {ExitStatus::ok} {
            m_stack.reserve(stack_length_limit);
            m_stack.resize(stack_length_limit);

            m_consts_view = prgm.consts.data();
            m_chunk_view = prgm.chunks.data();
            m_chunk_p = m_chunk_view + prgm.entry_func_id;

            m_rsbp = 0;
            m_rsp = -1;
            m_rip = 0;
            m_rcid = prgm.entry_func_id;

            m_frames.emplace_back(CallFrame {
                0,
                0,
                -1,
                -1
            });
            --m_frames_free;

            m_status = (m_rcid >= 0) ? ExitStatus::ok : ExitStatus::setup_err;
        }

        [[nodiscard]] auto get_value(Arg op_arg) noexcept -> Value* {
            const auto& [arg_n, arg_tag] = op_arg;
            
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
            m_stack[m_rsp] = std::forward<Value>(value);
        }

        void lazy_pop_values(int16_t n) noexcept {
            m_rsp -= n;

            if (m_rsp < 0) {
                m_status = ExitStatus::stack_err;
            }
        }

        [[nodiscard]] auto pop_off_value() -> Value {
            if (m_rsp < 0) {
                m_status = ExitStatus::stack_err;
                return {};
            }

            Value temp = std::move(m_stack[m_rsp]);

            --m_rsp;

            return temp;
        }

        [[nodiscard]] auto operator()() -> ExitStatus {
            while (!is_done()) {
                const auto& [next_argv, next_opcode] = m_chunk_p->data()[m_rip];

                switch (next_opcode) {
                case Opcode::djs_nop:
                    ++m_rip;
                    break;
                case Opcode::djs_push:
                    op_push(next_argv[0]);
                    break;
                case Opcode::djs_pop:
                    op_pop(next_argv[0]);
                    break;
                case Opcode::djs_dup:
                    op_dup(next_argv[0]);
                    break;
                // case Opcode::djs_create_obj:
                // case Opcode::djs_get_prop:
                // case Opcode::djs_put_prop:
                // case Opcode::djs_del_prop:
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
                case Opcode::djs_test:
                    op_test(next_argv[0]);
                    break;
                // case Opcode::djs_test_strict_eq:
                // case Opcode::djs_test_strict_ne:
                // case Opcode::djs_test_lt:
                // case Opcode::djs_test_lte:
                // case Opcode::djs_test_gt:
                // case Opcode::djs_test_gte:
                // case Opcode::djs_jump_else:
                // case Opcode::djs_jump:
                case djs_call:
                    op_call(next_argv[0], next_argv[1]);
                    break;
                case djs_native_call:
                    op_native_call(next_argv[0]);
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

            std::cout << "\n\x1b[1;33mDerkJS [Result Value]:\x1b[0m \x1b[1;32m" << m_stack.at(m_rsbp).to_string().value() << "\x1b[0m\n";

            return m_status;
        }
    };
}
