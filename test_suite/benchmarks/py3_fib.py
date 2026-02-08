"""
test_suite.benchmarks.py3_fib
By: DrkWithT (GitHub)

Contains a timed recursive Fibonacci 30 script for rough benchmarking purposes vs. DerkJS.
"""

import time

def fib_py_r(n, self_fn):
    if n < 2:
        return n
    else:
        return self_fn(n - 1, self_fn) + self_fn(n - 2, self_fn)

start_fib_time_ns = time.time_ns()
answer = fib_py_r(30, fib_py_r)
fib_runtime_ms = (time.time_ns() - start_fib_time_ns) / 1000000
print(f"answer = {answer}, runtime = \x1b[1;33m{fib_runtime_ms}ms\x1b[0m")
