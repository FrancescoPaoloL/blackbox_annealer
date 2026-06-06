#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "http.h"

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} write_buf_t;

static size_t write_cb(void *data, size_t size, size_t nmemb, void *userp)
{
    size_t real = size * nmemb;
    write_buf_t *wb = (write_buf_t *)userp;
    if (wb->len + real + 1 > wb->cap) return 0;
    memcpy(wb->buf + wb->len, data, real);
    wb->len += real;
    wb->buf[wb->len] = '\0';
    return real;
}

/* Escape string for JSON — handles quotes, backslashes, newlines, tabs. */
static size_t json_escape(const char *in, char *out, size_t out_size)
{
    size_t i = 0;
    while (*in && i + 2 < out_size) {
        unsigned char c = (unsigned char)*in++;
        if      (c == '"')  { out[i++] = '\\'; out[i++] = '"';  }
        else if (c == '\\') { out[i++] = '\\'; out[i++] = '\\'; }
        else if (c == '\n') { out[i++] = '\\'; out[i++] = 'n';  }
        else if (c == '\r') { out[i++] = '\\'; out[i++] = 'r';  }
        else if (c == '\t') { out[i++] = '\\'; out[i++] = 't';  }
        else if (c < 0x20)  { }
        else                { out[i++] = c;                      }
    }
    out[i] = '\0';
    return i;
}

static int do_post(const char *url, const char *body,
                   char *out_buf, size_t out_size)
{
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    write_buf_t wb = { out_buf, 0, out_size };
    int ret = -1;

    if (out_size > 0) out_buf[0] = '\0';

    curl = curl_easy_init();
    if (!curl) return -1;

    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL,           url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &wb);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       (long)HTTP_TIMEOUT_SEC);

    res = curl_easy_perform(curl);
    if (res == CURLE_OK)
        ret = 0;
    else
        fprintf(stderr, "[http] curl error: %s\n", curl_easy_strerror(res));

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return ret;
}

int http_completion(const char *server_url, const char *prompt,
                    float temperature, int n_predict,
                    char *out_buf, size_t out_size)
{
    char url[512], escaped[16384], body[32768];
    snprintf(url, sizeof(url), "%s/completion", server_url);
    json_escape(prompt, escaped, sizeof(escaped));
    snprintf(body, sizeof(body),
             "{\"prompt\":\"%s\",\"temperature\":%.4f,\"n_predict\":%d,\"stream\":false}",
             escaped, temperature, n_predict);
    return do_post(url, body, out_buf, out_size);
}

int http_embedding(const char *server_url, const char *text,
                   char *out_buf, size_t out_size)
{
    char url[512], escaped[8192], body[16384];
    snprintf(url, sizeof(url), "%s/embedding", server_url);
    json_escape(text, escaped, sizeof(escaped));
    snprintf(body, sizeof(body), "{\"content\":\"%s\"}", escaped);
    return do_post(url, body, out_buf, out_size);
}

