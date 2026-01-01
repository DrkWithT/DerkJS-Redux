"""
test_suite.benchmarks.py3_fib
By: DrkWithT (GitHub)

Contains a timed recursive Fibonacci 30 script for rough benchmarking purposes vs. DerkJS.
"""

import time

def fib_py_r(n):
    if n < 2:
        return n
    else:
        return fib_py_r(n - 1) + fib_py_r(n - 2)

start_fib_time_ns = time.time_ns()
answer = fib_py_r(35)
fib_runtime_ms = (time.time_ns() - start_fib_time_ns) / 1000000
print(f"answer = {answer}, runtime = \x1b[1;33m{fib_runtime_ms}ms\x1b[0m")
