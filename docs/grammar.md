### Grammar (Expressions)

```
<primary> = "undefined" | "null" | "this" | <identifier> | <boolean> | <number> | <object> | <lambda> | "(" <expr> ")"
<object> = "{" (<property> ",")* "}"
<lambda> = "function" "(" <identifier> ( "," <identifier> )* ")" <block>
<property> = <identifier> : <expr>
<member> = <primary> ( "." <member> | "[" <expr> "]" )?
    ; TODO: Update member grammar rule to support longer accesses.
<new> = "new"? <member>
<call> = <new> ( "(" ( <expr> ( "," <expr> )* )? ")" )?
<unary> = ( "!" | "+" )? <call>
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
