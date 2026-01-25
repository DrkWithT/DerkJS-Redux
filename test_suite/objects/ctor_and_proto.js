/*
    ctor_and_proto.js
    Tests basic constructor functions and setting prototype properties.
 */

var Sequence = function(start, step) {
    this.item = start;
    this.step = step;

    return this;
}

Sequence.prototype.next = function () {
    var temp = this.item;

    this.item = temp + this.step;

    return temp;
};

var evens = new Sequence(0, 2);

/* Should produce 0, 2, 4, 6 */
console.log(evens.next());
console.log(evens.next());
console.log(evens.next());
console.log(evens.next());

return 0;
