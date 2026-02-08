/**
 * @file parser.js
 * @summary The ULTIMATE TEST for DerkJS: parse a simple MDAS arithmetic expression.
 * @author DrkWithT
 */

var token_names = ["SPACES", "NUMBER", "*", "/", "+", "-", "BAD", "EOF"];

// var tag_spaces = 0;
// var tag_num = 1;
// var tag_op_mul = 2;
// var tag_op_div = 3;
// var tag_op_plus = 4;
// var tag_op_minus = 5;
// var tag_bad = 6;
// var tag_eof = 7;

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
            return 4;
        } else if (c_code == 45) {
            return 5;
        } else if (c_code == 42) {
            return 2;
        } else if (c_code == 47) {
            return 3;
        } else {
            return 6;
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
            tkType: 0,
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
            tkType: 1,
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
                tkType: 7,
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

var exprSrc = console.readln("Enter expr: ");
var tempToken = Lexer.init(exprSrc);

while (!Lexer.done()) {
    tempToken = Lexer.next();

    if (tempToken.tkType == 7) {
        console.log("EOF");
    } else {
        console.log(
            "Token [start, length, tag]",
            tempToken.tkStart,
            tempToken.tkLen,
            token_names.at(tempToken.tkType)
        );
    }
}

return 0;
