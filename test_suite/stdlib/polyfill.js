/**
 * @file stdlib/polyfill.js
 * @summary Implements ES5 polyfills for everything that doesn't need the DerkJS VM API.
 */

// Workaround for a bug within DerkJS setup: native function this-ptr mysteriously mutates before runtime.
String.prototype.constructor = String;

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
