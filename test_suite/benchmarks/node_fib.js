/**
 * @note Use this with NodeJS V8 jitless for a fair comparison against DerkJS.
 * @summary Does a recursive fibonacci 30 program with timings.
 */

function fib_r_node(n) {
    if (n < 2) {
        return n;
    }

    return fib_r_node(n - 1) + fib_r_node(n - 2);
}

const fib_begin_time_ms = Date.now();
const ans = fib_r_node(35);
const fib_runtime_ms = Date.now() - fib_begin_time_ms;
console.log(`answer = ${ans}, runtime = \x1b[1;33m${fib_runtime_ms}ms\x1b[0m`);
