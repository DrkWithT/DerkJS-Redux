var secret = 1;

var data = {
    secret: 2
};

function test(a) {
    return a + this.secret;
}

var x = test.call(data, 67);

if (x === 69) {
    console.log("PASS");
} else {
    console.log("Unexpected x:", x);
    throw new Error("Test failed, see logs.");
}
