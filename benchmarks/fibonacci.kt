// Benchmark: Fibonacci (recursive)
// Compare with: python, node, etc.

fn fib(n: int) -> int {
    if n <= 1 { return n }
    return fib(n - 1) + fib(n - 2)
}

fn main() -> int {
    let start = time_us()

    let n = 40
    let result = fib(n)

    let elapsed = time_us() - start
    print("fib(" + str(n) + ") = " + str(result))
    print("time: " + str(elapsed) + " us")

    return 0
}

// Expected: fib(40) = 102334155
