module;

#include <cstdint>
#include <vector>
#include <print>

export module runtime.bytecode;

import runtime.objects;
import runtime.value;

export namespace DerkJS {
    enum class ExitStatus : uint8_t {
        ok,
        setup_err,
        opcode_err,
        stack_err,
        heap_err,
        func_err
    };

    enum Opcode : uint8_t {
        djs_nop,
        djs_dup,
        djs_put_const,
        djs_put_val_ref, // a0: the id into the value space (VM stack / heap), a1: space-id: 0 -> stack, 1 -> consts, 2 -> heap
        djs_put_obj_ref,
        djs_deref, // takes a Value reference from RSP and replaces its stack slot with the deep-dereferenced Value
        djs_pop,
        djs_emplace,
        djs_put_obj_dud,
        djs_get_prop, // djs_get_prop gets a property value's ref based on an object's ref below a pooled string ref on the stack... the result is placed where the targeted ref was. --> Stack placement: <OBJ-REF-LOCAL> <PROP-KEY-HANDLE> --> <PROP-VALUE-REF>
        /// TODO: simplify this opcode to take arguments in-place on the stack like `djs_get_prop`!
        djs_put_prop, // djs_put_prop <obj-slot-id> <pop-before-place-n> --> Stack placement: <OBJ-REF-LOCAL> ... <PROP-KEY-HANDLE-VALUE> <NEW-VALUE> -- (lazy pop N) --> <OBJ-REF-LOCAL>
        djs_del_prop, // TODO!
        djs_mod,
        djs_mul,
        djs_div,
        djs_add,
        djs_sub,
        djs_test_falsy,
        djs_test_strict_eq,
        djs_test_strict_ne,
        djs_test_lt,
        djs_test_lte,
        djs_test_gt,
        djs_test_gte,
        djs_jump_else,
        djs_jump_if,
        djs_jump,
        djs_call,
        djs_native_call,
        djs_ret,
        djs_halt,
        last,
    };

    enum Location : uint8_t {
        code_chunk,
        immediate,
        constant,
        heap_obj,
        pooled_str,
        temp,
        end,
    };

    struct Arg {
        int16_t n;
        Location tag;
    };

    struct Instruction {
        int16_t args[2];
        Opcode op;
    };

    struct Program {
        /// Stores initial heap entries to load.
        PolyPool<ObjectBase<Value>> heap_items;

        std::vector<Value> consts;
        std::vector<Instruction> code;
        std::vector<int> offsets;
        int16_t entry_func_id;
    };

    void disassemble_program(const Program& prgm) {
        static constexpr std::array<std::string_view, static_cast<std::size_t>(Opcode::last)> opcode_names = {
            "djs_nop",
            "djs_dup",
            "djs_put_const",
            "djs_put_val_ref",
            "djs_put_obj_ref",
            "djs_deref",
            "djs_pop",
            "djs_emplace",
            "djs_put_obj_dud",
            "djs_get_prop", // gets a property value based on RSP: <OBJ-REF>, RSP - 1: <POOLED-STR-REF> 
            "djs_put_prop", // SEE: djs_get_prop for stack args passing...
            "djs_del_prop", // SEE: djs_get_prop for stack args passing...
            "djs_mod",
            "djs_mul",
            "djs_div",
            "djs_add",
            "djs_sub",
            "djs_test_falsy",
            "djs_test_strict_eq",
            "djs_test_strict_ne",
            "djs_test_lt",
            "djs_test_lte",
            "djs_test_gt",
            "djs_test_gte",
            "djs_jump_else",
            "djs_jump_if",
            "djs_jump",
            "djs_call",
            "djs_native_call",
            "djs_ret",
            "djs_halt",
        };

        const auto& [prgm_heap_items, prgm_consts, prgm_code, prgm_code_offsets, prgm_entry_id] = prgm;

        std::println("\x1b[1;33mProgram Dump:\x1b[0m\n\nEntry Chunk ID: {}\n", prgm_entry_id);

        std::println("\x1b[1;33mFunction Offsets:\x1b[0m\n");

        for (auto fn_bc_index = 0; const auto& fn_bc_pos : prgm_code_offsets) {
            std::println("Chunk {} offset -> {}", fn_bc_index, fn_bc_pos);
            ++fn_bc_index;
        }

        std::println("\n\x1b[1;33mConstants:\x1b[0m\n");

        for (int constant_id = 0; const auto& temp_constant : prgm_consts) {
            std::println(
                "\tconst:{} -> {}",
                constant_id,
                temp_constant.to_string().value()
            );

            ++constant_id;
        }

        std::println("\n\x1b[1;33mCode:\x1b[0m\n");

        for (int fn_offset_index = 0, abs_code_pos = 0; const auto& [instr_argv, instr_op] : prgm_code) {
            if (const auto fn_begin = prgm_code_offsets.at(fn_offset_index); abs_code_pos == fn_begin) {
                std::println("\n--- BEGIN CHUNK {} at {} ---\n", fn_offset_index, fn_begin);
                ++fn_offset_index;
            }

            std::println(
                "{}: {} {} {}",
                abs_code_pos,
                opcode_names.at(static_cast<std::size_t>(instr_op)),
                instr_argv[0],
                instr_argv[1]
            );

            ++abs_code_pos;
        }
    }
}
