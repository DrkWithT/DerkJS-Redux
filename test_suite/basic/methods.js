/* JS object method test */

var foo = {
    x: 1,
    next: function() {
        // gets 'x', not 1
        var temp = this.x;
        
        // this.x = temp + 1;

        return temp;
    }
};

console.log("PASS:", foo.next() === 1);
