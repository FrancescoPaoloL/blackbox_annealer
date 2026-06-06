#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "energy.h"
#include "http.h"

#define MAX_DIM 4096

/*
 * Parse llama.cpp /embedding response format:
 *   [{"index":0,"embedding":[[v1, v2, ...]]}]
 */
static double *parse_embedding(const char *json, int *out_dim)
{
    const char *p;
    double *vec;
    int dim = 0;

    *out_dim = 0;

    p = strstr(json, "\"embedding\"");
    if (!p) { fprintf(stderr, "[energy] no 'embedding' key\n"); return NULL; }

    p = strchr(p, '[');
    if (!p) return NULL;
    p++;

    while (*p == ' ' || *p == '\n') p++;
    if (*p == '[') p++;

    vec = malloc(MAX_DIM * sizeof(double));
    if (!vec) return NULL;

    while (*p && *p != ']' && dim < MAX_DIM) {
        char *end;
        double v = strtod(p, &end);
        if (end == p) break;
        vec[dim++] = v;
        p = end;
        while (*p == ',' || *p == ' ' || *p == '\n') p++;
    }

    if (dim == 0) { free(vec); return NULL; }
    *out_dim = dim;
    return vec;
}

static double cosine(const double *a, const double *b, int dim)
{
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (int i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        na  += a[i] * a[i];
        nb  += b[i] * b[i];
    }
    if (na == 0.0 || nb == 0.0) return 0.0;
    return dot / (sqrt(na) * sqrt(nb));
}

static double *target_vec = NULL;
static int     target_dim = 0;

int energy_init(const char *server_url, const char *target)
{
    char buf[HTTP_MAX_RESPONSE];

    if (http_embedding(server_url, target, buf, sizeof(buf)) != 0) {
        fprintf(stderr, "[energy] failed to fetch target embedding\n");
        return -1;
    }

    target_vec = parse_embedding(buf, &target_dim);
    if (!target_vec) {
        fprintf(stderr, "[energy] failed to parse target embedding\n");
        return -1;
    }

    fprintf(stderr, "[energy] target embedding loaded, dim=%d\n", target_dim);
    return 0;
}

double energy_compute(const char *server_url, const char *model_output)
{
    char buf[HTTP_MAX_RESPONSE];
    double *vec;
    int dim;

    if (!target_vec) { fprintf(stderr, "[energy] not initialised\n"); return -1.0; }

    if (http_embedding(server_url, model_output, buf, sizeof(buf)) != 0)
        return -1.0;

    vec = parse_embedding(buf, &dim);
    if (!vec) return -1.0;

    if (dim != target_dim) {
        fprintf(stderr, "[energy] dim mismatch: %d vs %d\n", dim, target_dim);
        free(vec);
        return -1.0;
    }

    double e = 1.0 - cosine(vec, target_vec, dim);
    free(vec);
    return e;
}

void energy_free(void)
{
    free(target_vec);
    target_vec = NULL;
    target_dim = 0;
}

