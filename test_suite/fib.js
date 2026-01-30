function fib_r(n) {
    if (n < 2) {
        return n;
    }

    return fib_r(n - 1) + fib_r(n - 2);
}

console.log(fib_r(35));

return 0;
