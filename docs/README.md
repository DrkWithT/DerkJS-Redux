## README

### Brief
My latest attempt at implmenting JavaScript under version ES5 in modern C++.

### Dependencies
 - LLVM 21.1.7+ (just the Homebrew distribution for now)
 - CMake 4.2+
 - Ninja

### Caveats
 - `use strict` is the locked default.
 - No automatic semicolon insertion.

### Simple Print
<img src="imgs/DerkJS_lambda_test.png" size="25%">

### Usage
 1. Give `./utility.sh` run permissions.
 2. Run `./utility.sh help` to see info on building, sloc count, etc.
 3. Run `./build/derkjs_tco -h` for help if you've successfully built the TCO intepreter version.
  - **WARNING:** The loop switch version of the VM is _not yet updated_ to support simple objects & native objects.

### Other Repo Docs:
 - [Subset Grammar](../docs/grammar.md)
 - [OLD Semantic Checking](../docs/sema_checks.md)
 - [Objects](../docs/objects.md)

### TO-DO's:
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
 14. Add support for `this` in functions.
   - Add closure opcode support: Makes / modifies an implicit JS object locally. It may be returned.
      - `djs_put_this_obj` & generate `djs_put_prop` invocations for certain assignment: `this.x = 123`.
   - Add support in VM call frames to reference an empty `this` object.
   - Implement usage of `this` in object "methods".
   - Implement `this` within constructor functions using `new`.
      - Add `prototype` support??
 16. Add mark and sweep GC.
