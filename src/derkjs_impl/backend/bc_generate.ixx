module;

#include <optional>
// #include <vector>
#include <flat_map>


export module backend.bc_generate;

import frontend.ast;
import runtime.value;
import runtime.bytecode;

export namespace DerkJS {
    class BytecodeGenPass {
    private:
        struct Backpatch {
            uint16_t dest_ip;
            uint16_t src_ip;
            bool is_reversed;
        };

        std::flat_map<int, Object<Value>> heap_items;
        std::vector<Value> consts;
        std::vector<Chunk> chunks;
        int m_temp_entry_id;

        int m_next_heap_id;
        int m_next_const_id;
        int m_next_temp_id;
        int m_next_chunk_id;
        // bool is_op_emplaced;
        // bool is_temp_poppable;

    public:
        BytecodeGenPass() = default;

        [[nodiscard]] auto operator()([[maybe_unused]] const ASTUnit& tu, [[maybe_unused]] const std::flat_map<int, std::string>& source_map) -> std::optional<Program> {
            return {};
        }
    };
}
