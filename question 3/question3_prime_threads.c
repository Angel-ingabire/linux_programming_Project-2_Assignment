#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define START_N 1
#define END_N 200000
#define THREADS 16

typedef struct
{
    int tid;
    int start; // inclusive
    int end;   // inclusive
} thread_arg_t;

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t g_total_primes = 0;

static int is_prime(int n)
{
    if (n < 2)
        return 0;
    if (n == 2)
        return 1;
    if ((n & 1) == 0)
        return 0;

    // trial division up to i*i <= n (integer math)
    for (int i = 3; (int64_t)i * (int64_t)i <= (int64_t)n; i += 2)
    {
        if (n % i == 0)
            return 0;
    }
    return 1;
}

static void *worker(void *arg)
{
    thread_arg_t *a = (thread_arg_t *)arg;
    uint64_t local_count = 0;

    for (int x = a->start; x <= a->end; ++x)
    {
        if (is_prime(x))
            local_count++;
    }

    pthread_mutex_lock(&g_mutex);
    g_total_primes += local_count;
    pthread_mutex_unlock(&g_mutex);

    return NULL;
}

int main(void)
{
    pthread_t threads[THREADS];
    thread_arg_t args[THREADS];

    const int total = END_N - START_N + 1;
    const int base = total / THREADS;
    const int rem = total % THREADS;

    int cur = START_N;
    for (int t = 0; t < THREADS; ++t)
    {
        int seg = base + (t < rem ? 1 : 0);
        args[t].tid = t;
        args[t].start = cur;
        args[t].end = cur + seg - 1;
        cur += seg;
    }

    for (int t = 0; t < THREADS; ++t)
    {
        if (pthread_create(&threads[t], NULL, worker, &args[t]) != 0)
        {
            perror("pthread_create");
            return EXIT_FAILURE;
        }
    }

    for (int t = 0; t < THREADS; ++t)
    {
        pthread_join(threads[t], NULL);
    }

    printf("The synchronized total number of prime numbers between 1 and 200,000 is %llu\n",
           (unsigned long long)g_total_primes);
    return EXIT_SUCCESS;
}
