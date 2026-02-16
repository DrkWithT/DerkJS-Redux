var ans = (function(a, b) {
    var foo = function(x, y) { return x * y; };

    return foo(a, b);
})(2, 21);

// Should be `true`
console.log("PASS:", ans === 42);
