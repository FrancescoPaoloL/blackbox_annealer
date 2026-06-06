#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

#include "mutator.h"

static pid_t child_pid  = -1;
static FILE *to_child   = NULL;
static FILE *from_child = NULL;

int mutator_init(const char *script_path, int seed, const char *seed_file)
{
    int pipe_in[2], pipe_out[2];

    if (pipe(pipe_in) != 0 || pipe(pipe_out) != 0) {
        perror("[mutator] pipe"); return -1;
    }

    child_pid = fork();
    if (child_pid < 0) { perror("[mutator] fork"); return -1; }

    if (child_pid == 0) {
        close(pipe_in[1]);
        close(pipe_out[0]);
        dup2(pipe_in[0],  STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_in[0]);
        close(pipe_out[1]);

        char seed_str[32];
        snprintf(seed_str, sizeof(seed_str), "%d", seed);
        execlp("python3", "python3", script_path, "--seed", seed_str, "--seed-file", seed_file, NULL);
        perror("[mutator] execlp");
        _exit(1);
    }

    close(pipe_in[0]);
    close(pipe_out[1]);

    to_child   = fdopen(pipe_in[1],  "w");
    from_child = fdopen(pipe_out[0], "r");

    if (!to_child || !from_child) { perror("[mutator] fdopen"); return -1; }

    fprintf(stderr, "[mutator] subprocess started (pid=%d, seed=%d)\n", child_pid, seed);
    return 0;
}

int mutator_run(const char *prompt, char *out_buf, size_t out_size)
{
    if (!to_child || !from_child) {
        fprintf(stderr, "[mutator] not initialised\n"); return -1;
    }

    fprintf(to_child, "%s\n", prompt);
    fflush(to_child);

    if (!fgets(out_buf, (int)out_size, from_child)) {
        fprintf(stderr, "[mutator] no response from subprocess\n"); return -1;
    }

    size_t len = strlen(out_buf);
    if (len > 0 && out_buf[len - 1] == '\n') out_buf[len - 1] = '\0';

    return 0;
}

void mutator_free(void)
{
    if (to_child)   { fclose(to_child);   to_child   = NULL; }
    if (from_child) { fclose(from_child); from_child = NULL; }
    if (child_pid > 0) { kill(child_pid, SIGTERM); child_pid = -1; }
}

