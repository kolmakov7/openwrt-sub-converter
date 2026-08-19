#define _DEFAULT_SOURCE

#include <arpa/inet.h>
#include <microhttpd.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static char *g_sub_body = NULL;
static size_t g_sub_len = 0;
static int g_sub_missing = 0;
static char *g_example_body = NULL;
static size_t g_example_len = 0;
static int g_example_missing = 1;

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s --port N --file PATH [--jsonfile PATH] [--host H]\n", prog);
}

static int read_file_mem(const char *path, char **out_body, size_t *out_len)
{
    FILE *f;
    long sz;
    size_t n;
    char *body;

    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return -1;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    body = malloc((size_t)sz + 1);
    if (!body) {
        fclose(f);
        return -1;
    }
    n = fread(body, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) {
        free(body);
        return -1;
    }
    body[n] = '\0';
    *out_body = body;
    *out_len = n;
    return 0;
}

static enum MHD_Result queue_response(struct MHD_Connection *con, unsigned int status,
                                      const char *ctype, const char *body, size_t body_len)
{
    struct MHD_Response *resp;
    enum MHD_Result ret;

    resp = MHD_create_response_from_buffer(body_len, (void *)body, MHD_RESPMEM_PERSISTENT);
    if (!resp) {
        return MHD_NO;
    }
    MHD_add_response_header(resp, MHD_HTTP_HEADER_CONTENT_TYPE, ctype);
    ret = MHD_queue_response(con, status, resp);
    MHD_destroy_response(resp);
    return ret;
}

static enum MHD_Result handler_cb(void *cls, struct MHD_Connection *con,
                                  const char *url, const char *method,
                                  const char *version, const char *upload_data,
                                  size_t *upload_data_size, void **con_cls)
{
    const char *hdr;

    (void)cls;
    (void)version;
    (void)upload_data;
    if (!*con_cls) {
        *con_cls = (void *)1;
        return MHD_YES;
    }
    if (*upload_data_size > 0) {
        *upload_data_size = 0;
        return MHD_YES;
    }
    if (strcmp(method, MHD_HTTP_METHOD_GET) != 0) {
        return queue_response(con, MHD_HTTP_METHOD_NOT_ALLOWED, "text/plain",
                              "method not allowed", 18);
    }
    if (strcmp(url, "/sub") == 0) {
        if (g_sub_missing) {
            return queue_response(con, MHD_HTTP_INTERNAL_SERVER_ERROR, "text/plain",
                                  "missing file", 12);
        }
        return queue_response(con, MHD_HTTP_OK, "text/plain", g_sub_body, g_sub_len);
    }
    if (strcmp(url, "/example") == 0) {
        if (!g_example_missing && g_example_body) {
            return queue_response(con, MHD_HTTP_OK, "application/json", g_example_body,
                                  g_example_len);
        }
        return queue_response(con, MHD_HTTP_NOT_FOUND, "text/plain", "not found", 9);
    }
    if (strcmp(url, "/health") == 0) {
        return queue_response(con, MHD_HTTP_OK, "text/plain", "ok", 2);
    }
    if (strcmp(url, "/echo-headers") == 0) {
        hdr = MHD_lookup_connection_value(con, MHD_HEADER_KIND, "X-Sub-Forward-Test");
        if (hdr) {
            return queue_response(con, MHD_HTTP_OK, "text/plain", hdr, strlen(hdr));
        }
        return queue_response(con, MHD_HTTP_OK, "text/plain", "none", 4);
    }
    return queue_response(con, MHD_HTTP_NOT_FOUND, "text/plain", "not found", 9);
}

static struct MHD_Daemon *start_daemon(const char *host, int port)
{
    struct sockaddr_in addr;
    struct MHD_Daemon *d;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        return NULL;
    }
    d = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_POLL,
                         (uint16_t)port, NULL, NULL, handler_cb, NULL,
                         MHD_OPTION_SOCK_ADDR_LEN, (socklen_t)sizeof(addr), &addr,
                         MHD_OPTION_END);
    return d;
}

int main(int argc, char **argv)
{
    const char *host = "127.0.0.1";
    const char *file = NULL;
    const char *jsonfile = NULL;
    int port = 0;
    struct MHD_Daemon *daemon;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            file = argv[++i];
        } else if (strcmp(argv[i], "--jsonfile") == 0 && i + 1 < argc) {
            jsonfile = argv[++i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (port <= 0 || !file) {
        usage(argv[0]);
        return 2;
    }
    if (read_file_mem(file, &g_sub_body, &g_sub_len) < 0) {
        g_sub_missing = 1;
    }
    if (jsonfile) {
        if (read_file_mem(jsonfile, &g_example_body, &g_example_len) < 0) {
            g_example_missing = 1;
        } else {
            g_example_missing = 0;
        }
    }
    daemon = start_daemon(host, port);
    if (!daemon) {
        fprintf(stderr, "mock_upstream: failed to start server\n");
        return 1;
    }
    fprintf(stderr, "mock_upstream: listening on port %d\n", port);
    for (;;) {
        pause();
    }
    return 0;
}
