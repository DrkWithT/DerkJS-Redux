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

### Grammar (Expressions)
```
<primary> = "undefined" | "null" | "this" | <identifier> | <boolean> | <number> | <object> | "(" <expr> ")"
<object> = "{" (<property> ",")* "}"
<property> = <identifier> : <expr>
<member> = <primary> ( "." <call> | "[" <expr> "]" )?
<new> = "new"? <member>
<call> = <new> ( "(" ( <expr> ( "," <expr> )* )? ")" )?
<unary> = ( "!" )? <call>
<factor> = <unary> ( ( "%" | "*" | "/" ) <unary> )*
<term> = <factor> ( ( "+" | "-" ) <factor> )*
<compare> = <term> ( ( "<" | ">" | "<=" | ">=" ) <term> )*
<equality> = <compare> ( ( "==" | "!=" | "===" | "!==" ) <compare> )*
<logical-and> = <equality> ( "&&" <equality> )*
<logical-or> = <logical-and> ( "||" <logical-and> )*
<expr> = <logical-or>
```

### Grammar (Statements)
```
<program> = <stmt>+
<stmt> = <variable> | <if> | <return> | <while> | <function> | <expr-stmt>
<variable> = "var" <var-decl> ( "," <var-decl>)* ";"
<var-decl> = <identifier> ( "=" <expr> )?
<if> = "if" "(" <expr> ")" <block> ( "else" ( <block> | <if> | <return> | <expr-stmt> ) )? ; maybe add dangling while loops later, meh
<return> = "return" <expr> ";"
<while> = "while" "(" <expr> ")" <block>   ; just have loops contain block bodies for simplicity!
<function> = "function" <identifier> "(" ( <identifier> ( "," <identifier> )* )? ")" <block>
<block> = "{" <stmt>+ "}"
<expr-stmt> = <call> ( "=" <expr> )? ";"
```

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
      - Implement `this` as a special reference variable to the current context's instance-`Object`.
 10. Add built-in context objects and global objects
    - Implement `[Console console]` basics: `log`, `clear`, `prompt` (DerkJS extension)
 11. Add mark and sweep GC.
