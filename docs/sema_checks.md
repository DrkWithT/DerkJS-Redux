## Semantic Checker Notes

### Validity
 - Every name must be declared once before use.
 - Only valid LHS values can be assigned to:
    - Invalid LHS: `1 = abc;`
 - Function calls must have matching arity to their function definition's parameter count.
