/**
 * @file stdlib/polyfill.js
 * @summary Implements ES5 polyfills for everything that doesn't need the DerkJS VM API.
 */


/// Patch polyfills for built-in prototypes / functions:

function isNaN(arg) {
    if (arg instanceof Array) {
        return false;
    }

    return +arg !== +arg;
}

function displayAnyError() {
    return String(this.name) + " " + String(this.message);
}

Object.prototype.toString = function () {
    if (this === undefined) {
        return "[object Undefined]";
    } else if (this === null) {
        return "[object Null]";
    } else {
        return "[object Object]";
    }
};

Object.prototype.constructor = Object;

Boolean.prototype.toString = function () {
    if (this instanceof Boolean === false) {
        throw new Error("Only booleans are allowed for Boolean.toString().");
    }

    if (this.valueOf()) {
        return "true";
    }

    return "false";
};

Boolean.prototype.constructor = Boolean;

Number.prototype.toString = function () {
    return String(this.valueOf());
};

Number.prototype.constructor = Number;

// Workaround for a bug within DerkJS setup: native function this-ptr mysteriously mutates before runtime.
String.prototype.constructor = String;

String.prototype.toString = function () {
    return new String(this);
};

Array.isArray = function (arg) {
    if (typeof arg !== "object") {
        return false;
    }

    return arg instanceof Array;
};

Array.prototype.at = function(index) {
    if (index < 0) {
        index = index + this.length;
    }

    return this[index];
};

// push is native

Array.prototype.pop = function() {
    var old = this.at(-1);

    this.length = this.length - 1;

    return old;
};

Array.prototype.indexOf = function (item, fromIndex) {
    var pos = fromIndex;
    var end = this.length;
    var result = -1;

    if (!pos || pos < 0) {
        pos = 0;
    }

    while (pos < end) {
        if (this.at(pos) === item) {
            result = pos;
            break;
        }

        ++pos;
    }

    return result;
};

Array.prototype.lastIndexOf = function (item, fromIndex) {
    var pos;
    var end = -1;
    var result = -1;

    if (fromIndex === undefined || fromIndex === null) {
        pos = this.length - 1;
    } else if (fromIndex >= 0) {
        pos = +fromIndex;
    } else {
        pos = this.length - (0 - fromIndex);
    }

    if (pos > this.length - 1) {
        pos = this.length - 1;
    }

    while (pos > end) {
        if (this[pos] === item) {
            result = pos;
            break;
        }

        --pos;
    }

    return result;
};

Array.prototype.reverse = function () {
    var left = 0;
    var right = this.length - 1;
    var temp;

    while (left < right) {
        temp = this[left];
        this[left] = this[right];
        this[right] = temp;

        ++left;
        --right;
    }

    return this;
};

Array.prototype.forEach = function (callbackFn, thisArg) {
    var pos = 0;
    var end = this.length;

    if (typeof callbackFn !== "function") {
        return undefined;
    }

    while (pos < end) {
        var discard = callbackFn.call(thisArg, this[pos]);

        ++pos;
    }

    return undefined;
};

Array.prototype.map = function (callbackFn, thisArg) {
    var pos = 0;
    var end = this.length;
    var temp = [];

    if (typeof callbackFn !== "function") {
        return temp;
    }

    while (pos < end) {
        var discard = temp.push(callbackFn.call(thisArg, this[pos]));
        ++pos;
    }

    return temp;
};

Array.prototype.filter = function (predicateFn, thisArg) {
    var pos = 0;
    var end = this.length;
    var temp = [];

    if (typeof predicateFn !== "function") {
        return temp;
    }

    while (pos < end) {
        var item = this[pos];

        if (predicateFn.call(thisArg, item) === true) {
            temp.push(item);
        }

        ++pos;
    }

    return temp;
};

Array.prototype.some = function (predicateFn, thisArg) {
    var pos = 0;
    var end = this.length;
    var ok = true;

    if (typeof predicateFn !== "function") {
        return false;
    }

    while (pos < end) {
        var item = this[pos];

        if (!predicateFn.call(thisArg, item)) {
            ok = false;
            break;
        }

        ++pos;
    }

    return ok;
};

Array.prototype.toString = function () {
    return this.join();
};

Array.prototype.constructor = Array;

Function.prototype.toString = function () {
    return new String(this);
};

Function.prototype.constructor = Function;

function Error(msg) {
    this.message = String(msg);
}

Error.prototype = {
    name: "Error",
    message: "",
    constructor: Error,
    toString: displayAnyError
};

function SyntaxError(msg) {
    this.message = String(msg);
}

SyntaxError.prototype = {
    name: "SyntaxError",
    message: "",
    constructor: SyntaxError,
    toString: displayAnyError
};

function TypeError(msg) {
    this.message = String(msg);
}

TypeError.prototype = {
    name: "TypeError",
    message: "",
    constructor: TypeError,
    toString: displayAnyError
};

console.log = function (...args) {
    var argCount = args.length;
    var temp;
    var discard;

    for (var argPos = 0; argPos < argCount; ++argPos) {
        temp = args.at(argPos);

        //? TODO: support Function.toString().
        if (temp === null) {
            discard = nativePrint("null", 32);
        } else if (typeof temp === "object") {
            discard = nativePrint(temp.toString(), 32);
        } else {
            discard = nativePrint(temp, 32);
        }
    }

    discard = nativePrint(" ", 10);

    return discard; // undefined
};

console.readln = nativeReadLine;

/// By the spec, freeze built-in prototypes:

Object.prototype.freeze();
Boolean.prototype.freeze();
Number.prototype.freeze();
String.prototype.freeze();
Array.prototype.freeze();
Function.prototype.freeze();
Error.prototype.freeze();
