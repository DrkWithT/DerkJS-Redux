/*
    ctor_and_proto.js
    Tests basic constructor functions and setting prototype properties.
*/

var Sequence = function(start) {
    this.item = start;

    return this;
};

Sequence.prototype.next = function() {
    var temp = this.item;

    this.item = temp + 2;

    return temp;
};

var odds = new Sequence(1);
var x = odds.next();
var y = odds.next();
var z = odds.next();
var ok = 0;

if (x === 1) {
    ++ok;
} else {
    console.log("Unexpected value by odds:", x);
}

if (y === 3) {
    ++ok;
} else {
    console.log("Unexpected value by odds:", y);
}

if (z === 5) {
    ++ok;
} else {
    console.log("Unexpected value by odds:", z);
}

if (ok === 3) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
