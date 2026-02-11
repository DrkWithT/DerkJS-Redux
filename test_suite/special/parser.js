/**
 * @file parser.js
 * @summary The ULTIMATE TEST for DerkJS: parse a simple MDAS arithmetic expression.
 * @author DrkWithT
 */

var token_names = ["SPACES", "NUMBER", "*", "/", "+", "-", "BAD", "EOF"];

var tag_spaces = 0;
var tag_num = 1;
var tag_op_mul = 2;
var tag_op_div = 3;
var tag_op_plus = 4;
var tag_op_minus = 5;
var tag_bad = 6;
var tag_eof = 7;

var op_none = 0;
var op_mul = 1;
var op_div = 2;
var op_add = 3;
var op_sub = 4;

var Lexer = {
    init: function(txt) {
        this.src = txt;
        this.pos = 0;
        this.end = txt.len();
        return undefined;
    },
    matchDigit: function(c_code) {
        return c_code >= 48 && c_code <= 57;
    },
    matchOp: function(c_code) {
        if (c_code == 43) {
            return tag_op_plus;
        } else if (c_code == 45) {
            return tag_op_minus;
        } else if (c_code == 42) {
            return tag_op_mul;
        } else if (c_code == 47) {
            return tag_op_div;
        } else {
            return tag_bad;
        }
    },
    done: function() {
        return this.pos >= this.end;
    },
    lexSpaces: function() {
        var begin = this.pos;
        var count = 0;
        var c = undefined;

        while (!this.done()) {
            c = this.src.charCodeAt(this.pos);

            if (c != 32) {
                break;
            } else {
                count = count + 1;
                this.pos = this.pos + 1;
            }
        }

        return {
            tkType: tag_spaces,
            tkStart: begin,
            tkLen: count
        };
    },
    lexNumber: function() {
        var begin = this.pos;
        var count = 0;
        var c = undefined;

        while (!this.done()) {
            c = this.src.charCodeAt(this.pos);

            if (!this.matchDigit(c)) {
                break;
            } else {
                count = count + 1;
                this.pos = this.pos + 1;
            }
        }

        return {
            tkType: tag_num,
            tkStart: begin,
            tkLen: count
        };
    },
    lexOp: function() {
        var begin = this.pos;
        var op_ascii = this.src.charCodeAt(begin);
        var op_tk_tag = this.matchOp(op_ascii);
        
        this.pos = this.pos + 1;

        return {
            tkType: op_tk_tag,
            tkStart: begin,
            tkLen: 1
        };
    },
    next: function() {
        var curr = this.pos;
        var peek = this.src.charCodeAt(curr);

        if (this.done()) {
            return {
                tkType: tag_eof,
                tkStart: curr,
                tkLen: 1
            };
        }

        if (peek == 32) {
            return this.lexSpaces();
        } else if (this.matchDigit(peek)) {
            return this.lexNumber();
        } else {
            return this.lexOp();
        }
    }
};

var previous = null, current = {tkType: tag_bad, tkStart: 0, tkLen: 0};

function advance(lexer) {
    var temp;

    while (!lexer.done()) {
        temp = lexer.next();

        if (temp.tkType != tag_spaces) {
            break;
        }
    }

    return temp;
}

function consumeOn(lexer, tag0, tag1, msg) {
    if (!tag0 || current.tkType == tag0 || current.tkType == tag1) {
        previous = current;
        current = advance(lexer);
        return true;
    }

    console.log(msg);

    return false;
}

function matchToken(token, tag) {
    return token.tkType == tag;
}

function parseLiteral(lexer, source) {
    if (current.tkType != tag_num) {
        console.log("Expected int literal at pos:", current.tkStart);
        return undefined;
    }

    var lexeme = source.substr(current.tkStart, current.tkLen);
    var n = parseInt(lexeme);
    var foo = consumeOn(lexer, undefined, undefined, "");

    return {op: op_none, data: n};
}

function parseFactor(lexer, source) {
    var lhs = parseLiteral(lexer, source);

    while (!lexer.done()) {
        if (current.tkType == tag_op_mul) {
            var foo = consumeOn(lexer, undefined, undefined, null);
            lhs = {
                op: op_mul,
                left: lhs,
                right: parseLiteral(lexer, source)
            };
        } else if (current.tkType == tag_op_div) {
            var foo = consumeOn(lexer, undefined, undefined, null);
            lhs = {
                op: op_div,
                left: lhs,
                right: parseLiteral(lexer, source)
            };
        } else {
            break;
        }
    }

    return lhs;
}

function parseTerm(lexer, source) {
    var lhs = parseFactor(lexer, source);

    while (!lexer.done()) {
        if (current.tkType == tag_op_plus) {
            var foo = consumeOn(lexer, undefined, undefined, null);
            lhs = {
                op: op_add,
                left: lhs,
                right: parseFactor(lexer, source)
            };
        } else if (current.tkType == tag_op_minus) {
            var foo = consumeOn(lexer, undefined, undefined, null);
            lhs = {
                op: op_sub,
                left: lhs,
                right: parseFactor(lexer, source)
            };
        } else {
            break;
        }
    }

    return lhs;
}

function walkAST(root) {
    if (!root) {
        return 0;
    }

    if (root.op == op_mul) {
        return walkAST(root.left) * walkAST(root.right);
    } else if (root.op == op_div) {
        return walkAST(root.left) / walkAST(root.right);
    } else if (root.op == op_add) {
        return walkAST(root.left) + walkAST(root.right);
    } else if (root.op == op_sub) {
        return walkAST(root.left) - walkAST(root.right);
    } else {
        return root.data;
    }
}

while (true) {
    var exprSource = console.readln("Type expr / 'q': ");

    if (exprSource == "q") {
        break;
    }
    
    var foo = Lexer.init(exprSource);

    current = consumeOn(Lexer, undefined, undefined, "");

    var answer = walkAST(parseTerm(Lexer, exprSource));

    if (answer != undefined) {
        console.log("ans:", answer);
    }
}

return 0;
