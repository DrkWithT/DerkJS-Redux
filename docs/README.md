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
<logical-and> = <equality> ( "&&" <equality> )*
<logical-or> = <logical-and> ( "||" <logical-and> )*
<expr> = <logical-or>
```

### Grammar (Statements)
```
<program> = <stmt>+
<stmt> = <variable> | <if> | <return> | <function> | <expr-stmt>
<variable> = "var" <var-decl> ( "," <var-decl>)* ";"
<var-decl> = <identifier> ( "=" <expr> )?
<if> = "if" "(" <expr> ")" <block> "else" ( <block> | <if> | <return> | <expr-stmt> )
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
    - These logical operators _must_ short circuit. As per the ES5 spec, these logical operators only give either LHS or RHS, _not necessarily_ a boolean.
        - OR: If LHS is true, yield it. Otherwise, RHS is yielded.
        - ADD: If LHS is false, yield it. Otherwise, RHS is yielded.
    - To-Do: implement `op_jump_if`, `op_jump`!
 5. Add `else` statement support.
 6. Add `print(arg)` native function.
 6. Add `while` statement support.
 7. Add semantic warnings in yellow text.
 8. Add static-size & heap strings with interning.
 9. Add objects.
