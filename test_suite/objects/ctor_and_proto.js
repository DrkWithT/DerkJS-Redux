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
var num = 0;

while (num < 10) {
    num = odds.next();

    console.log(num);
}

return 0;
