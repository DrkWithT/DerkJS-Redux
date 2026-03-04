function fib(n) {
    if (n < 2) {
        return n;
    }

    return fib(n - 1) + fib(n - 2);
}

var start_ms = Date.now();
var ans = fib(35);
var end_ms = Date.now();

console.log("answer:", ans, "run-time:", end_ms - start_ms);
