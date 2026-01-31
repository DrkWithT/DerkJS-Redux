/**
 * @file parser.js
 * @summary The ULTIMATE TEST for DerkJS: parse a simple MDAS arithmetic expression.
 * @author DrkWithT
 */

var tag_spaces = 0;
var tag_num = 1;
var tag_op_mul = 2;
var tag_op_div = 3;
var tag_op_plus = 4;
var tag_op_minus = 5;
var tag_bad = 6;
var tag_eof = 7;

var matchSpace = function (c_code) {
    return c_code == 32;
}

var matchDigit = function(c_code) {
    return c_code >= 48 && c_code <= 57;
};

var matchOp = function(c_code) {
    if (c_code == 43) {
        return tag_op_plus;
    } else if (c_code == 45) {
        return tag_op_minus;
    } else if (c_code == 42) {
        return tag_op_mul;
    } else if (c_code == 47) {
        return tag_op_div;
    }

    return tag_bad;
};

var Lexer = {
    src: null,
    pos: 0,
    end: 0,
    init: function(txt) {
        this.src = txt;
        this.pos = 0;
        this.end = txt.len();
    },
    done: function() {
        return this.pos >= this.end;
    },
    lexSpaces: function() {
        var done = false;
        var begin = this.pos;
        var count = 0;
        var c;

        while (!this.done() && !done) {
            c = this.src.charCodeAt(begin + count);

            if (!matchSpace(c)) {
                done = true;
            } else {
                count = count + 1;
                this.pos = begin + count;
            }
        }

        return {
            tkType: tag_spaces,
            tkStart: begin,
            tkLen: count
        };
    },
    lexNumber: function() {
        var done = false;
        var begin = this.pos;
        var count = 0;
        var c;

        while (!this.done() && !done) {
            c = this.src.charCodeAt(begin + count);

            if (!matchDigit(c)) {
                done = true;
            } else {
                count = count + 1;
                this.pos = begin + count;
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
        var op_tk_tag = matchOp(op_ascii);

        this.pos = begin + 1;

        return {
            tkType: op_tk_tag,
            tkStart: begin,
            tkLen: 1
        };
    },
    next: function() {
        var peek = this.src[this.pos];

        if (this.done()) {
            return {
                tkType: tag_eof,
                tkStart: this.end,
                tkLen: 1
            };
        }

        if (matchSpace(peek)) {
            return this.lexSpaces();
        } else if (matchDigit(peek)) {
            return this.lexNumber();
        } else {
            return this.lexOp();
        }
    }
};

var exprSrc = console.readln("Enter expr: ");
var tempToken;

Lexer.init(exprSrc);

while (!Lexer.done()) {
    tempToken = Lexer.next();

    if (tempToken.tkType == tag_eof) {
        console.log("EOF");
    } else {
        console.log("Token [start, length, tag]", tempToken.tkStart, tempToken.tkLen, tempToken.tkType);
    }
}

return 0;
