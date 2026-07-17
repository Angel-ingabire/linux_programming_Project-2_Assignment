This repository contains solutions for **Questions 1–4** from the project assignment.

---

## Repository Structure

- **question1/** — `fork()`, `execvp()`, `pipe()`, capture pipeline output to a file, partial display, and `strace` log review
- **question2/** — two large-file copy utilities (low-level vs stdio), timing + syscall count (via trace)
- **question 3/** — `pthreads` prime counting from **1..200000** using **16 threads** and `pthread_mutex_t`
- **question 4/** — multithreaded keyword search across multiple files using shared output file + mutex

---

## Question 1 — Pipeline with fork/exec/pipe + strace analysis

### Implemented program

- **Source:** `question1/pipe_pipeline.c`
- **Executable (built on Linux):** depends on your build command

### Behavior

1. Parent creates a **pipe**.
2. Parent **forks twice** to create **two children**.
3. Child #1 executes:
   - `ps aux`
4. Child #2 executes:
   - `grep root`
   - stdin is connected to the pipe read-end
5. The pipeline output is redirected by the parent-side setup so that **`grep` output is written to `question1/output.txt`**.
6. Parent waits for both children and then **prints the first few lines** from `question1/output.txt`.

### Build (Linux)

```bash
gcc -O2 -Wall -Wextra question1/pipe_pipeline.c -o question1/pipe_pipeline -pthread
```

### Run

```bash
./question1/pipe_pipeline
```

### strace artifacts

- **Log:** `question1/trace.log`

### What to trace (assignment requirements)

Use `strace` to observe and explain:

- `fork()` / `clone()` (process creation)
- `pipe()` / `pipe2()` (IPC channel creation)
- `dup2()` (stdout/stdin redirection)
- `open*()` / `fstat()` / `close()` (file operations)
- `execve()` (process image replacement via `execvp()`)

### System call sequence (role explanation)

From `question1/trace.log`, the key pipeline stages appear as:

1. **Dynamic loader / program startup** (many `openat`, `mmap`, `fstat`, etc.)
2. **Process creation**:
   - `clone(...SIGCHLD...)` corresponds to `fork()`-like behavior.
3. **Pipe creation**:
   - `pipe2([3, 4], 0)` establishes endpoints used for stdin/stdout connections.
4. **File open for output**:
   - `openat(... "question1/output.txt", O_WRONLY|O_CREAT|O_TRUNC, ...)`
5. **Redirection**:
   - `dup2(pipe_write, STDOUT_FILENO)` for the `ps` child
   - `dup2(pipe_read, STDIN_FILENO)` for the `grep` child
   - `dup2(output_fd, STDOUT_FILENO)` for the `grep` child
6. **Exec**:
   - multiple `execve()` attempts for `ps` and `grep` across different absolute paths (standard behavior)
7. **Parent waits**:
   - `wait4(child_pid, ...)`
8. **Output display**:
   - parent reads `question1/output.txt` (`openat`, `read`, `close`) and prints partial output.

> If your `strace` log includes `ENOENT` for certain `execve()` search paths, that is expected: libc searches common locations for the binaries.

---

## Question 2 — Large file copy: syscalls vs stdio + performance comparison

### Implementations

- **Low-level syscalls:**
  - `question2/large_copy` (and/or `question2/large_copy.c`)
- **Standard I/O version:**
  - output binaries:
    - `question2/output_stdio.bin`
- Trace summaries:
  - `question2/trace_summary.txt`

### Requirements checklist

- Copy file **≥ 100MB**
- Measure **execution time**
- Use `strace` to compare:
  - **number of system calls**
  - any observed I/O behavior differences

### Report guidance (what to analyze)

- `read/write` versions typically show more frequent syscalls when buffering is small.
- `fread/fwrite` versions usually perform fewer syscalls due to internal buffering.
- Compare:
  - throughput (time)
  - syscalls count (`strace` + filtering)
  - overhead (context switches / kernel crossings)

---

## Question 3 — Prime counting (16 threads) with mutex synchronization

### Source

- `question 3/question3_prime_threads.c`

### Build

```bash
gcc -O2 -pthread "question 3/question3_prime_threads.c" -o "question 3/question3_primes"
```

### Run

```bash
./question 3/question3_primes
```

### Logic overview

- Range: **1..200000**
- Workload split into **16 contiguous segments** (equal partition with remainder distributed to first threads)
- Each thread:
  - counts primes in its own segment via trial division
  - uses a local counter
- Shared result:
  - `uint64_t g_total_primes`
  - updated once per thread under `pthread_mutex_t g_mutex`

### Expected output format

```text
The synchronized total number of prime numbers between 1 and 200,000 is <total>
```

---

## Question 4 — Multithreaded keyword search across multiple files

### Source

- `question 4/search_keyword.c`

### Build

```bash
gcc -O2 -pthread "question 4/search_keyword.c" -o "question 4/search_keyword"
```

### Run

```bash
./question 4/search_keyword keyword output.txt file1.txt file2.txt ... <number_of_threads>
```

### Intended behavior (assignment requirements)

- **Threading model**: multiple POSIX threads
- **Work distribution**:
  - threads perform **round-robin over all input files**
  - for thread `t`: `t, t+num_threads, t+2*num_threads, ...`
- **Counting**:
  - counts occurrences of `keyword` within each file
  - supports overlaps using a tail buffer across `fread()` chunk boundaries
- **Synchronization**:
  - shared output file writes are protected by `pthread_mutex_t g_write_mutex`

### Output format

For each file processed:

```text
/path/to/file: <occurrence_count>
```

### Notes on correctness

- Overlap handling is performed by keeping the last `(klen-1)` bytes from the previous chunk in a `tail` buffer, then concatenating `tail + next_chunk` into a `combined` buffer for comparisons.

---

## How to Reproduce Builds Quickly (Linux)

### Q1

```bash
gcc -O2 -Wall -Wextra question1/pipe_pipeline.c -o question1/pipe_pipeline
./question1/pipe_pipeline
```

### Q2

```bash
# build commands depend on your provided make/build setup
```

### Q3

```bash
gcc -O2 -pthread "question 3/question3_prime_threads.c" -o "question 3/question3_primes"
./"question 3/question3_primes"
```

### Q4

```bash
gcc -O2 -pthread "question 4/search_keyword.c" -o "question 4/search_keyword"
./"question 4/search_keyword" keyword output.txt file1.txt file2.txt 4
```

---
