#define _DEFAULT_SOURCE

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <json-c/json.h>

#include "unity.h"
#include "httpc.h"
#include "jsonx.h"
#include "server.h"

static int g_port = 0;

static void make_url(char *buf, size_t n, const char *path)
{
    snprintf(buf, n, "http://127.0.0.1:%d%s", g_port, path);
}

static int pick_free_port(void)
{
    struct sockaddr_in addr;
    socklen_t alen = sizeof(addr);
    int one = 1;
    int fd;
    int port;

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
    if (getsockname(fd, (struct sockaddr *)&addr, &alen) < 0) {
        close(fd);
        return -1;
    }
    port = ntohs(addr.sin_port);
    close(fd);
    return port;
}

static const char *find_header(const httpc_response *resp, const char *name)
{
    size_t i;

    for (i = 0; i < resp->nheaders; i++) {
        if (strcasecmp(resp->headers[i].name, name) == 0) {
            return resp->headers[i].value;
        }
    }
    return NULL;
}

static void test_health(void)
{
    char url[512];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/health");
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_OK, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(200, (int)resp->status);
    TEST_ASSERT_NOT_NULL(strstr(resp->body, "\"ok\""));
    TEST_ASSERT_NOT_NULL(strstr(resp->body, "0.1.0"));
    httpc_response_free(resp);
}

static void test_root(void)
{
    char url[512];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/");
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_OK, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(200, (int)resp->status);
    TEST_ASSERT_NOT_NULL(strstr(resp->body, "openwrt-sub-converter"));
    httpc_response_free(resp);
}

static void test_convert_missing_url(void)
{
    char url[512];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/convert?type=singbox");
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_HTTP_ERROR, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(400, (int)resp->status);
    TEST_ASSERT_NOT_NULL(strstr(resp->body, "missing 'url'"));
    httpc_response_free(resp);
}

static void test_convert_source_unimplemented(void)
{
    char url[512];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/convert?source=myprovider");
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_HTTP_ERROR, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(400, (int)resp->status);
    TEST_ASSERT_NOT_NULL(strstr(resp->body, "missing 'type' or 'source'"));
    httpc_response_free(resp);
}

static void test_convert_unknown_type(void)
{
    char url[512];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/convert?type=bogus&url=http%3A%2F%2F127.0.0.1%2Fsub");
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_HTTP_ERROR, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(400, (int)resp->status);
    TEST_ASSERT_NOT_NULL(strstr(resp->body, "unknown converter"));
    httpc_response_free(resp);
}

static void test_convert_bad_headers(void)
{
    char url[512];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url),
             "/convert?type=singbox&url=http%3A%2F%2F127.0.0.1%2Fsub&headers=notjson");
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_HTTP_ERROR, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(400, (int)resp->status);
    TEST_ASSERT_NOT_NULL(strstr(resp->body, "invalid headers JSON"));
    httpc_response_free(resp);
}

static void test_convert_upstream_fail(void)
{
    char url[1024];
    httpc_req req;
    httpc_response *resp;
    int port;
    int rc;

    port = pick_free_port();
    if (port <= 0) {
        TEST_FAIL_MESSAGE("cannot pick free port");
        return;
    }
    snprintf(url, sizeof(url),
             "http://127.0.0.1:%d/convert?type=singbox&url=http%%3A%%2F%%2F127.0.0.1%%3A%d%%2Fx",
             g_port, port);
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_HTTP_ERROR, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(502, (int)resp->status);
    httpc_response_free(resp);
}

static void test_convert_missing(void)
{
    char url[512];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/convert");
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_HTTP_ERROR, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(400, (int)resp->status);
    TEST_ASSERT_NOT_NULL(strstr(resp->body, "bad_request"));
    httpc_response_free(resp);
}

static void test_404(void)
{
    char url[512];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/nope");
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_HTTP_ERROR, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(404, (int)resp->status);
    httpc_response_free(resp);
}

static void test_post_valid(void)
{
    char url[512];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/convert/manual");
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.method = "POST";
    req.body = "{\"recipe\":[]}";
    req.body_len = strlen(req.body);
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_OK, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(200, (int)resp->status);
    httpc_response_free(resp);
}

static void test_post_invalid(void)
{
    char url[512];
    httpc_req req;
    httpc_response *resp;
    int rc;

    make_url(url, sizeof(url), "/convert/manual");
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.method = "POST";
    req.body = "{invalid";
    req.body_len = strlen(req.body);
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_HTTP_ERROR, rc);
    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(400, (int)resp->status);
    httpc_response_free(resp);
}

static void test_post_valid_json(void)
{
    char url[512];
    httpc_req req;
    httpc_response *resp;
    struct json_object *obj;
    int rc;

    make_url(url, sizeof(url), "/convert/manual");
    memset(&req, 0, sizeof(req));
    req.url = url;
    req.method = "POST";
    req.body = "{\"recipe\":[]}";
    req.body_len = strlen(req.body);
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_OK, rc);
    TEST_ASSERT_NOT_NULL(resp);
    obj = jsonx_parse(resp->body, resp->body_len);
    TEST_ASSERT_NOT_NULL(obj);
    if (obj) {
        json_object_put(obj);
    }
    httpc_response_free(resp);
}

static void test_content_type(void)
{
    char url[1024];
    httpc_req req;
    httpc_response *resp;
    const char *ct;
    int port;
    int rc;

    port = pick_free_port();
    if (port <= 0) {
        TEST_FAIL_MESSAGE("cannot pick free port");
        return;
    }
    snprintf(url, sizeof(url),
             "http://127.0.0.1:%d/convert?type=singbox&url=http%%3A%%2F%%2F127.0.0.1%%3A%d%%2Fx",
             g_port, port);
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_HTTP_ERROR, rc);
    TEST_ASSERT_NOT_NULL(resp);
    ct = find_header(resp, "Content-Type");
    TEST_ASSERT_NOT_NULL(ct);
    TEST_ASSERT_EQUAL_STRING("application/json", ct);
    httpc_response_free(resp);

    make_url(url, sizeof(url), "/health");
    memset(&req, 0, sizeof(req));
    req.url = url;
    rc = httpc_request(&req, &resp);
    TEST_ASSERT_EQUAL_INT(HTTPC_OK, rc);
    TEST_ASSERT_NOT_NULL(resp);
    ct = find_header(resp, "Content-Type");
    TEST_ASSERT_NOT_NULL(ct);
    TEST_ASSERT_EQUAL_STRING("application/json", ct);
    httpc_response_free(resp);
}

void setUp(void)
{
}

void tearDown(void)
{
}

int main(void)
{
    server_config cfg;
    server *s;
    int ret;

    memset(&cfg, 0, sizeof(cfg));
    cfg.port = 0;
    s = server_create(&cfg);
    if (!s) {
        return 1;
    }
    if (server_start(s) < 0) {
        server_stop(s);
        return 1;
    }
    g_port = server_get_port(s);
    if (g_port <= 0) {
        server_stop(s);
        return 1;
    }
    UNITY_BEGIN();
    RUN_TEST(test_health);
    RUN_TEST(test_root);
    RUN_TEST(test_convert_missing);
    RUN_TEST(test_convert_missing_url);
    RUN_TEST(test_convert_source_unimplemented);
    RUN_TEST(test_convert_unknown_type);
    RUN_TEST(test_convert_bad_headers);
    RUN_TEST(test_convert_upstream_fail);
    RUN_TEST(test_404);
    RUN_TEST(test_post_valid);
    RUN_TEST(test_post_invalid);
    RUN_TEST(test_post_valid_json);
    RUN_TEST(test_content_type);
    ret = UNITY_END();
    server_stop(s);
    return ret;
}