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
<unary> = ( "!" | "-" )? <call>
<factor> = <unary> ( ( "%" | "*" | "/" ) <unary> )*
<term> = <factor> ( ( "+" | "-" ) <factor> )*
<compare> = <term> ( ( "<" | ">" | "<=" | ">=" ) <term> )*
<equality> = <compare> ( ( "==" | "!=" | "===" | "!==" ) <compare> )*
<expr> = <equality>
```

### Grammar (Statements)
```
<program> = <stmt>+
<stmt> = <variable> | <if> | <return> | <function> | <expr-stmt>
<variable> = "var" <var-decl> ( "," <var-decl>)* ";"
<var-decl> = <identifier> ( "=" <expr> )?
<if> = "if" "(" <expr> ")" <block>
<return> = "return" <expr> ";"
<function> = "function" <identifier> "(" ( <identifier> ( "," <identifier> )* )? ")" <block>
<block> = "{" <stmt>+ "}"
<expr-stmt> = <call> ( "=" <expr> )? ";"
```

### TO-DO's:
 1. ~~Fix duplicate constant storage in bytecode.~~
 2. Shrink VM value to 8B. (**PENDING**)
 3. ~~Refactor VM to use TCO?~~
 4. Add `||` or `&&` operator support.
 5. Add `else` statement support.
 6. Add `print(arg)` native function.
 6. Add `while` statement support.
 7. Add semantic warnings in yellow text.
 8. Add static-size & heap strings with interning.
 9. Add objects.
