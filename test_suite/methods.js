/* JS object method test */

var countdown = {
    x: 10,
    next: function() {
        var temp = this.x;

        this.x = temp - 1;

        return temp;
    }
};

console.log(countdown.next());
console.log(countdown.next());
console.log(countdown.next());
console.log(countdown.next());
console.log(countdown.next());

return 0;
