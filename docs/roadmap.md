### Roadmap:
 1. ~~Fix duplicate constant storage in bytecode.~~
 2. Shrink VM value to 8B. (**PENDING**)
 3. ~~Refactor VM to use TCO?~~
 4. ~~Add `||` or `&&` operator support.~~
 5. ~~Add `else` statement support.~~
 6. ~~Add `while` statement support.~~
 7. ~~Add support for undefined variable declarations.~~
 8. ~~Add static-size & heap strings with interning.~~
 9. ~~Add simple data objects.~~
 10. ~~Add custom natives: `console`, `clock` objects!~~
 11. ~~Add larger strings under `ObjectBase<Value>, StringBase` for string literals.~~
   - ~~Add `console.readline(msg: string);`~~
 12. ~~Make `banana.js` work.~~
   - ~~Add `djs_numify` & `djs_strcat` opcodes.~~
 13. ~~Add anonymous functions as expressions.~~
   - ~~Add parsing support.~~
   - ~~Add semantic checker support.~~
   - ~~Add `Lambda` internals for bytecode-callables.~~
   - ~~Modify the codegen to handle callable objects.~~
 14. ~~Add support for `this` in functions & objects.~~
   - Implement `Object.create(), Object.getPrototype()`.
 15. ~~Add Arrays:~~
   - ~~Create `Array` subclass of `ObjectBase<Value>`.~~
      - ~~Remove need for parent object pointers for property-handles.~~
   - ~~Add `Array` native object (not an actual function) to provide helper methods & be the 'interface" prototype of `[]` objects.~~
      - ~~Array methods: `push, pop, at, indexOf, reverse`.~~
 16. ~~Add mark and sweep GC.~~
 17. ~~Add `break` and `continue`.~~
 18. ~~Parse function decls differently: `function name() {}` is like `var name = function() {...}`.~~
 19. ~~Hoist `var` decls per scope.~~
    - ~~Refactor bytecode gen to stub-define all recursively present `var`s as `undefined`. The 2nd pass over `var`s will assign their "initializers" to them.~~
 20. ~~Add support for scoped symbol resolution in bytecode generation.~~
    - **FIX GC to track DEAD CAPTURE OBJECTS**!!
 21. ~~Add support for _Property Descriptors_.~~
    - ~~Implement `PropertyDescriptor`.~~
    - ~~Replace property handle keys with `Value` keys.~~
    - ~~Update VM opcodes to use `PropertyDescriptor`s in property accesses.~~
 22. ~~Add an `Object.method` API:~~
    - ~~Add Object utility interface with: `new Object(arg), Object.create(proto), Object.freeze()`~~
    - Improve `toString()` operations for objects:
      - Prevent circular reference chasing.
      - Show all properties recursively.
 23. ~~Update `Object` and `Array` built-ins to be more conformant:~~
    - ~~`Object` is a function that makes objects- `new Object(proto)` is the object constructor BUT `Object()` should box primitives later & no-op return existing objects.~~
      - ~~All object literals should have `this.prototype = Object.prototype`.~~
      - ~~`Object.prototype.constructor` must be the `Object` function itself.~~
    - ~~`Array` is a function that makes arrays- `new Array(args...)` and `Array(args...)` should behave identically.~~
      - ~~All array objects should have `this.prototype = Array.prototype`.~~
      - ~~`Array.prototype.constructor` must be the `Array` function itself.~~
    - Add more methods from the ES5 specification.
 24. Add `typeof` operator.
 25. Add `void` operator.
 26. Add `+=, -=, *=, /=, %=` operators.
