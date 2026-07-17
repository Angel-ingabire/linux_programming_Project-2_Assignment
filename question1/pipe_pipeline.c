#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)

int main(void)
{
    fprintf(stderr, "This program requires a Linux/Unix environment with fork(), pipe(), and execvp().\n");
    return EXIT_FAILURE;
}

#else

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define OUTPUT_PATH "question1/output.txt"
#define MAX_READ 256

static void print_error_and_exit(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

static void display_output_file(const char *path)
{
    FILE *file = fopen(path, "r");
    char line[MAX_READ];
    int lines_shown = 0;

    if (file == NULL)
    {
        print_error_and_exit("fopen");
    }

    printf("\n--- Partial output captured from %s ---\n", path);
    while (lines_shown < 5 && fgets(line, sizeof(line), file) != NULL)
    {
        fputs(line, stdout);
        lines_shown++;
    }

    fclose(file);
}

int main(void)
{
    int pipefd[2];
    int output_fd;
    pid_t child1, child2;
    char *ps_args[] = {"ps", "aux", NULL};
    char *grep_args[] = {"grep", "root", NULL};

    if (pipe(pipefd) == -1)
    {
        print_error_and_exit("pipe");
    }

    output_fd = open(OUTPUT_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd == -1)
    {
        print_error_and_exit("open");
    }

    child1 = fork();
    if (child1 == -1)
    {
        print_error_and_exit("fork child1");
    }

    if (child1 == 0)
    {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) == -1)
        {
            print_error_and_exit("dup2 child1 stdout");
        }
        close(pipefd[1]);

        execvp("ps", ps_args);
        print_error_and_exit("execvp ps");
    }

    child2 = fork();
    if (child2 == -1)
    {
        print_error_and_exit("fork child2");
    }

    if (child2 == 0)
    {
        close(pipefd[1]);
        if (dup2(pipefd[0], STDIN_FILENO) == -1)
        {
            print_error_and_exit("dup2 child2 stdin");
        }
        close(pipefd[0]);

        if (dup2(output_fd, STDOUT_FILENO) == -1)
        {
            print_error_and_exit("dup2 child2 stdout");
        }
        close(output_fd);

        execvp("grep", grep_args);
        print_error_and_exit("execvp grep");
    }

    close(pipefd[0]);
    close(pipefd[1]);
    close(output_fd);

    waitpid(child1, NULL, 0);
    waitpid(child2, NULL, 0);

    display_output_file(OUTPUT_PATH);

    return EXIT_SUCCESS;
}

#endif
