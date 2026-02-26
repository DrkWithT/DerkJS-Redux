var secret = 1;

var data = {
    secret: 2
};

function test(a) {
    return a + this.secret;
}

var ok = 0;
var x = test.call(data, 67);
var y = test.call(null, 67);

if (x === 69) {
    ++ok;
} else {
    console.log("Unexpected x:", x);
}

if (y === 68) {
    ++ok;
} else {
    console.log("Unexpected y:", y);
}

console.log("PASS:", ok === 2);
