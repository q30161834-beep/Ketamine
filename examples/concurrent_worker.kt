// Concurrent Worker Pool in Ketamine — compiles to Go
// Demonstrates goroutines, channels, and sync primitives via go:: FFI
// Run: ketc concurrent_worker.kt --target go -o workers.go && go run workers.go

import go::sync
import go::fmt
import go::time
import go::log

struct Job {
    id:    int,
    input: int
}

struct Result {
    job_id: int,
    output: int,
    worker: int
}

fn do_work(job: Job, worker_id: int) -> Result {
    // Simulate work with sleep
    go::time::Sleep(100 * go::time::Millisecond)
    return Result {
        job_id: job.id,
        output: job.input * job.input,  // compute square
        worker: worker_id
    }
}

fn worker(id: int, jobs: Channel<Job>, results: Channel<Result>, wg: go::sync::WaitGroup) {
    loop {
        let job = jobs.receive()
        if job == null {
            break
        }
        let r = do_work(job, id)
        go::log::Println(
            go::fmt::Sprintf("worker %d: job %d → %d", id, r.job_id, r.output)
        )
        results.send(r)
    }
    wg.Done()
}

fn main() -> int {
    let num_workers = 4
    let num_jobs    = 20

    let jobs    = make_channel(Job,    num_jobs)
    let results = make_channel(Result, num_jobs)
    let wg      = go::sync::WaitGroup{}

    // Start workers
    mut let w = 0
    while w < num_workers {
        wg.Add(1)
        go worker(w + 1, jobs, results, wg)
        w = w + 1
    }

    // Send jobs
    mut let j = 0
    while j < num_jobs {
        jobs.send(Job { id: j + 1, input: j + 1 })
        j = j + 1
    }
    jobs.close()

    // Wait for workers, then close results
    go fn() {
        wg.Wait()
        results.close()
    }()

    // Collect results
    mut let total = 0
    loop {
        let r = results.receive()
        if r == null {
            break
        }
        total = total + r.output
    }

    go::fmt::Println("All jobs done. Sum of squares 1..20 =", total)
    return 0
}
