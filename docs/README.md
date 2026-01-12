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

### Simple Demo
<img src="imgs/DerkJS_console_log.png" size="25%">

### Usage
 1. Give `./utility.sh` run permissions.
 2. Run `./utility.sh help` to see info on building, sloc count, etc.
 3. Run `./build/derkjs_tco -h` for help if you've successfully built the TCO intepreter version.
  - **WARNING:** The loop switch version of the VM is _not yet updated_ to support simple objects & native objects.

### Other Repo Docs:
 - [Subset Grammar](../docs/grammar.md)
 - [OLD Semantic Checking](../docs/sema_checks.md)

### TO-DO's:
 1. ~~Fix duplicate constant storage in bytecode.~~
 2. Shrink VM value to 8B. (**PENDING**)
 3. ~~Refactor VM to use TCO?~~
 4. ~~Add `||` or `&&` operator support.~~
 5. ~~Add `else` statement support.~~
 6. ~~Add `while` statement support.~~
 7. ~~Add support for undefined variable declarations.~~
 8. ~~Add static-size & heap strings with interning.~~
 9. Add simple data objects.
    - Each object instance maps property descriptors to values.
      - Prototype inheritance exists because of any object's prototype chain to the `[null Prototype]` prototype... Built-in prototypes such as `Object` exist just before `[null Prototype]`.
      - Prototypes can be "template objects".
      - Some properties are inherent duplicates from its prototype(s).
      - Some properties are instance-specific properties a.k.a "own" props.
      - Instances are _clones_ of prototypes!
 10. Add custom natives: Console, Date objects!
    - Implement native objects and function wrappers for bytecode / native callables!
      - Bytecode objects just hold an `Instruction` buffer, and `call()` takes an `ExternVMCtx*, PropPool<PropertyHandle<Value>, Value>`.
    - Implement:
      - `console.log('Hello World');`
      - `console.read('Enter something: ');` -> Should print the prompt string and return a line from STDIN??
      - `Date.nano();` -> Gives a whole number of nanoseconds since the Epoch.
    - Implement support for function-level & global `this`.
 11. Add built-in context objects and global objects for `this`.
 12. Implement usage of `this` in constructor functions with `new` & object "methods".
    - Implement `[Console console]` basics: `log`, `clear`, `prompt` (DerkJS extension)
 13. Add mark and sweep GC.
