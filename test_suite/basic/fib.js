function fib_r(n, self_fn) {
    if (n < 2) {
        return n;
    }

    return self_fn(n - 1, self_fn) + self_fn(n - 2, self_fn);
}

/* temporary hack before closure support */
var ans = fib_r(30, fib_r);

console.log(ans);

return 0;
