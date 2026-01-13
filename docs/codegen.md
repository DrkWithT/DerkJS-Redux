## Bytecode Generation

### Name & Symbol Mappings
 - Mapping:
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

### Other Notes:
 - Property accesses must be treated as "lvalues" for property assignments & calls.
 - Certain opcodes such as `jump_if` & `jump_else` conditionally pop off the 1st operand on the VM for short circuit evaluation.
    - OR: the LHS is the result iff TRUTHY, but the RHS is taken iff the LHS is falsy
    - AND: the LHS is the result iff FALSY, but the RHS is taken otherwise
 - Calls always place their result on the 1st slot where the callee's locals are (the caller's `RSP - ARG_COUNT (+ 1)?`).
 - Normal function declarations must have their precise offsets within the bytecode recorded in a "function index table". Their names will be mapped to a function index from `[0, N)` where `N` is the count of top-level function decls which are 1st processed. Each function index is-premapped to an _absolute_ position within the entire bytecode buffer.
 - 
