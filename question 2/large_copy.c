#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)

int main(void)
{
    fprintf(stderr, "This benchmark requires a Linux/Unix environment with POSIX file I/O and timing support.\n");
    return EXIT_FAILURE;
}

#else

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define INPUT_FILE "question 2/input.bin"
#define OUTPUT_SYS "question 2/output_syscalls.bin"
#define OUTPUT_STDIO "question 2/output_stdio.bin"
#define COPY_SIZE_BYTES (100ULL * 1024ULL * 1024ULL)
#define BUFFER_SIZE 65536

static double elapsed_seconds(const struct timespec *start, const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

static int create_large_input_file(const char *path)
{
    FILE *file = fopen(path, "wb");
    unsigned char buffer[BUFFER_SIZE];
    size_t i;
    size_t blocks = COPY_SIZE_BYTES / sizeof(buffer);

    if (file == NULL)
    {
        perror("fopen create");
        return -1;
    }

    for (i = 0; i < sizeof(buffer); ++i)
    {
        buffer[i] = (unsigned char)('A' + (i % 26));
    }

    for (i = 0; i < blocks; ++i)
    {
        if (fwrite(buffer, 1, sizeof(buffer), file) != sizeof(buffer))
        {
            perror("fwrite create");
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}

static int copy_with_syscalls(const char *src, const char *dest)
{
    int input_fd = open(src, O_RDONLY);
    int output_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    unsigned char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    ssize_t bytes_written;

    if (input_fd == -1 || output_fd == -1)
    {
        perror("open");
        return -1;
    }

    while ((bytes_read = read(input_fd, buffer, sizeof(buffer))) > 0)
    {
        bytes_written = 0;
        while (bytes_written < bytes_read)
        {
            ssize_t current = write(output_fd, buffer + bytes_written, bytes_read - bytes_written);
            if (current == -1)
            {
                perror("write");
                close(input_fd);
                close(output_fd);
                return -1;
            }
            bytes_written += current;
        }
    }

    if (bytes_read == -1)
    {
        perror("read");
        close(input_fd);
        close(output_fd);
        return -1;
    }

    if (close(input_fd) == -1 || close(output_fd) == -1)
    {
        perror("close");
        return -1;
    }

    return 0;
}

static int copy_with_stdio(const char *src, const char *dest)
{
    FILE *input = fopen(src, "rb");
    FILE *output = fopen(dest, "wb");
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;

    if (input == NULL || output == NULL)
    {
        perror("fopen");
        return -1;
    }

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) > 0)
    {
        if (fwrite(buffer, 1, bytes_read, output) != bytes_read)
        {
            perror("fwrite");
            fclose(input);
            fclose(output);
            return -1;
        }
    }

    if (ferror(input))
    {
        perror("fread");
        fclose(input);
        fclose(output);
        return -1;
    }

    if (fclose(input) != 0 || fclose(output) != 0)
    {
        perror("fclose");
        return -1;
    }

    return 0;
}

int main(void)
{
    struct timespec start, end;
    double sys_time, stdio_time;

    if (create_large_input_file(INPUT_FILE) != 0)
    {
        return EXIT_FAILURE;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    if (copy_with_syscalls(INPUT_FILE, OUTPUT_SYS) != 0)
    {
        return EXIT_FAILURE;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    sys_time = elapsed_seconds(&start, &end);

    clock_gettime(CLOCK_MONOTONIC, &start);
    if (copy_with_stdio(INPUT_FILE, OUTPUT_STDIO) != 0)
    {
        return EXIT_FAILURE;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    stdio_time = elapsed_seconds(&start, &end);

    printf("Version 1 (read/write): %.6f seconds\n", sys_time);
    printf("Version 2 (fread/fwrite): %.6f seconds\n", stdio_time);

    return EXIT_SUCCESS;
}

#endif
