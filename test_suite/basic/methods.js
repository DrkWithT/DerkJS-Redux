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

var ans = foo.next();

if (ans === 1) {
    console.log("PASS");
} else {
    console.log("Results:", ans);
    throw new Error("Test failed, see logs.");
}
