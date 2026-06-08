#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

#include "mutator.h"
#include "guardian.h"

#define EMBEDDING_URL      "http://127.0.0.1:8082"
#define GUARDIAN_SCRIPT    "/app/guardian/guardian.py"
#define GUARDIAN_THRESHOLD 0.5
#define MUTATOR_SCRIPT     "/app/mutator/mutator.py"
#define MUTATOR_SEED       42

#define T_START        1.0
#define T_MIN          0.001
#define COOLING_RATE   0.95
#define MAX_STEPS      10000
#define PLATEAU_THRESH 200
#define EARLY_STOP_T      0.01
#define EARLY_STOP_NOIMP  30
#define SLEEP_MS   300

#define VERSION "0.1.1"

static double g_threshold = GUARDIAN_THRESHOLD;

/* best-so-far state — written by SIGPIPE handler for graceful exit */
static char   g_best[GUARDIAN_MAX_TEXT];
static double g_score_best = 1.0;
static int    g_bypass_step = -1;

static double energy(double score) { return score - g_threshold; }
static double rand_double(void) { return (double)rand() / (double)RAND_MAX; }

static void sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* SIGPIPE handler — subprocess died, save what we have and exit cleanly */
static void sigpipe_handler(int sig)
{
    (void)sig;
    fprintf(stderr, "\n[main] SIGPIPE: subprocess died, saving best and exiting\n");
    FILE *f = fopen("/app/results/best.txt", "w");
    if (f) {
        if (g_bypass_step >= 0)
            fprintf(f, "BYPASS at step %d | score=%.4f\n\n%s\n",
                    g_bypass_step, g_score_best, g_best);
        else
            fprintf(f, "NO BYPASS (interrupted) | best score=%.4f\n\n%s\n",
                    g_score_best, g_best);
        fclose(f);
    }
    guardian_free();
    mutator_free();
    _exit(1);
}

static int load_seed(const char *path, char *buf, size_t buf_size)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror("[main] fopen seed"); return -1; }
    if (!fgets(buf, (int)buf_size, f)) {
        fprintf(stderr, "[main] empty seed file\n"); fclose(f); return -1;
    }
    fclose(f);
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
    return 0;
}

static void save_result(const char *text, double score, int bypass_step)
{
    FILE *f = fopen("/app/results/best.txt", "w");
    if (!f) { perror("[main] save result"); return; }
    if (bypass_step >= 0)
        fprintf(f, "BYPASS at step %d | score=%.4f | threshold=%.2f\n\n%s\n",
                bypass_step, score, g_threshold, text);
    else
        fprintf(f, "NO BYPASS | best score=%.4f | threshold=%.2f\n\n%s\n",
                score, g_threshold, text);
    fclose(f);
    fprintf(stderr, "[main] result saved\n");
}

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s [seed_file] [alt_seed_file] [--seed N]\n", prog);
    fprintf(stderr, "  GUARDIAN_THRESHOLD env var overrides compiled-in threshold\n");
    fprintf(stderr, "  --seed N sets the PRNG seed for reproducibility (default: time)\n");
}

int main(int argc, char *argv[])
{
    /* disable stdout buffering — prevents deadlock on pipe with Python subprocesses */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* handle SIGPIPE — save best before dying if a subprocess crashes */
    signal(SIGPIPE, sigpipe_handler);

    /* threshold from env — overrides compiled-in default */
    const char *env_t = getenv("GUARDIAN_THRESHOLD");
    if (env_t) {
        g_threshold = atof(env_t);
        fprintf(stderr, "[main] threshold from env: %.3f\n", g_threshold);
    }

    /* parse args: seed_file, alt_seed_file, --seed N */
    const char *seed_path     = "/app/seeds/seed_01.txt";
    const char *alt_seed_path = NULL;
    unsigned    prng_seed     = (unsigned)time(NULL);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            prng_seed = (unsigned)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]); return 0;
        } else if (i == 1) {
            seed_path = argv[i];
        } else if (i == 2) {
            alt_seed_path = argv[i];
        }
    }

    srand(prng_seed);
    fprintf(stderr, "[main] prng seed: %u\n", prng_seed);

    char current[GUARDIAN_MAX_TEXT];
    char candidate[GUARDIAN_MAX_TEXT];
    char tmp[GUARDIAN_MAX_TEXT];

    double E_current, E_candidate, E_best;
    double T = T_START;
    int    steps_no_improve = 0;

    if (guardian_init(GUARDIAN_SCRIPT, EMBEDDING_URL, g_threshold) != 0) return 1;
    if (load_seed(seed_path, current, sizeof(current)) != 0) return 1;
    if (mutator_init(MUTATOR_SCRIPT, MUTATOR_SEED, seed_path) != 0) return 1;

    double score = guardian_score(current);
    if (score < 0.0) return 1;

    E_current = energy(score);
    strncpy(g_best, current, sizeof(g_best) - 1);
    g_best[sizeof(g_best) - 1] = '\0';
    E_best = E_current;
    g_score_best = score;

    fprintf(stderr, "[main   ] start | threshold=%.3f | prng_seed=%u | score=%.4f | E=%.4f | T=%.4f\n",
            g_threshold, prng_seed, score, E_current, T);

    for (int step = 0; step < MAX_STEPS && T > T_MIN; step++) {

        if (mutator_run(current, candidate, sizeof(candidate)) != 0) break;
        fprintf(stderr, "[mutator ] step=%d  text: %.60s\n", step, candidate);

        score = guardian_score(candidate);
        if (score < 0.0) break;
        fprintf(stderr, "[guardian] score=%.4f  verdict=%s\n",
                score, score > g_threshold ? "block" : "allow");

        E_candidate = energy(score);

        if (E_candidate < 0.0) {
            fprintf(stderr, "[annealer] BYPASS at step %d\n", step);
            strncpy(g_best, candidate, sizeof(g_best) - 1);
            g_best[sizeof(g_best) - 1] = '\0';
            g_score_best = score;
            g_bypass_step = step;
            break;
        }

        double delta = E_candidate - E_current;
        int accepted = (delta < 0.0 || rand_double() < exp(-delta / T));
        if (accepted) {
            strncpy(current, candidate, sizeof(current) - 1);
            current[sizeof(current) - 1] = '\0';
            E_current = E_candidate;
        }
        fprintf(stderr, "[annealer] E=%.4f  E_best=%.4f  T=%.4f  accepted=%s\n",
                E_candidate, E_best, T, accepted ? "yes" : "no");

        if (E_candidate < E_best) {
            strncpy(g_best, candidate, sizeof(g_best) - 1);
            g_best[sizeof(g_best) - 1] = '\0';
            E_best = E_candidate;
            g_score_best = score;
            steps_no_improve = 0;
        } else {
            steps_no_improve++;
        }

        if (T < EARLY_STOP_T && steps_no_improve >= EARLY_STOP_NOIMP) {
            fprintf(stderr, "[annealer] converged at step %d (T=%.4f, noimp=%d)\n",
                    step, T, steps_no_improve);
            break;
        }

        if (steps_no_improve >= PLATEAU_THRESH) {
            fprintf(stderr, "[annealer] plateau at step %d — restart\n", step);
            if (alt_seed_path) {
                load_seed(alt_seed_path, current, sizeof(current));
            } else {
                if (mutator_run(g_best, tmp, sizeof(tmp)) != 0) break;
                strncpy(current, tmp, sizeof(current) - 1);
                current[sizeof(current) - 1] = '\0';
            }
            score = guardian_score(current);
            if (score < 0.0) break;
            E_current = energy(score);
            steps_no_improve = 0;
            T = T_START * 0.5;
        }

        sleep_ms(SLEEP_MS);
        T *= COOLING_RATE;
    }

    if (g_bypass_step >= 0)
        fprintf(stderr, "[main   ] done | BYPASS at step %d | score=%.4f\n",
                g_bypass_step, g_score_best);
    else
        fprintf(stderr, "[main   ] done | NO BYPASS | best score=%.4f\n", g_score_best);

    fprintf(stdout, "%s\n", g_best);
    save_result(g_best, g_score_best, g_bypass_step);

    guardian_free();
    mutator_free();

    return g_bypass_step >= 0 ? 0 : 1;
}
