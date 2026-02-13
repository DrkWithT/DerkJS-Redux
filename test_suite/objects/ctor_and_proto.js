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

console.log(odds.next() == 1);
console.log(odds.next() == 3);
console.log(odds.next() == 5);

return 0;
