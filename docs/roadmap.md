### Roadmap:
 1. ~~Fix duplicate constant storage in bytecode.~~
 2. Shrink VM value to 8B. (**WONT-FIX**)
 3. ~~Refactor VM to use TCO?~~
 4. ~~Add `||` or `&&` operator support.~~
 5. ~~Add `else` statement support.~~
 6. ~~Add `while` statement support.~~
 7. ~~Add support for undefined variable declarations.~~
 8. ~~Add static-size & heap strings with interning.~~
 9. ~~Add simple data objects.~~
 10. ~~Add custom natives: `console`, `clock` objects!~~
 11. ~~Add larger strings under `ObjectBase<Value>, StringBase` for string literals.~~
 12. ~~Make `banana.js` work.~~
 13. ~~Add anonymous functions as expressions.~~
 14. ~~Add support for `this` in functions & objects.~~
   - Implement `Object.create(), Object.getPrototype()`.
 15. ~~Add Arrays:~~
 16. ~~Add mark and sweep GC.~~
 17. ~~Add `break` and `continue`.~~
 18. ~~Parse function decls differently: `function name() {}` is like `var name = function() {...}`.~~
 19. ~~Hoist `var` decls per scope.~~
 20. ~~Add support for scoped symbol resolution in bytecode generation.~~
    - **FIX GC to track DEAD CAPTURE OBJECTS**!!
 21. ~~Add support for _Property Descriptors_.~~
 22. ~~Add an `Object.method` API:~~
 23. ~~Update `Object` and `Array` built-ins to be more conformant:~~
 24. ~~Add `typeof` and `void` operators.~~
 25. ~~Add implicit `ret` opcode on omitted returns- it yields a `this` reference on ctor-mode calls, discarding any regular expr result.~~
 26. ~~Add `array.length` with side-effects!~~
 27. ~~Make `String` a constructor function & add some methods.~~
 28. ~~Refactor bytecode gen to Dep. Inj. modules for generating each expr / stmt.~~
 29. ~~Add prefix increment & decrement operator.~~
 30. ~~Support Polyfills.~~
 31. ~~Add exceptions (see `codegen.md`).~~
 32. ~~Add for loops.~~
 33. `Function.prototype` should be a dud function (return `undefined` & length = `0`).
 34. Improve operations for objects:
    - Modify `on_accessor_mut` method to take the property key.
    - Improve `as_string` method of object-base... Pretty print object literal / `[object <class name> ... ]`
 35. Add more support for built-in methods:
    - Number methods
    - String methods
    - Object methods: getPrototypeOf, seal, isFrozen, isSealed
    - Date methods: toString?? toDateString??
    - Math methods: pow, cos, sin, tan, log, logn, floor, ceil??
    - Array methods: sort, splice??
 36. Add `__proto__` AKA `[[prototype]]` support:
    - Add `__proto__` to parsing.
    - Add `__proto__` support to bytecode & compiler.
    - Add `__proto__` support to `Value` & VM.
    - Add `__proto__` test cases.
