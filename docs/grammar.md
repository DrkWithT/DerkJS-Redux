### Grammar (Expressions)

```
<string-literal> = <single-quote> <string-symbol>* <single-quote> | <double-quote> <string-symbol>* <double-quote>
<string-symbol> = <non-quote & not-backslash & not-LF> | <escape-sequence>
<escape-sequence> = <simple-escape> | <hex-escape>
<simple-escape> = "\" ("b" | "f" | "n" | "r" | "t" | "v" | "\" | "'" | """ | "0")
<hex-escape> = "\" "x" <hex-digit>{2}
<hex-digit> = ( [a - f] | [0 - 9] )
<param> = "..."? <identifier>

<primary> = "undefined" | "null" | "this" | <identifier> | <boolean> | <number> | <string-literal> | <object> | <array> | <lambda> | "(" <expr> ")"
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
<stmt> = <variable> | <if> | <return> | <break> | <continue> | <throw> | <try-catch> | <while> | <do-while> | <for> | <function> | <expr-stmt> | <empty-stmt>
<variable> = "var" <var-decl> ( "," <var-decl>)* ";"
<var-decl> = <identifier> ( "=" <expr> )?
<if> = "if" "(" <expr> ")" <stmt> ( "else" <stmt> )? ; maybe add dangling while loops later, meh
<return> = "return" <expr>? ";"
<while> = "while" "(" <expr> ")" <stmt>

<do-while> = "do" <stmt> "while" "(" <expr> ")" ";"

<for> = "for" "(" <expr> | <variable>? ";" <expr>? ";" <expr>? ")" <stmt>   ; omitted check-expr becomes `djs_push <true>` but other parts become NOPs.
<break> = "break" ";"
<continue> = "continue" ";"
<throw> = "throw" <expr> ";"
<try-catch> = "try" <block> "catch" "(" <identifier> ")" <block> ( "finally" <block> )?
<function> = "function" <identifier> "(" ( <identifier> ( "," <identifier> )* )? ")" <block>
<block> = "{" <stmt>+ "}"
<expr-stmt> = <prefix-unary> ( "=" <expr> )? ";"
<empty-stmt> = ";"
```

### Other Notes
 - `break` / `continue` must be in loops -> SyntaxError
 - `return` must be in a function -> SyntaxError
