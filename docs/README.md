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
<primary> = "undefined" | "null" | "this" | <identifier> | <boolean> | <number> | "(" <expr> ")"
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
 7. Add edge case of JS: support for undefined variable declarations.
   - `var x;` should be `undefined` IIRC...
 8. Add static-size & heap strings with interning.
    - Redo `Object*` pointer and `Object`, `Prototype` utility types as per _ECMAScript 5, Section 8.6.2, Table 8_.
    - Add implementation-specific `StringBase` virtual class.
    - Add internal `StaticString` type. This is for constant strings no longer than 16 characters.
    - Add internal `String` type for flexible strings. This may wrap a `std::string`.
 8. Add simple data objects.
 9. Add built-in context objects and global objects
    - Implement `[Console console]` basics: `log`, `prompt` (DerkJS extension)
    - Implement `this` objects that reference anything in a local scope (and link to parent ones).
 10. Add mark and sweep GC.
