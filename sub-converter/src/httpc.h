#ifndef SUB_CONV_HTTPC_H
#define SUB_CONV_HTTPC_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    HTTPC_OK = 0,
    HTTPC_ERR = -1,
    HTTPC_TIMEOUT = -2,
    HTTPC_HTTP_ERROR = -3
} httpc_status;

typedef struct httpc_header {
    char *name;
    char *value;
} httpc_header;

typedef struct httpc_response {
    long status;
    char *body;
    size_t body_len;
    httpc_header *headers;
    size_t nheaders;
} httpc_response;

typedef struct httpc_req {
    const char *url;
    const char *method;
    const char *body;
    size_t body_len;
    const char *const *headers;
    size_t nheaders;
    long timeout_ms;
    long max_response;
} httpc_req;

int httpc_request(const httpc_req *req, httpc_response **out);
void httpc_response_free(httpc_response *resp);
const char *httpc_strerror(int status);

#endif
