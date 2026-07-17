#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

typedef struct
{
    int tid;
    int file_index; // starting file index for this thread (round-robin)
} worker_arg_t;

static pthread_mutex_t g_write_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    const char *keyword;
    size_t klen;
    FILE *out;
    char **files;
    int num_files;
} shared_t;

static uint64_t count_occurrences_in_file(const char *path, const char *keyword, size_t klen)
{
    if (klen == 0)
        return 0;

    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;

    const size_t CHUNK = 1u << 16; // 64KB
    uint8_t *buf = (uint8_t *)malloc(CHUNK);
    uint8_t *tail = (uint8_t *)malloc((klen > 0 ? (klen - 1) : 0));

    uint64_t count = 0;
    size_t tail_len = 0;

    if (!buf || (!tail && klen > 1))
    {
        free(buf);
        free(tail);
        fclose(f);
        return 0;
    }

    while (1)
    {
        size_t nread = fread(buf, 1, CHUNK, f);
        if (nread == 0)
            break;

        size_t combined_len = tail_len + nread;

        // Correctness-first: build combined buffer with overlap bytes.
        uint8_t *combined = (uint8_t *)malloc(combined_len);
        if (!combined)
            break;

        if (tail_len > 0)
            memcpy(combined, tail, tail_len);
        memcpy(combined + tail_len, buf, nread);

        for (size_t i = 0; i + klen <= combined_len; ++i)
        {
            if (memcmp(combined + i, keyword, klen) == 0)
                count++;
        }

        free(combined);

        // Update tail for next chunk.
        tail_len = 0;
        if (klen > 1)
        {
            size_t new_tail = (nread < (klen - 1)) ? nread : (klen - 1);
            if (new_tail > 0)
            {
                memcpy(tail, buf + (nread - new_tail), new_tail);
                tail_len = new_tail;
            }
        }
    }

    free(tail);
    free(buf);
    fclose(f);
    return count;
}

typedef struct
{
    worker_arg_t wa;
    shared_t sh;
} thread_ctx_t;

static void *thread_func(void *arg)
{
    thread_ctx_t *ctx = (thread_ctx_t *)arg;

    // Round-robin over files assigned to this thread.
    // Thread t processes: t, t + num_threads, t + 2*num_threads, ...
    for (int idx = ctx->wa.file_index; idx < ctx->sh.num_files; idx += ctx->wa.tid ? ctx->wa.tid : 1)
    {

        const char *path = ctx->sh.files[idx];
        uint64_t occurrences = count_occurrences_in_file(path, ctx->sh.keyword, ctx->sh.klen);

        pthread_mutex_lock(&g_write_mutex);
        fprintf(ctx->sh.out, "%s: %llu\n", path, (unsigned long long)occurrences);
        fflush(ctx->sh.out);
        pthread_mutex_unlock(&g_write_mutex);
    }

    return NULL;
}

static void die_usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s keyword output.txt file1.txt file2.txt ... <number_of_threads>\n",
            prog);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    if (argc < 5)
        die_usage(argv[0]);

    const char *keyword = argv[1];
    const char *output_path = argv[2];
    size_t klen = strlen(keyword);

    int num_threads = atoi(argv[argc - 1]);
    if (num_threads <= 0)
        num_threads = 1;

    int num_files = argc - 3 - 1;
    if (num_files <= 0)
        die_usage(argv[0]);

    char **files = &argv[3];
    if (num_threads > num_files)
        num_threads = num_files;

    FILE *out = fopen(output_path, "w");
    if (!out)
    {
        perror("fopen output");
        return EXIT_FAILURE;
    }

    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * (size_t)num_threads);
    thread_ctx_t *ctxs = (thread_ctx_t *)malloc(sizeof(thread_ctx_t) * (size_t)num_threads);
    if (!threads || !ctxs)
    {
        perror("malloc");
        fclose(out);
        free(threads);
        free(ctxs);
        return EXIT_FAILURE;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int t = 0; t < num_threads; ++t)
    {
        ctxs[t].wa.tid = t;
        ctxs[t].wa.file_index = t; // starting index for this thread (round-robin)
        ctxs[t].sh.keyword = keyword;
        ctxs[t].sh.klen = klen;
        ctxs[t].sh.out = out;
        ctxs[t].sh.files = files;
        ctxs[t].sh.num_files = num_files;

        if (pthread_create(&threads[t], NULL, thread_func, &ctxs[t]) != 0)
        {
            perror("pthread_create");
            fclose(out);
            free(threads);
            free(ctxs);
            return EXIT_FAILURE;
        }
    }

    for (int t = 0; t < num_threads; ++t)
        pthread_join(threads[t], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (double)(end.tv_sec - start.tv_sec) +
                     (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;

    fclose(out);
    free(threads);
    free(ctxs);

    printf("Time: %.6f seconds using %d threads\n", elapsed, num_threads);
    return 0;
}
