function fib(n) {
    if (n < 2) {
        return n;
    }

    return fib(n - 1) + fib(n - 2);
}

var start_ms = Date.now();
var ans = fib(40);
var end_ms = Date.now();

console.log("PASS:", ans === 102334155, "Run-Time:", end_ms - start_ms);
