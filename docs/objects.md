### JS Object Basics
 - Each object instance maps property descriptors to values.
    - Prototype inheritance exists because of any object's prototype chain to the `[null Prototype]` prototype.
      - Some properties are instance-specific properties a.k.a "own" props.
      - Otherwise, some properties are found from their object's prototype.
    - Prototypes are the "foundational" objects referenced by any plain object ("instance").
    - Constructor functions will create a blank object for `this`, and it'll become a prototype of created objects from that constructor.

### Property Descriptors
 - These are "handles" to property values within an object. In the actual implementation, this avoids making multitudes of string allocations- Each property descriptors just contains a reference to the actual key value & another to the parent (uniquely allocated on the VM heap & native heap).
    - By avoiding naive string allocations, memory is saved.
 - Objects just map these descriptors to their own property values.

### Property Lookup
 - An object's own properties are searched 1st, but prototypes are searched as the next resort.
 - A property can be looked up as a primitive or object reference value. Checks on being callable, etc. are at runtime.
 - In the stack VM, the object reference is pushed before a key value / reference on top. Then both are "popped" off and the lower slot is filled with the property's value reference.

### "This"
 - For simplicity, it's a special variable referencing a local context of a function.
   - This would help represent closures... The caller's is the parent prototype, the callee's is the instance.
 - IFF a function returns `this`, it's a constructor.

### Arrays
JS arrays can have "holes" and only integer-based keys can put in sequential items. However, other key types just set object properties of an array object.
