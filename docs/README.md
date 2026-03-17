## README

### Brief
My latest attempt at implmenting a subset of JavaScript, specifically ES5 with cherry-picked ES6+ features, in modern C++.

### Required
 - Clang 21+ (just the Homebrew distribution for now)
 - CMake 4.2+
 - Ninja

### Caveats / Quirks
 - No `eval()`.
 - No automatic semicolon insertion.
 - No `arguments`, but rest parameters are given.
 - No BigInt.
 - No Promise API yet.

### Usage
 1. Give `./utility.sh` run permissions.
 2. Run `./utility.sh help` to see info on building, sloc count, etc.
 3. Run `./build/derkjs_tco -h` for help if you've successfully built the TCO intepreter version.

### Other Repo Docs:
 - [Roadmap](../docs/roadmap.md)
 - [Subset Grammar](../docs/grammar.md)
 - [Objects](../docs/objects.md)
 - [OLD Semantic Checking](../docs/sema_checks.md)
