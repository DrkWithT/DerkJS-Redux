/* JS object method test */

var foo = {
    x: 1,
    next: function() {
        var temp = this.x;
        
        this.x = temp + 1;

        return temp;
    }
};

console.log(foo.next());
console.log(foo.next());
console.log(foo.next());

return 0;
