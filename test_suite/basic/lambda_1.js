var ans = (function(a, b) {
    var foo = function(x, y) { return x * y; };

    return foo(a, b);
})(2, 21);

if (ans === 42) {
    console.log("PASS");
} else {
    console.log("Results:", ans);
    throw new Error("Test failed, see logs.");
}
