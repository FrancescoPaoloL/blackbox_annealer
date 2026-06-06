#ifndef GUARDIAN_H
#define GUARDIAN_H

/*
 * Thin wrapper around guardian.py subprocess.
 * Protocol: C writes one text line to stdin, reads one "<score> <verdict>" line from stdout.
 * The subprocess stays alive for the entire run.
 */

#define GUARDIAN_MAX_TEXT 4096

/* Start the guardian subprocess. Returns 0 on success, -1 on error. */
int guardian_init(const char *script_path, const char *server_url, double threshold);

/* Send text to guardian. Returns score in [0.0, 1.0], -1.0 on error. */
double guardian_score(const char *text);

/* Terminate the subprocess and close pipes. */
void guardian_free(void);

#endif

