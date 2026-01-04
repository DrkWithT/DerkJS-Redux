module;

#include <cstdint>
#include <vector>
#include <print>

export module runtime.bytecode;

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
        djs_put_obj_ref,
        djs_pop,
        // djs_create_obj,
        // djs_get_prop,
        // djs_put_prop,
        // djs_del_prop,
        djs_mod,
        djs_mul,
        djs_div,
        djs_add,
        djs_sub,
        djs_test,
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
        std::vector<Object<Value>> heap_items;
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
            "djs_put_obj_ref",
            "djs_pop",
            // "djs_create_obj",
            // "djs_get_prop",
            // "djs_put_prop",
            // "djs_del_prop",
            "djs_mod",
            "djs_mul",
            "djs_div",
            "djs_add",
            "djs_sub",
            "djs_test",
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

        static constexpr std::array<std::string_view, static_cast<std::size_t>(Location::end)> location_names = {
            "chunk",
            "imm",
            "const",
            "heap",
            "temp"
        };

        const auto& [prgm_pre_heap, prgm_consts, prgm_code, prgm_code_offsets, prgm_entry_id] = prgm;

        std::println("\x1b[1;33mProgram Dump:\x1b[0m\n\nEntry Chunk ID: {}\n", prgm_entry_id);

        std::println("\x1b[1;33mConstants:\x1b[0m\n");

        for (int constant_id = 0; const auto& temp_constant : prgm_consts) {
            std::println(
                "\tconst:{} -> {}",
                constant_id,
                temp_constant.to_string().value()
            );

            ++constant_id;
        }

        std::println("\x1b[1;33mCode:\x1b[0m\n");

        for (int fn_offset_index = 0, abs_code_pos = 0; const auto& [instr_argv, instr_op] : prgm_code) {
            if (abs_code_pos == prgm_code_offsets[fn_offset_index]) {
                std::println("Chunk {}:\n", fn_offset_index);
                ++fn_offset_index;
            }

            std::println(
                "\t{} {} {}",
                opcode_names[static_cast<std::size_t>(instr_op)],
                instr_argv[0],
                instr_argv[1]
            );

            ++abs_code_pos;
        }
    }
}
