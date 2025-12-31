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
        djs_push,
        djs_pop,
        djs_dup,
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
        // djs_test_strict_eq,
        // djs_test_strict_ne,
        // djs_test_lt,
        // djs_test_lte,
        // djs_test_gt,
        // djs_test_gte,
        // djs_jump_else,
        // djs_jump,
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
        Arg args[2];
        Opcode op;
    };

    using Chunk = std::vector<Instruction>;

    struct Program {
        std::vector<Object<Value>> heap_items;
        std::vector<Value> consts;
        std::vector<Chunk> chunks;
        int16_t entry_func_id;
    };

    void disassemble_program(const Program& prgm) {
        static constexpr std::array<std::string_view, static_cast<std::size_t>(Opcode::last)> opcode_names = {
            "djs_nop",
            "djs_push",
            "djs_pop",
            "djs_dup",
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
            // "djs_test_strict_eq",
            // "djs_test_strict_ne",
            // "djs_test_lt",
            // "djs_test_lte",
            // "djs_test_gt",
            // "djs_test_gte",
            // "djs_jump_else",
            // "djs_jump",
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

        const auto& [prgm_pre_heap, prgm_consts, prgm_chunks, prgm_entry_id] = prgm;

        std::println("\x1b[1;33mProgram Dump:\x1b[0m\n\nEntry Chunk ID: {}\n\n", prgm_entry_id);

        std::println("\x1b[1;33mConstants:\x1b[0m\n\n");

        for (int constant_id = 0; const auto& temp_constant : prgm_consts) {
            std::println(
                "\tconst:{} -> {}",
                constant_id,
                temp_constant.to_string().value()
            );

            ++constant_id;
        }

        std::println("\x1b[1;33mChunks:\x1b[0m\n\n");

        for (int chunk_id = 0; const auto& temp_chunk : prgm_chunks) {
            std::println("Chunk {}:\n\n", chunk_id);

            for (const auto& [instr_argv, instr_op] : temp_chunk) {
                std::println(
                    "\t{} {}:{} {}:{}",
                    opcode_names[static_cast<std::size_t>(instr_op)],
                    location_names[instr_argv[0].tag],
                    instr_argv[0].n,
                    location_names[instr_argv[1].tag],
                    instr_argv[1].n
                );
            }

            ++chunk_id;
        }
    }
}
