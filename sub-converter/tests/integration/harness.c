#define _DEFAULT_SOURCE

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "httpc.h"
#include "util.h"

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

static char *read_file(const char *path, size_t *out_len)
{
    FILE *f;
    char *data;
    long sz;
    size_t n;

    f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    data = malloc((size_t)sz + 1);
    if (!data) {
        fclose(f);
        return NULL;
    }
    n = fread(data, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) {
        free(data);
        return NULL;
    }
    data[n] = '\0';
    *out_len = n;
    return data;
}

static char *sb_detach(strbuf *sb)
{
    char *out;

    if (!sb->data) {
        out = xstrdup("");
    } else {
        out = sb->data;
        sb->data = NULL;
    }
    sb->len = 0;
    sb->cap = 0;
    return out;
}

static char *encode(const char *s)
{
    strbuf sb;

    strbuf_init(&sb);
    url_encode(s, strlen(s), &sb);
    return sb_detach(&sb);
}

static char *mk_url(int port, const char *path)
{
    strbuf sb;

    strbuf_init(&sb);
    strbuf_appendf(&sb, "http://127.0.0.1:%d%s", port, path);
    return sb_detach(&sb);
}

static char *mk_convert(int port, const char *query)
{
    strbuf sb;

    strbuf_init(&sb);
    strbuf_appendf(&sb, "http://127.0.0.1:%d/convert?%s", port, query);
    return sb_detach(&sb);
}

static httpc_response *http_get(const char *url)
{
    httpc_req req;
    httpc_response *resp = NULL;

    memset(&req, 0, sizeof(req));
    req.url = url;
    req.timeout_ms = 2000;
    httpc_request(&req, &resp);
    return resp;
}

static int wait_ready(const char *url)
{
    int i;

    for (i = 0; i < 50; i++) {
        httpc_response *resp = http_get(url);

        if (resp) {
            if (resp->status == 200) {
                httpc_response_free(resp);
                return 0;
            }
            httpc_response_free(resp);
        }
        usleep(100000);
    }
    return -1;
}

static int spawn_process(char *const argv[], pid_t *out_pid)
{
    int devnull;
    pid_t pid;

    devnull = open("/dev/null", O_WRONLY);
    if (devnull < 0) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        close(devnull);
        return -1;
    }
    if (pid == 0) {
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        close(devnull);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(devnull);
    *out_pid = pid;
    return 0;
}

static int run_check(int cond, const char *desc)
{
    if (cond) {
        printf("PASS: %s\n", desc);
    } else {
        printf("FAIL: %s\n", desc);
    }
    return cond;
}

static int golden_match(const httpc_response *resp, const char *golden, size_t golden_len)
{
    return resp && resp->status == 200 && resp->body_len == golden_len &&
           memcmp(resp->body, golden, golden_len) == 0;
}

static int check_golden(const char *url, const char *golden, size_t golden_len, const char *desc,
                        int *failed)
{
    httpc_response *resp = http_get(url);

    if (!resp) {
        printf("FAIL: %s (request failed)\n", desc);
        *failed = 1;
        return 1;
    }
    if (!run_check(golden_match(resp, golden, golden_len), desc)) {
        httpc_response_free(resp);
        *failed = 1;
        return 1;
    }
    httpc_response_free(resp);
    return 0;
}

static int check_status(const char *url, int expect, const char *desc, int *failed)
{
    httpc_response *resp = http_get(url);

    if (!resp) {
        printf("FAIL: %s (request failed)\n", desc);
        *failed = 1;
        return 1;
    }
    if (!run_check(resp->status == expect, desc)) {
        httpc_response_free(resp);
        *failed = 1;
        return 1;
    }
    httpc_response_free(resp);
    return 0;
}

static int check_body(const char *url, int expect, const char *needle, const char *desc,
                      int *failed)
{
    httpc_response *resp = http_get(url);

    if (!resp) {
        printf("FAIL: %s (request failed)\n", desc);
        *failed = 1;
        return 1;
    }
    if (!run_check(resp->status == expect && resp->body && strstr(resp->body, needle) != NULL,
                   desc)) {
        httpc_response_free(resp);
        *failed = 1;
        return 1;
    }
    httpc_response_free(resp);
    return 0;
}

static void cleanup_children(pid_t convd_pid, pid_t mock_pid)
{
    if (convd_pid > 0) {
        kill(convd_pid, SIGTERM);
    }
    if (mock_pid > 0) {
        kill(mock_pid, SIGTERM);
    }
    if (convd_pid > 0) {
        waitpid(convd_pid, NULL, 0);
    }
    if (mock_pid > 0) {
        waitpid(mock_pid, NULL, 0);
    }
}

int main(int argc, char **argv)
{
    char *convd_path;
    char *mock_path;
    char *fixture_path;
    char *json_path;
    char *golden_path;
    pid_t convd_pid = -1;
    pid_t mock_pid = -1;
    int mock_port;
    int daemon_port;
    int closed_port;
    char *fixture = NULL;
    size_t fixture_len = 0;
    char *example_json = NULL;
    size_t example_len = 0;
    char *golden = NULL;
    size_t golden_len = 0;
    char *health_url = NULL;
    char *mock_health_url = NULL;
    char *mock_sub_url = NULL;
    char *mock_example_url = NULL;
    char *example_enc = NULL;
    char *closed_src = NULL;
    char *closed_enc = NULL;
    char *headers_enc = NULL;
    char *bad_headers_enc = NULL;
    char *convert_ok_url = NULL;
    char *convert_headers_url = NULL;
    char *convert_bad_headers_url = NULL;
    char *convert_no_url_url = NULL;
    char *convert_bogus_url = NULL;
    char *convert_closed_url = NULL;
    char *convert_none_url = NULL;
    char *nope_url = NULL;
    httpc_response *resp = NULL;
    strbuf q;
    int failed = 0;

    if (argc < 6) {
        fprintf(stderr, "usage: %s <subconvd> <mock_upstream> <sub.txt> <singbox.json> <expected.yaml>\n",
                argv[0]);
        return 2;
    }
    convd_path = argv[1];
    mock_path = argv[2];
    fixture_path = argv[3];
    json_path = argv[4];
    golden_path = argv[5];

    fixture = read_file(fixture_path, &fixture_len);
    if (!fixture) {
        fprintf(stderr, "harness: cannot read fixture %s\n", fixture_path);
        return 1;
    }
    example_json = read_file(json_path, &example_len);
    if (!example_json) {
        fprintf(stderr, "harness: cannot read example json %s\n", json_path);
        failed = 1;
        goto cleanup;
    }
    golden = read_file(golden_path, &golden_len);
    if (!golden) {
        fprintf(stderr, "harness: cannot read golden yaml %s\n", golden_path);
        failed = 1;
        goto cleanup;
    }

    mock_port = pick_free_port();
    daemon_port = pick_free_port();
    closed_port = pick_free_port();
    if (mock_port <= 0 || daemon_port <= 0 || closed_port <= 0) {
        fprintf(stderr, "harness: cannot pick free ports\n");
        failed = 1;
        goto cleanup;
    }

    {
        char mock_port_s[16];
        char daemon_port_s[16];
        char *mock_argv[] = { mock_path, "--port", mock_port_s, "--file", fixture_path,
                              "--jsonfile", json_path, NULL };
        char *convd_argv[] = { convd_path, "--host", "127.0.0.1", "--port", daemon_port_s, NULL };

        snprintf(mock_port_s, sizeof(mock_port_s), "%d", mock_port);
        snprintf(daemon_port_s, sizeof(daemon_port_s), "%d", daemon_port);
        if (spawn_process(mock_argv, &mock_pid) < 0) {
            fprintf(stderr, "harness: cannot start mock_upstream\n");
            failed = 1;
            goto cleanup;
        }
        if (spawn_process(convd_argv, &convd_pid) < 0) {
            fprintf(stderr, "harness: cannot start subconvd\n");
            failed = 1;
            goto cleanup;
        }
    }

    health_url = mk_url(daemon_port, "/health");
    mock_health_url = mk_url(mock_port, "/health");
    mock_sub_url = mk_url(mock_port, "/sub");
    mock_example_url = mk_url(mock_port, "/example");
    example_enc = encode(mock_example_url);
    closed_src = mk_url(closed_port, "/x");
    closed_enc = encode(closed_src);
    headers_enc = encode("{\"X-Test\":\"1\"}");
    bad_headers_enc = encode("\"notjson\"");

    strbuf_init(&q);
    strbuf_appendf(&q, "type=singbox&url=%s", example_enc);
    convert_ok_url = mk_convert(daemon_port, q.data);
    strbuf_clear(&q);
    strbuf_appendf(&q, "type=singbox&url=%s&headers=%s", example_enc, headers_enc);
    convert_headers_url = mk_convert(daemon_port, q.data);
    strbuf_clear(&q);
    strbuf_appendf(&q, "type=singbox&url=%s&headers=%s", example_enc, bad_headers_enc);
    convert_bad_headers_url = mk_convert(daemon_port, q.data);
    strbuf_clear(&q);
    strbuf_appendf(&q, "type=singbox");
    convert_no_url_url = mk_convert(daemon_port, q.data);
    strbuf_clear(&q);
    strbuf_appendf(&q, "type=bogus&url=%s", example_enc);
    convert_bogus_url = mk_convert(daemon_port, q.data);
    strbuf_clear(&q);
    strbuf_appendf(&q, "type=singbox&url=%s", closed_enc);
    convert_closed_url = mk_convert(daemon_port, q.data);
    strbuf_free(&q);

    convert_none_url = mk_url(daemon_port, "/convert");
    nope_url = mk_url(daemon_port, "/nope");

    if (wait_ready(health_url) < 0) {
        printf("FAIL: subconvd became ready\n");
        failed = 1;
        goto cleanup;
    }
    printf("PASS: subconvd became ready\n");

    if (wait_ready(mock_health_url) < 0) {
        printf("FAIL: mock_upstream became ready\n");
        failed = 1;
        goto cleanup;
    }
    printf("PASS: mock_upstream became ready\n");

    resp = http_get(health_url);
    if (!resp) {
        printf("FAIL: daemon /health request failed\n");
        failed = 1;
        goto cleanup;
    }
    if (!run_check(resp->status == 200 && strstr(resp->body, "\"ok\"") != NULL,
                   "daemon /health returns 200 with ok")) {
        failed = 1;
        goto cleanup;
    }
    httpc_response_free(resp);
    resp = NULL;

    resp = http_get(mock_sub_url);
    if (!resp) {
        printf("FAIL: mock /sub request failed\n");
        failed = 1;
        goto cleanup;
    }
    if (!run_check(resp->status == 200 && resp->body_len == fixture_len &&
                       memcmp(resp->body, fixture, fixture_len) == 0,
                   "mock /sub returns fixture contents exactly")) {
        failed = 1;
        goto cleanup;
    }
    httpc_response_free(resp);
    resp = NULL;

    resp = http_get(mock_example_url);
    if (!resp) {
        printf("FAIL: mock /example request failed\n");
        failed = 1;
        goto cleanup;
    }
    if (!run_check(resp->status == 200 && resp->body_len == example_len &&
                       memcmp(resp->body, example_json, example_len) == 0,
                   "mock /example returns example.json contents exactly")) {
        failed = 1;
        goto cleanup;
    }
    httpc_response_free(resp);
    resp = NULL;

    if (check_golden(convert_ok_url, golden, golden_len,
                     "daemon /convert converts example.json to golden singbox yaml", &failed)) {
        goto cleanup;
    }
    if (check_golden(convert_headers_url, golden, golden_len,
                     "daemon /convert with forwarded headers still yields golden yaml", &failed)) {
        goto cleanup;
    }
    if (check_status(convert_bad_headers_url, 400,
                     "daemon /convert with non-object headers returns 400", &failed)) {
        goto cleanup;
    }
    if (check_status(convert_no_url_url, 400,
                     "daemon /convert with missing url returns 400", &failed)) {
        goto cleanup;
    }
    if (check_body(convert_bogus_url, 400, "unknown converter",
                   "daemon /convert with unknown converter returns 400", &failed)) {
        goto cleanup;
    }
    if (check_status(convert_closed_url, 502,
                     "daemon /convert with unreachable upstream returns 502", &failed)) {
        goto cleanup;
    }
    if (check_status(convert_none_url, 400,
                     "daemon /convert without params returns 400", &failed)) {
        goto cleanup;
    }
    if (check_status(nope_url, 404, "daemon GET /nope returns 404", &failed)) {
        goto cleanup;
    }

    if (failed == 0) {
        printf("PASS: all integration checks passed\n");
    }

cleanup:
    if (resp) {
        httpc_response_free(resp);
    }
    cleanup_children(convd_pid, mock_pid);
    free(health_url);
    free(mock_health_url);
    free(mock_sub_url);
    free(mock_example_url);
    free(example_enc);
    free(closed_src);
    free(closed_enc);
    free(headers_enc);
    free(bad_headers_enc);
    free(convert_ok_url);
    free(convert_headers_url);
    free(convert_bad_headers_url);
    free(convert_no_url_url);
    free(convert_bogus_url);
    free(convert_closed_url);
    free(convert_none_url);
    free(nope_url);
    free(fixture);
    free(example_json);
    free(golden);
    return failed ? 1 : 0;
}
