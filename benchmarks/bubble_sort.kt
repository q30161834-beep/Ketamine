// Benchmark: Bubble sort (10,000 elements)

fn sort(arr: [int]) {
    let n = len(arr)
    mut let i = 0
    while i < n {
        mut let j = 0
        while j < n - i - 1 {
            if arr[j] > arr[j + 1] {
                let tmp = arr[j]
                arr[j] = arr[j + 1]
                arr[j + 1] = tmp
            }
            j = j + 1
        }
        i = i + 1
    }
}

fn main() -> int {
    let n = 10000
    let arr = [n]int

    // Initialize with random-ish values
    mut let i = 0
    while i < n {
        arr[i] = (i * 7 + 13) % 1000
        i = i + 1
    }

    let start = time_us()
    sort(arr)
    let elapsed = time_us() - start

    print("sorted " + str(n) + " elements in " + str(elapsed) + " us")
    return 0
}
