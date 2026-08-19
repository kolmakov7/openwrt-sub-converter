#include "httpc.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

typedef struct body_buf {
    char *data;
    size_t len;
    size_t cap;
} body_buf;

typedef struct header_buf {
    httpc_header *arr;
    size_t n;
    size_t cap;
} header_buf;

static size_t body_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    body_buf *buf = (body_buf *)userdata;
    size_t n = size * nmemb;
    size_t ncap;
    char *ndata;

    if (n == 0) {
        return 0;
    }
    if (buf->len + n + 1 > buf->cap) {
        ncap = buf->cap ? buf->cap : 1024;
        while (ncap < buf->len + n + 1) {
            ncap *= 2;
        }
        ndata = (char *)realloc(buf->data, ncap);
        if (!ndata) {
            return 0;
        }
        buf->data = ndata;
        buf->cap = ncap;
    }
    memcpy(buf->data + buf->len, ptr, n);
    buf->len += n;
    buf->data[buf->len] = '\0';
    return n;
}

static char *dup_trimmed(const char *s, size_t n)
{
    const char *start = s;
    const char *end = s + n;
    size_t len;
    char *out;

    while (start < end && isspace((unsigned char)*start)) {
        start++;
    }
    while (end > start && isspace((unsigned char)end[-1])) {
        end--;
    }
    len = (size_t)(end - start);
    out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static size_t header_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    header_buf *buf = (header_buf *)userdata;
    size_t n = size * nmemb;
    char *line;
    char *end;
    char *colon;
    httpc_header *h;
    size_t ncap;

    if (n == 0) {
        return 0;
    }
    line = (char *)malloc(n + 1);
    if (!line) {
        return 0;
    }
    memcpy(line, ptr, n);
    line[n] = '\0';
    end = line + n;
    while (end > line && (end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    *end = '\0';
    colon = strchr(line, ':');
    if (!colon) {
        free(line);
        return n;
    }
    if (buf->n + 1 > buf->cap) {
        ncap = buf->cap ? buf->cap : 8;
        while (ncap < buf->n + 1) {
            ncap *= 2;
        }
        h = (httpc_header *)realloc(buf->arr, ncap * sizeof(*h));
        if (!h) {
            free(line);
            return 0;
        }
        buf->arr = h;
        buf->cap = ncap;
    }
    h = &buf->arr[buf->n];
    h->name = dup_trimmed(line, (size_t)(colon - line));
    h->value = dup_trimmed(colon + 1, (size_t)(end - colon - 1));
    if (!h->name || !h->value) {
        free(h->name);
        free(h->value);
        free(line);
        return 0;
    }
    buf->n++;
    free(line);
    return n;
}

int httpc_request(const httpc_req *req, httpc_response **out)
{
    static int g_init;
    CURL *curl;
    struct curl_slist *hdr_list = NULL;
    httpc_response *resp;
    body_buf body;
    header_buf hdrs;
    const char *method;
    size_t i;
    CURLcode cc;
    long status = 0;
    int rc = HTTPC_ERR;

    if (!out) {
        return HTTPC_ERR;
    }
    resp = (httpc_response *)calloc(1, sizeof(*resp));
    if (!resp) {
        *out = NULL;
        return HTTPC_ERR;
    }
    *out = resp;
    if (!req || !req->url) {
        return HTTPC_ERR;
    }
    memset(&body, 0, sizeof(body));
    memset(&hdrs, 0, sizeof(hdrs));
    if (!g_init) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        g_init = 1;
    }
    curl = curl_easy_init();
    if (!curl) {
        return HTTPC_ERR;
    }
    curl_easy_setopt(curl, CURLOPT_URL, req->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, body_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_write_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &hdrs);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "sub-converter/0.1");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    if (req->timeout_ms > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, req->timeout_ms);
    }
    if (req->max_response > 0) {
        curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)req->max_response);
    }
    if (req->headers && req->nheaders > 0) {
        for (i = 0; i < req->nheaders; i++) {
            hdr_list = curl_slist_append(hdr_list, req->headers[i]);
            if (!hdr_list) {
                break;
            }
        }
        if (hdr_list) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr_list);
        }
    }
    method = req->method;
    if (!method || !*method) {
        method = "GET";
    }
    if (strcmp(method, "POST") == 0) {
        const char *post_data = req->body;
        size_t post_len = req->body_len;

        if (post_data && post_len == 0) {
            post_len = strlen(post_data);
        }
        if (post_data && post_len > 0) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)post_len);
        }
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }
    cc = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    resp->status = status;
    resp->body = body.data;
    resp->body_len = body.len;
    resp->headers = hdrs.arr;
    resp->nheaders = hdrs.n;
    if (cc == CURLE_OK) {
        rc = status >= 400 ? HTTPC_HTTP_ERROR : HTTPC_OK;
    } else if (cc == CURLE_OPERATION_TIMEDOUT) {
        rc = HTTPC_TIMEOUT;
    } else if (cc == CURLE_HTTP_RETURNED_ERROR) {
        rc = HTTPC_HTTP_ERROR;
    } else {
        rc = HTTPC_ERR;
    }
    if (hdr_list) {
        curl_slist_free_all(hdr_list);
    }
    curl_easy_cleanup(curl);
    return rc;
}

void httpc_response_free(httpc_response *resp)
{
    size_t i;

    if (!resp) {
        return;
    }
    free(resp->body);
    for (i = 0; i < resp->nheaders; i++) {
        free(resp->headers[i].name);
        free(resp->headers[i].value);
    }
    free(resp->headers);
    free(resp);
}

const char *httpc_strerror(int status)
{
    switch (status) {
    case HTTPC_OK:
        return "ok";
    case HTTPC_ERR:
        return "network error";
    case HTTPC_TIMEOUT:
        return "timeout";
    case HTTPC_HTTP_ERROR:
        return "http error";
    default:
        return "unknown";
    }
}
