/// JS Polyfills for anything that I don't need the DerkJS VM API for.

Array.prototype.at = function(index) {
    if (index < 0) {
        index = index + this.length;
    }

    return this[index];
};

// push is native

Array.prototype.pop = function() {
    var oldLen = this.length;

    --this.length;
    return oldLen;
};

Array.prototype.indexOf = function (item, fromIndex) {
    var pos = fromIndex || 0;
    var end = item.length;
    var result = -1;

    while (pos < end) {
        if (this[pos] === item) {
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

    if (fromIndex === undefined) {
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

// TODO: Array.prototype.filter, Array.prototype.some
