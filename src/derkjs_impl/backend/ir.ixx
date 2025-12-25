module;

#include <cstdint>
#include <vector>
#include <variant>

export module backend.ir;

export namespace DerkJS {
    enum class IrOp : uint8_t {
        ir_noop, // no action
        ir_push, // pushes multiple values given IR-arg
        ir_pop, // removes top-most / top-N-most items from stack's top
        ir_emplace_vargs, // constructs an object in place on the stack using copies of multiple arguments from the top
        ir_push_closure, // pushes a context for any `this` reference, especially in closures
        ir_vm_call, // by callee ID: pushes callee's call frame, sets bytecode location to the corresponding chunk, and CAN take an optional closure argument for `this`
        ir_ret, // pops callee's call frame, yields the top stack value to the caller, and may run the (later) GC to cleanup dead closures
        ir_mod, // this, like other arithmetic operations, takes the top two stack `Values` and does the math - except when the flag is `1` for emplaced operations where the top-most stack value is operated in-place to the neighboring value below
        ir_mul,
        ir_div,
        ir_add,
        ir_sub,
        ir_negate,
        ir_jmp_eq,
        ir_jump_ne,
        ir_jmp_strict_eq,
        ir_jmp_strict_ne,
        ir_jmp_lt,
        ir_jmp_le,
        ir_jmp_gt,
        ir_jmp_gte,
    };

    enum class ArgTag : uint8_t {
        aa_immediate,
        aa_const,
        aa_local,
        aa_func,
        aa_native_func,
    };

    struct IrArg {
        ArgTag tag;
        uint16_t n;
    };

    /// NOTE: fixed arity IR "instruction" contained in BB's
    struct IrStep {
        IrOp op;
        IrArg args[3];
    };

    /// BB terminator with no successors
    struct IrEnd {};

    /// BB terminator for control-flow branches
    struct IrFork {
        int lhs_id;
        int rhs_id;
    };

    using IRTerminator = std::variant<IrEnd, IrFork>;

    /// NOTE: models a CFG basic block (no jumps in the step sequence)
    class IrBB {
    public:
        static constexpr auto dud_id = -1;

    private:
        std::vector<IrStep> m_steps;
        IRTerminator m_terminator;

    public:
        IrBB()
        : m_steps {}, m_terminator {} {}

        [[nodiscard]] auto get_steps_mut() noexcept -> std::vector<IrStep>& {
            return m_steps;
        }

        [[nodiscard]] auto get_terminator() const noexcept -> const IRTerminator& {
            return m_terminator;
        }

        [[nodiscard]] auto has_successors() const noexcept -> bool {
            return std::holds_alternative<IrFork>(m_terminator);
        }

        void link_lhs_id(int id) {
            if (!has_successors()) {
                m_terminator = IrFork {
                    .lhs_id = dud_id,
                    .rhs_id = dud_id,
                };
            }

            std::get<IrFork>(m_terminator).lhs_id = id;
        }

        void link_rhs_id(int id) {
            if (!has_successors()) {
                m_terminator = IrFork {
                    .lhs_id = dud_id,
                    .rhs_id = dud_id,
                };
            }

            std::get<IrFork>(m_terminator).rhs_id = id;
        }
    };

    class BBHandle {
    private:
        IrBB* m_bb_p;
        int m_bb_id;

    public:
        constexpr BBHandle() noexcept
        : m_bb_p {}, m_bb_id {-1} {}

        constexpr BBHandle(IrBB* bb_p, int bb_id) noexcept
        : m_bb_p {bb_p}, m_bb_id {bb_id} {}

        [[nodiscard]] constexpr auto get_bb_ptr() noexcept -> IrBB* {
            return m_bb_p;
        }

        [[nodiscard]] constexpr auto get_bb_id() const noexcept -> int {
            return m_bb_id;
        }

        explicit constexpr operator bool(this auto&& self) noexcept {
            return !self.m_bb_p || self.m_bb_id < 0;
        }
    };

    /// NOTE: models a CFG of basic blocks as nodes, connected by control-flow terminators that can be T/F "forks"
    class IrCFG {
    private:
        std::vector<IrBB> m_blocks;
        int m_block_count;

    public:
        IrCFG()
        : m_blocks {}, m_block_count {0} {}

        [[nodiscard]] auto get_bb_mut(int id) noexcept -> BBHandle {
            return BBHandle {
                &m_blocks[id],
                id
            };
        }

        [[nodiscard]] auto add_bb() -> BBHandle {
            const int next_bb_id = m_blocks.size();

            m_blocks.emplace_back(IrBB {});

            return BBHandle {
                &m_blocks[next_bb_id],
                next_bb_id
            };
        }
    };

    struct SourcedCFG {
        IrCFG cfg;
        int source_id;
    };

    /// NOTE: represents a JS TU as a collection of top-level CFGs
    using IrBundle = std::vector<SourcedCFG>;
}
