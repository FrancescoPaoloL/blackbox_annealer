#ifndef ENERGY_H
#define ENERGY_H

/*
 * Energy = 1 - cosine(embed(model_output), embed(target))
 * Range [0.0, 2.0] — 0.0 means identical, 1.0 orthogonal, 2.0 opposite.
 *
 * Call energy_init() once at startup, then energy_compute() per step.
 */

/* Cache the target embedding. Returns 0 on success, -1 on error. */
int energy_init(const char *server_url, const char *target);

/* Compute energy for a model output. Returns value in [0.0, 2.0], -1.0 on error. */
double energy_compute(const char *server_url, const char *model_output);

/* Free cached target embedding. */
void energy_free(void);

#endif

