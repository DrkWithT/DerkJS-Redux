### Roadmap:
 1. ~~Fix duplicate constant storage in bytecode.~~
 2. NaN boxing (cancelled)
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
 33. ~~`Function.prototype` should be a dud function (return `undefined` & length = `0`).~~
 34. Improve operations for objects:
    - **DELAYED:** Improve `as_string` method of object-base... Pretty print object literal / `[object <class name> ... ]`
 35. ~~Add `x instanceof y` operator.~~
 36. ~~Add rest parameters by _ES6_.~~
 37. Add `delete` operator.
   - Add `delete` operator bytecode support:
      - `djs_try_del` opcode:
         - If the value is not a reference, return `true`.
         - If the value holds an "own property" reference of some object, try removing it. The value must be configurable.
         - If the value holds an environment (capture object) property's reference, try removing it. The value must be configurable.
         - Throw errors as indicated below.
      - Errors:
         - Attempting a variable, argument parameter, or function name deletion will throw a `SyntaxError`.
         - Accessing an `undefined.property_name` will throw a `TypeError`. See `djs_get_prop` opcode for adding a `TypeError` throw check.
 39. Add postfix `++` and `--` operators. **WIP**
 40. Improve runtime errors: **WIP**
   - `ReferenceError` on bad property accesses.
   - `SyntaxError` on bad syntax evaluation / Function ctor calls.
 41. Add `__proto__` AKA `[[prototype]]` support:
    - Add `__proto__` to parsing.
    - Add `__proto__` support to bytecode & compiler.
    - Add `__proto__` support to `Value` & VM.
    - Add `__proto__` test cases.
 41. Add more support for built-in methods:
    - Number methods
    - String methods
    - Object methods: seal, isFrozen, isSealed
    - Date methods: toString?? toDateString??
    - Math methods: pow, cos, sin, tan, log, logn, floor, ceil??
    - Array methods: sort, splice??
