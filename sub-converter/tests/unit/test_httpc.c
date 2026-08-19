#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "unity.h"
#include "httpc.h"

static int g_listen_fd = -1;
static uint16_t g_port = 0;
static pthread_t g_thread;
static volatile int g_shutdown = 0;

static char g_big[1048576];

static void send_all(int cfd, const void *data, size_t n)
{
    const char *p = (const char *)data;
    size_t off = 0;

    while (off < n) {
        ssize_t w = send(cfd, p + off, n - off, 0);

        if (w <= 0) {
            break;
        }
        off += (size_t)w;
    }
}

static void respond(int cfd, const char *status_line, const char *ctype,
                    const char *body, size_t body_len)
{
    char hdr[512];
    int hn;

    hn = snprintf(hdr, sizeof(hdr),
                  "HTTP/1.1 %s\r\nContent-Length: %zu\r\nContent-Type: %s\r\n\r\n",
                  status_line, body_len, ctype ? ctype : "text/plain");
    send_all(cfd, hdr, (size_t)hn);
    if (body_len > 0) {
        send_all(cfd, body, body_len);
    }
}

static void handle_conn(int cfd)
{
    char buf[8192];
    size_t len = 0;
    char path[1024] = "";
    ssize_t n;

    while (len < sizeof(buf) - 1) {
        n = recv(cfd, buf + len, sizeof(buf) - 1 - len, 0);

        if (n <= 0) {
            break;
        }
        len += (size_t)n;
        buf[len] = '\0';
        if (strstr(buf, "\r\n\r\n")) {
            break;
        }
    }
    buf[len] = '\0';
    sscanf(buf, "%*s %1023s", path);
    if (strcmp(path, "/ok") == 0) {
        respond(cfd, "200 OK", "text/plain", "hello world", 11);
    } else if (strcmp(path, "/notfound") == 0) {
        respond(cfd, "404 Not Found", "text/plain", "nope", 4);
    } else if (strcmp(path, "/headers") == 0) {
        const char *start = strstr(buf, "X-Echo: ");
        const char *end;

        if (start) {
            start += strlen("X-Echo: ");
            end = strstr(start, "\r\n");
            respond(cfd, "200 OK", "text/plain", start,
                    end ? (size_t)(end - start) : strlen(start));
        } else {
            respond(cfd, "200 OK", "text/plain", "", 0);
        }
    } else if (strcmp(path, "/big") == 0) {
        memset(g_big, 'A', sizeof(g_big));
        respond(cfd, "200 OK", "application/octet-stream", g_big, sizeof(g_big));
    } else if (strcmp(path, "/slow") == 0) {
        struct timespec ts;

        ts.tv_sec = 3;
        ts.tv_nsec = 0;
        nanosleep(&ts, NULL);
        respond(cfd, "200 OK", "text/plain", "late", 4);
    } else if (strcmp(path, "/redirect") == 0) {
        static const char resp[] =
            "HTTP/1.1 302 Found\r\nLocation: /ok\r\nContent-Length: 0\r\n\r\n";

        send_all(cfd, resp, sizeof(resp) - 1);
    } else {
        respond(cfd, "404 Not Found", "text/plain", "nope", 4);
    }
    close(cfd);
}

static void *server_loop(void *arg)
{
    (void)arg;
    while (!g_shutdown) {
        struct pollfd pfd;
        int pr;

        pfd.fd = g_listen_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        pr = poll(&pfd, 1, 200);
        if (pr <= 0) {
            continue;
        }
        if (g_listen_fd >= 0) {
            struct sockaddr_in addr;
            socklen_t alen = sizeof(addr);
            int cfd;

            cfd = accept(g_listen_fd, (struct sockaddr *)&addr, &alen);
            if (cfd >= 0) {
                handle_conn(cfd);
            }
        }
    }
    return NULL;
}

static int server_start(void)
{
    struct sockaddr_in addr;
    socklen_t alen = sizeof(addr);
    int fd;
    int one = 1;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 8) < 0) {
        close(fd);
        return -1;
    }
    if (getsockname(fd, (struct sockaddr *)&addr, &alen) < 0) {
        close(fd);
        return -1;
    }
    g_listen_fd = fd;
    g_port = ntohs(addr.sin_port);
    return 0;
}

static void make_url(char *buf, size_t n, const char *path)
{
    snprintf(buf, n, "http://127.0.0.1:%u%s", (unsigned)g_port, path);
}

static void test_get_ok(void)
{
    char url[256];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/ok");
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_OK, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(200, (int)resp->status);
    TEST_ASSERT_EQUAL_INT(11, (int)resp->body_len);
    TEST_ASSERT_EQUAL_STRING("hello world", resp->body);
    httpc_response_free(resp);
}

static void test_get_notfound(void)
{
    char url[256];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/notfound");
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_HTTP_ERROR, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(404, (int)resp->status);
    TEST_ASSERT_EQUAL_STRING("nope", resp->body);
    httpc_response_free(resp);
}

static void test_get_headers(void)
{
    char url[256];
    httpc_req req;
    httpc_response *resp;
    const char *headers[] = { "X-Echo: hello-header" };
    int rc;

    make_url(url, sizeof(url), "/headers");
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.headers = headers;
    req.nheaders = 1;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_OK, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_STRING("hello-header", resp->body);
    httpc_response_free(resp);
}

static void test_get_redirect(void)
{
    char url[256];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/redirect");
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_OK, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(200, (int)resp->status);
    TEST_ASSERT_EQUAL_STRING("hello world", resp->body);
    httpc_response_free(resp);
}

static void test_post_ok(void)
{
    char url[256];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/ok");
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.method = "POST";
    req.body = "xyz";
    req.body_len = 0;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_OK, rc);
    TEST_ASSERT_NOT_NULL(resp);
    httpc_response_free(resp);
}

static void test_get_big(void)
{
    char url[256];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/big");
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_OK, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(1048576, (int)resp->body_len);
    httpc_response_free(resp);
}

static void test_get_big_limited(void)
{
    char url[256];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/big");
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.max_response = 100000;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_TRUE(rc != HTTPC_OK);
    TEST_ASSERT_NOT_NULL(resp);
    httpc_response_free(resp);
}

static void test_get_slow(void)
{
    char url[256];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/slow");
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.timeout_ms = 500;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_TIMEOUT, rc);
    TEST_ASSERT_NOT_NULL(resp);
    httpc_response_free(resp);
}

static void test_strerror(void)
{
    TEST_ASSERT_EQUAL_STRING("ok", httpc_strerror(HTTPC_OK));
    TEST_ASSERT_EQUAL_STRING("network error", httpc_strerror(HTTPC_ERR));
    TEST_ASSERT_EQUAL_STRING("timeout", httpc_strerror(HTTPC_TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("http error", httpc_strerror(HTTPC_HTTP_ERROR));
    TEST_ASSERT_EQUAL_STRING("unknown", httpc_strerror(999));
    TEST_ASSERT_NOT_NULL(httpc_strerror(HTTPC_OK));
    TEST_ASSERT_NOT_NULL(httpc_strerror(HTTPC_ERR));
    TEST_ASSERT_NOT_NULL(httpc_strerror(HTTPC_TIMEOUT));
    TEST_ASSERT_NOT_NULL(httpc_strerror(HTTPC_HTTP_ERROR));
}

static void test_response_free(void)
{
    char url[256];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/ok");
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_OK, rc);
    httpc_response_free(resp);
    httpc_response_free(NULL);
}

void setUp(void)
{
}

void tearDown(void)
{
}

int main(void)
{
    int ret;

    signal(SIGPIPE, SIG_IGN);
    if (server_start() < 0) {
        return 1;
    }
    pthread_create(&g_thread, NULL, server_loop, NULL);
    UNITY_BEGIN();
    RUN_TEST(test_get_ok);
    RUN_TEST(test_get_notfound);
    RUN_TEST(test_get_headers);
    RUN_TEST(test_get_redirect);
    RUN_TEST(test_post_ok);
    RUN_TEST(test_get_big);
    RUN_TEST(test_get_big_limited);
    RUN_TEST(test_get_slow);
    RUN_TEST(test_strerror);
    RUN_TEST(test_response_free);
    ret = UNITY_END();
    g_shutdown = 1;
    pthread_join(g_thread, NULL);
    close(g_listen_fd);
    return ret;
}
