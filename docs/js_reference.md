### JS Reference Design

#### General Semantics:
 - For assignment: `Value` should be able to handle local variable references, using a `Value*` with metadata.
    - **NOTE:** Every value assignment only succeeds when `[[Writable]]` is ON.
 - For property accesses: See below notes.
 - For `delete` operator: TODO!

#### PropEntry:
 - Optional function pointer to a mutation handler... This eases `Array.length`.
 - Struct fields: `Key, Value, Handler?` (flags come from the value)

#### AttrMask (value, object, or property metadata):
 - ~~All objects and simple Values should have a unified metadata system of 1 - 8 flags:~~
    - F0: writable
    - F1: configurable
    - F2: enumerable
    - F3: accessor (for handling accessor mutation)
    - **NOTE:** Default flags are `F0, F1`. Only Arrays and Objects have `F2` on.

#### PropertyDescriptor:
 - `PropertyDescriptor` should be redone:
    - Contains the key `Value`
    - Contains the parent object pointer (the "Base")
    - Contains the property value's pointer
    - Flags such as `[[Writable]]` are viewed from the `Value`.
    - `get_value`, `set_value`, and `ref_value` methods:
        - **NOTE:** On usage of `update_on_accessor_mut(const Value& key, ...)`, this should lookup the `PropEntry<Value, Value>` by reference and invoke its `handler` member if `F3 == ON`.
