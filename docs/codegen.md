## Bytecode Generation

### Name & Symbol Mappings
 - Mapping of type `Arg`:
    - The "id" into the entity space
    - Entity space (scoped enum tag):
        - code_chunk
        - immediate
        - constant
        - heap_obj
        - temp
 - All constants are _global_ & stored in a separate mapping (symbol-string to mapping) that takes 1st priority:
    - A static reference to any heap-string object is a constant. Corresponds to a `Value` storing an `ObjectBase<Value>*` within the tracked constant values, possibly referencing a global-scoped native object.
    - A primitive symbol. Corresponds to a primitive boxing `Value`.
 - All other entities are stored in another scope.
    - A local variable's identifier is usually mapped to a temporary stack location (which can be any Value).

### Pre-loaded heap and constant items
 - This is primarily done for native objects e.g `console`:
    1. A blank object is injected into the Driver class.
    2. A list of "entities" each with a native value / regular value is inserted 1 by 1 into the object. The property name is interned as a string and becomes a constant referencing the same string... The property value is then pre-loaded into the heap: (`PolyPool<ObjectBase<Value>>`) & a special entity is also stored in the appropriate list.
    3. The bytecode generator initially loads the pre-populated "heap" & entries before actual generation.

### Data Handling
 - The codegen logic only tracks the count of locals... The rest of the stack handling is deferred to opcodes at runtime.
   - No more `m_next_id` to track where temporary locals end up because runtime cannot be fully anticipated at compile-time.
      - `put_local_ref <local-id>` is necessary before updating locals.
      - `djs_emplace` now requires a value "reference" below the incoming temporary value.
 - Property accesses must be treated as "lvalues" for property assignments & calls.
   - **NOTE:** Function declarations have a different stack temp counter vs. the implicit top-level function.
 - The `this` value should be an object filled per function call with a reference to the parent `this`:
   - Global Scope's `this` should have all global vars set as properties.
   - Local Scopes have `this` referring to an empty object which can be filled with properties and returned, making a constructor.

### Control Flow
 - Calls always place their result on the 1st slot where the callee's locals are (the caller's `RSP - ARG_COUNT (+ 1)?`).
 - Certain opcodes such as `jump_if` & `jump_else` conditionally pop off the 1st operand on the VM for short circuit evaluation.
    - OR: the LHS is the result iff TRUTHY, but the RHS is taken iff the LHS is falsy
    - AND: the LHS is the result iff FALSY, but the RHS is taken otherwise

### Calls
 - Basic pre-call layout: `<thisArg (undefined)>, <callee>, <args...>`
    - `thisArg` is patched to a new object for constructors or `capture_p` for regular functions.
 - Returns per function are placed at `CALLEE_RBSP - 1`.

### Error
 - These are exceptions which unwind the call stack frame by frame. Between each unwind, the callee is linearly searched for a `djs_catch` instruction.
 - Otherwise, if no `djs_catch` instruction is hit, the script exits in failure.
 - `throw new Error(msg);` will place an error in a special `ExternVMCtx` field. `catch` blocks will clear this field after they run.
   - TODO: Fix `polyfill.js:211` once Error ctor is done.

### Pass 1: emit pass
 - Purpose: Generates bytecode using the context from the earlier pass(es).
