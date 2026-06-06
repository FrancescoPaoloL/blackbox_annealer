#ifndef MUTATOR_H
#define MUTATOR_H

/*
 * Thin wrapper around mutator.py subprocess.
 * Protocol: C writes one prompt line to stdin, reads one mutated line from stdout.
 * The subprocess stays alive for the entire run.
 */

#define MUTATOR_MAX_PROMPT 4096

/* Start the mutator subprocess. Returns 0 on success, -1 on error. */
int mutator_init(const char *script_path, int seed, const char *seed_file);

/* Mutate a prompt. out_buf must be at least MUTATOR_MAX_PROMPT bytes. */
int mutator_run(const char *prompt, char *out_buf, size_t out_size);

/* Terminate the subprocess and close pipes. */
void mutator_free(void);

#endif

