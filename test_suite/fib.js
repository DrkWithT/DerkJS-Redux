function fib_r(n) {
    if (n < 2) {
        return n;
    }

    return fib_r(n - 1) + fib_r(n - 2);
}

display(fib_r(30) == 832040);
