# Ketamine Benchmarks

## Run

```bash
ketc fibonacci.kt --target jit
ketc bubble_sort.kt --target jit
ketc prime_sieve.kt --target jit
```

Or compile to native:
```bash
ketc fibonacci.kt --target x86 -o fib.bin
```

## Results

| Benchmark | Ketamine | Python | Node.js | C (GCC -O3) |
|-----------|----------|--------|---------|-------------|
| fib(40)   | -        | -      | -       | -           |
| sort(10K) | -        | -      | -       | -           |
| sieve(1M) | -        | -      | -       | -           |

*(Complete with actual timings on your machine)*
