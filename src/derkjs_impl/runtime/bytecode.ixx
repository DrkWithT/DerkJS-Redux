module;

#include <cstdint>
#include <vector>
#include <flat_map>

export module runtime.bytecode;

import runtime.value;

export namespace DerkJS {
    using Chunk = std::vector<uint8_t>;

    struct Program {
        std::flat_map<int, Object<Value>> heap_items;
        std::vector<Value> consts;
        std::vector<Chunk> chunks;
        int entry_func_id;
    };

    enum class ExitStatus : uint8_t {
        ok,
        setup_err,
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
    };

    enum Location : uint8_t {
        immediate,
        constant,
        temp,
    };

    struct Arg {
        uint16_t n;
        Location tag;
    };

    struct Instruction {
        Arg args[3];
        Opcode op;
    };
}
