module;

#include <cstdint>
#include <array>
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
        djs_dup, // Duplicates the top stack value.
        djs_dup_local, // Duplicates a local relative to the callee's RBP.
        djs_ref_local, // Pushes a reference to a local, offset from the callee's RBP.
        djs_store_upval, // Stores a CLONED variable to the "capture" object that may be used by a callee; Stack: <temp-value> <string-key> -> <temp-value> ---  closure[key] = <temp-value>)
        djs_ref_upval, // Gets an upvalue reference from the "capture" object that was created by the caller earlier. Stack: <string-key> -> <up-value-ref>
        djs_put_const, // Pushes a global constant.
        djs_deref, // Args: Replaces the top value with its fully-dereferenced Value.
        djs_pop, // Lazy pops by N given <pop-n>.
        djs_emplace, // Pops the temporary and stores it into the under value (local variable reference / property reference) which gets popped afterwards.
        djs_put_this, // Pushes a reference to the current `this` object.
        djs_discard, // For `void`, replaces the evaluated expression value with an `undefined`.
        djs_typename, // For `typeof`, replaces the expression value with a newly created string to its typename.
        djs_put_obj_dud, // Pushes a newly created, empty JS object.
        djs_make_arr, // Args: <item-count>; Pushes a newly created, empty JS array from the top N stack items. The Array prototype is automatically bound to the new array.
        djs_put_proto_key, // Replaces top stack obj-ref with proto-ref.
        djs_get_prop, // djs_get_prop gets a property value's ref based on an object's ref below a pooled string ref on the stack... the result is placed where the targeted ref was. --> Stack placement: <OBJ-REF-LOCAL> <PROP-KEY-HANDLE> --> <PROP-VALUE-REF>
        djs_put_prop, // djs_put_prop --> Stack placement: <OBJ-REF> <PROP-KEY-HANDLE-VALUE> <NEW-VALUE> --> <OBJ-REF>
        djs_del_prop, // TODO!
        djs_numify, // converts the VM stack's top value to a number
        djs_strcat, // concatenates 2 string copies since the ref-wrapping `Value` is decoupled from VM state --> Stack placement: <STRING-1> <STRING-2> --> <NEW-STRING>
        djs_pre_inc, // puts the VM stack's top value copy, increments, then clones it
        djs_pre_dec, // puts the VM stack's top value copy, decrements, then clones it
        djs_mod,
        djs_mul,
        djs_div,
        djs_add,
        djs_sub,
        djs_test_falsy, // Converts the VM stack's top value to a boolean via truthiness.
        djs_test_strict_eq, // Stack: <RHS> <LHS> -> <RESULT-BOOL>
        djs_test_strict_ne,
        djs_test_lt,
        djs_test_lte,
        djs_test_gt,
        djs_test_gte,
        djs_jump_else, // Pops the LHS if it's truthy before incrementing RIP. Otherwise, the relative offset is applied to RIP.
        djs_jump_if, // Pops the LHS if it's falsy before incrementing RIP. Otherwise, the relative offset is applied to RIP.
        djs_jump,
        djs_object_call, // Args: <arg-count> <pass-this-flag>: Assumes the top stack value references a `ObjectBase<Value>` to invoke on <arg-count> temporaries below. NativeFunction objects don't need to affect `RSBP` and `RSP` for restoring caller stack state. The call() virtual method per function object can now take 'this' on `pass-this-flag == 1`: the caller object is 'this', laying on top of all other arguments as the consuming temporary. Stack: `<obj-ref> <obj-ref> <key-value> -> <result>`
        djs_ctor_call, // Args: <arg-count>; creates a this object to initialize and return via `var foo = new Foo()` where the function has `return this;`. Invokes the object's `call_as_ctor()` virtual method. If `opt-chunk-id >= 0`: invokes the bytecode function as a constructor. ONLY WORKS WITH FUNCTION OBJECTS!!
        djs_ret, // Arg: <is-implicit> Yields the callee's result to the caller. If `is-implicit = 1`, return `undefined` or `this`, but yield the top-stack value otherwise.
        djs_halt,
        last,
    };

    enum class CallFlags : uint8_t {
        is_ctor = 0b00000001
    };

    enum Location : uint8_t {
        code_chunk,
        immediate,
        constant,
        heap_obj, // -2: this, -3: prototype
        local,
        key_str,
        end,
    };

    struct Arg {
        int16_t n;
        Location tag;
        bool is_str_literal;
        bool from_closure;
    };

    struct Instruction {
        int16_t args[2];
        Opcode op;
    };

    struct Program {
        /// Stores initial heap entries to load.
        PolyPool<ObjectBase<Value>> heap_items;

        /// Stores JS intrinsic prototypes.
        std::array<ObjectBase<Value>*, static_cast<std::size_t>(BasePrototypeID::last)> base_protos;
        
        std::vector<Value> consts;
        std::vector<Instruction> code;
        std::vector<int> offsets;
        int16_t entry_func_id;
    };

    void disassemble_program(const Program& prgm) {
        static constexpr std::array<std::string_view, static_cast<std::size_t>(Opcode::last)> opcode_names = {
            "djs_nop",
            "djs_dup",
            "djs_dup_local",
            "djs_ref_local",
            "djs_store_upval",
            "djs_ref_upval",
            "djs_put_const",
            "djs_deref",
            "djs_pop",
            "djs_emplace",
            "djs_put_this",
            "djs_discard",
            "djs_typename",
            "djs_put_obj_dud",
            "djs_make_arr",
            "djs_put_proto_key",
            "djs_get_prop", // Args: <should-default>: gets a property value based on RSP: <OBJ-REF>, RSP - 1: <POOLED-STR-REF>; IF should-default == 1, default any invalid key to `undefined`.
            "djs_put_prop", // SEE: djs_get_prop for stack args passing...
            "djs_del_prop", // SEE: djs_get_prop for stack args passing...
            "djs_numify",
            "djs_strcat",
            "djs_pre_inc",
            "djs_pre_dec",
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
            "djs_object_call",
            "djs_ctor_call",
            "djs_ret",
            "djs_halt",
        };

        const auto& [prgm_heap_items, prgm_prototype_bases, prgm_consts, prgm_code, prgm_code_offsets, prgm_entry_id] = prgm;

        std::println("\x1b[1;33mProgram Dump:\x1b[0m\n\nEntry Chunk ID: {}\n", prgm_entry_id);

        std::println("\x1b[1;33mFunction Offsets:\x1b[0m\n");

        for (auto fn_bc_index = 0; const auto& fn_bc_pos : prgm_code_offsets) {
            std::println("Chunk {} offset -> {}", fn_bc_index, fn_bc_pos);
            ++fn_bc_index;
        }

        std::println("\n\x1b[1;33mInitial Heap:\x1b[0m\n");

        for (int heap_id = 0; const auto& heap_cell : prgm_heap_items.view_items()) {
            if (auto heap_obj_p = heap_cell.get(); heap_obj_p) {
                std::println("heap-cell:{} -> {}", heap_id, heap_obj_p->as_string());
                ++heap_id;
            } else {
                break;
            }
        }

        std::println("\n\x1b[1;33mConstants:\x1b[0m\n");

        for (int constant_id = 0; const auto& temp_constant : prgm_consts) {
            std::println(
                "const:{} -> {}",
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
