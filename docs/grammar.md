### Grammar (Expressions)

```
<param> = "..."? <identifier>

<primary> = "undefined" | "null" | "this" | <identifier> | <boolean> | <number> | <object> | <array> | <lambda> | "(" <expr> ")"
<object> = "{" (<property> ",")* "}"
<property> = <identifier> : <expr>
<array> = "[" (<expr> ("," <expr>)* )? "]"
<lambda> = "function" "(" <identifier> ( "," <identifier> )* ")" <block>
<member> = <primary> ( "." <identifier> | "[" <expr> "]" )*
    ; TODO: Update member grammar rule to support longer accesses.
<new> = "new"? <member>
<call> = <new> ( "(" ( <expr> ( "," <expr> )* )? ")" )?
<postfix-unary> = <call> ( "++" | "--" )?
<prefix-unary> = ( "!" | "+" | "++" | "--" | "typeof" | "void" )? <postfix-unary>
<factor> = <prefix-unary> ( ( "%" | "*" | "/" ) <prefix-unary> )*
<term> = <factor> ( ( "+" | "-" ) <factor> )*
<compare> = <term> ( ( "<" | ">" | "<=" | ">=" | "instanceof" ) <term> )*
<equality> = <compare> ( ( "==" | "!=" | "===" | "!==" ) <compare> )*
<logical-and> = <equality> ( "&&" <equality> )*
<logical-or> = <logical-and> ( "||" <logical-and> )*
<expr> = <logical-or>
```

### Grammar (Statements)
```
<program> = <stmt>+
<stmt> = <variable> | <if> | <return> | <break> | <continue> | <throw> | <try-catch> | <while> | <for> | <function> | <expr-stmt>
<variable> = "var" <var-decl> ( "," <var-decl>)* ";"
<var-decl> = <identifier> ( "=" <expr> )?
<if> = "if" "(" <expr> ")" <block> ( "else" ( <block> | <if> | <return> | <expr-stmt> ) )? ; maybe add dangling while loops later, meh
<return> = "return" <expr> ";"
<while> = "while" "(" <expr> ")" <block>   ; just have loops contain block bodies for simplicity!
<for> = "for" "(" <expr> | <variable>? ";" <expr>? ";" <expr>? ")" <stmt>   ; omitted check-expr becomes `djs_push <true>` but other parts become NOPs.
<break> = "break" ";"
<continue> = "continue" ";"
<throw> = "throw" <expr> ";"
<try-catch> = "try" <block> "catch" "(" <identifier> ")" <block>
<function> = "function" <identifier> "(" ( <identifier> ( "," <identifier> )* )? ")" <block>
<block> = "{" <stmt>+ "}"
<expr-stmt> = <prefix-unary> ( "=" <expr> )? ";"
```

### Other Notes
 - I should check if breaks & continues are in a loop, even at top-level.
