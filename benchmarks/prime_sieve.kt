// Benchmark: Sieve of Eratosthenes (1,000,000)

fn sieve(n: int) -> int {
    let is_prime = [n + 1]bool
    mut let i = 2
    while i * i <= n {
        if !is_prime[i] {
            mut let j = i * i
            while j <= n {
                is_prime[j] = true
                j = j + i
            }
        }
        i = i + 1
    }

    mut let count = 0
    i = 2
    while i <= n {
        if !is_prime[i] { count = count + 1 }
        i = i + 1
    }
    return count
}

fn main() -> int {
    let start = time_us()
    let result = sieve(1000000)
    let elapsed = time_us() - start

    print("primes up to 1,000,000: " + str(result))
    print("time: " + str(elapsed) + " us")
    return 0
}

// Expected: 78498 primes under 1,000,000
