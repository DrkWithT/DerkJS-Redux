function fib(n) {
    if (n < 2) {
        return n;
    }

    return fib(n - 1) + fib(n - 2);
}

var ans = fib(30);

console.log("PASS:", ans === 832040);
