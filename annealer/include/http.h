#ifndef HTTP_H
#define HTTP_H

/*
 * Minimal HTTP client for llama.cpp server.
 * Two endpoints: /completion and /embedding.
 */

#define HTTP_MAX_RESPONSE 2097152  /* 2MB — MiniLM embedding responses are large */
#define HTTP_TIMEOUT_SEC  30

/* Send prompt to /completion. Returns 0 on success, -1 on error. */
int http_completion(const char *server_url,
                    const char *prompt,
                    float temperature,
                    int n_predict,
                    char *out_buf,
                    size_t out_size);

/* Send text to /embedding. Returns 0 on success, -1 on error. */
int http_embedding(const char *server_url,
                   const char *text,
                   char *out_buf,
                   size_t out_size);

#endif

