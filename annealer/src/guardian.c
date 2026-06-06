#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

#include "guardian.h"

static pid_t child_pid  = -1;
static FILE *to_child   = NULL;
static FILE *from_child = NULL;

int guardian_init(const char *script_path, const char *server_url, double threshold)
{
    int pipe_in[2], pipe_out[2];

    if (pipe(pipe_in) != 0 || pipe(pipe_out) != 0) {
        perror("[guardian] pipe"); return -1;
    }

    child_pid = fork();
    if (child_pid < 0) { perror("[guardian] fork"); return -1; }

    if (child_pid == 0) {
        close(pipe_in[1]);
        close(pipe_out[0]);
        dup2(pipe_in[0],  STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_in[0]);
        close(pipe_out[1]);

        char threshold_str[32];
        snprintf(threshold_str, sizeof(threshold_str), "%.3f", threshold);

        execlp("python3", "python3", script_path,
               "--server", server_url,
               "--threshold", threshold_str,
               NULL);
        perror("[guardian] execlp");
        _exit(1);
    }

    close(pipe_in[0]);
    close(pipe_out[1]);

    to_child   = fdopen(pipe_in[1],  "w");
    from_child = fdopen(pipe_out[0], "r");

    if (!to_child || !from_child) { perror("[guardian] fdopen"); return -1; }

    /* consume the "[guardian] ready" stderr line — it goes to our stderr, not stdout */
    fprintf(stderr, "[guardian] subprocess started (pid=%d)\n", child_pid);
    return 0;
}

double guardian_score(const char *text)
{
    if (!to_child || !from_child) {
        fprintf(stderr, "[guardian] not initialised\n"); return -1.0;
    }

    fprintf(to_child, "%s\n", text);
    fflush(to_child);

    char reply[256];
    if (!fgets(reply, sizeof(reply), from_child)) {
        fprintf(stderr, "[guardian] no response\n"); return -1.0;
    }

    double score;
    char verdict[32];
    if (sscanf(reply, "%lf %31s", &score, verdict) != 2) {
        fprintf(stderr, "[guardian] bad reply: %s\n", reply); return -1.0;
    }

    return score;
}

void guardian_free(void)
{
    if (to_child)   { fclose(to_child);   to_child   = NULL; }
    if (from_child) { fclose(from_child); from_child = NULL; }
    if (child_pid > 0) { kill(child_pid, SIGTERM); child_pid = -1; }
}

