function fib(n, self) {
    if (n < 2) {
        return n;
    }

    return self(n - 1, self) + self(n - 2, self);
}

var ans = fib(25, fib);

console.log(ans);

return 0;
