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
var term = 0;

while (term < 10) {
    term = odds.next();
    console.log(term);
}

return 0;
