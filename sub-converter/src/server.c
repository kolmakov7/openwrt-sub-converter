#include "server.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <json-c/json.h>
#include <microhttpd.h>

#include "converter.h"
#include "httpc.h"
#include "jsonx.h"
#include "util.h"
#include "version.h"

struct server {
    char *host;
    int port;
    struct MHD_Daemon *daemon;
};

static char *strbuf_detach(strbuf *sb)
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

static enum MHD_Result send_response(struct MHD_Connection *con, unsigned int status,
                                     const char *ctype, char *body)
{
    struct MHD_Response *resp;
    enum MHD_Result ret;

    resp = MHD_create_response_from_buffer(strlen(body), body, MHD_RESPMEM_MUST_COPY);
    if (!resp) {
        free(body);
        return MHD_NO;
    }
    MHD_add_response_header(resp, MHD_HTTP_HEADER_CONTENT_TYPE, ctype);
    ret = MHD_queue_response(con, status, resp);
    MHD_destroy_response(resp);
    free(body);
    return ret;
}

static enum MHD_Result method_not_allowed(struct MHD_Connection *con)
{
    return send_response(con, MHD_HTTP_METHOD_NOT_ALLOWED, "application/json",
                         xstrdup("{\"error\":\"method not allowed\",\"code\":\"method_not_allowed\"}"));
}

static enum MHD_Result handle_root(struct MHD_Connection *con, const char *method)
{
    const converter *const *cs;
    strbuf sb;
    char *body;
    size_t n;
    size_t i;

    if (strcmp(method, MHD_HTTP_METHOD_GET) != 0) {
        return method_not_allowed(con);
    }
    cs = converter_all();
    n = converter_count();
    strbuf_init(&sb);
    strbuf_appendf(&sb, "{\"service\":\"openwrt-sub-converter\",\"version\":\"%s\",\"converters\":[",
                   subconv_version());
    for (i = 0; i < n; i++) {
        if (i > 0) {
            strbuf_append_char(&sb, ',');
        }
        strbuf_appendf(&sb, "\"%s\"", cs[i]->name);
    }
    strbuf_append_str(&sb, "],\"endpoints\":[\"/health\",\"/convert\",\"/convert/manual\"]}");
    body = strbuf_detach(&sb);
    return send_response(con, MHD_HTTP_OK, "application/json", body);
}

static enum MHD_Result handle_health(struct MHD_Connection *con, const char *method)
{
    strbuf sb;
    char *body;

    if (strcmp(method, MHD_HTTP_METHOD_GET) != 0) {
        return method_not_allowed(con);
    }
    strbuf_init(&sb);
    strbuf_appendf(&sb, "{\"status\":\"ok\",\"version\":\"%s\"}", subconv_version());
    body = strbuf_detach(&sb);
    return send_response(con, MHD_HTTP_OK, "application/json", body);
}

static void free_header_arr(char **arr, size_t n)
{
    size_t i;

    if (!arr) {
        return;
    }
    for (i = 0; i < n; i++) {
        free(arr[i]);
    }
    free(arr);
}

static char **parse_headers(const char *json, size_t *out_n)
{
    struct json_object *obj;
    char **arr = NULL;
    size_t n = 0;
    size_t cap = 0;
    int idx = 0;
    const char *key;
    struct json_object *val;

    *out_n = 0;
    obj = jsonx_parse(json, strlen(json));
    if (!obj || !jsonx_is_object(obj)) {
        json_object_put(obj);
        return NULL;
    }
    while (jsonx_obj_entries(obj, &idx, &key, &val) == 0) {
        const char *v;
        strbuf sb;
        char *h;

        if (!key || !jsonx_is_string(val)) {
            continue;
        }
        if (strcasecmp(key, "host") == 0 || strcasecmp(key, "content-length") == 0 ||
            strcasecmp(key, "transfer-encoding") == 0) {
            continue;
        }
        v = jsonx_str(val);
        strbuf_init(&sb);
        strbuf_appendf(&sb, "%s: %s", key, v);
        h = strbuf_detach(&sb);
        if (n == cap) {
            size_t ncap = cap ? cap * 2 : 8;
            char **na = realloc(arr, ncap * sizeof(*na));

            if (!na) {
                free(h);
                free_header_arr(arr, n);
                json_object_put(obj);
                return NULL;
            }
            arr = na;
            cap = ncap;
        }
        arr[n++] = h;
    }
    json_object_put(obj);
    *out_n = n;
    return arr;
}

static enum MHD_Result handle_convert(struct MHD_Connection *con, const char *method)
{
    const char *type;
    const char *urlv;
    const char *headers;
    const converter *conv;
    char **hdr_arr = NULL;
    size_t nheaders = 0;
    httpc_req req;
    httpc_response *resp = NULL;
    char *out_yaml = NULL;
    conv_ctx ctx;
    strbuf sb;
    char *body;
    long status;
    int rc;

    if (strcmp(method, MHD_HTTP_METHOD_GET) != 0) {
        return method_not_allowed(con);
    }
    type = MHD_lookup_connection_value(con, MHD_GET_ARGUMENT_KIND, "type");
    urlv = MHD_lookup_connection_value(con, MHD_GET_ARGUMENT_KIND, "url");
    headers = MHD_lookup_connection_value(con, MHD_GET_ARGUMENT_KIND, "headers");
    if (!type) {
        return send_response(con, MHD_HTTP_BAD_REQUEST, "application/json",
                             xstrdup("{\"error\":\"missing 'type' or 'source'\",\"code\":\"bad_request\"}"));
    }
    conv = converter_find(type);
    if (!conv) {
        return send_response(con, MHD_HTTP_BAD_REQUEST, "application/json",
                             xstrdup("{\"error\":\"unknown converter\",\"code\":\"bad_request\"}"));
    }
    if (!urlv) {
        return send_response(con, MHD_HTTP_BAD_REQUEST, "application/json",
                             xstrdup("{\"error\":\"missing 'url'\",\"code\":\"bad_request\"}"));
    }
    if (headers && *headers) {
        hdr_arr = parse_headers(headers, &nheaders);
        if (!hdr_arr) {
            return send_response(con, MHD_HTTP_BAD_REQUEST, "application/json",
                                 xstrdup("{\"error\":\"invalid headers JSON\",\"code\":\"bad_request\"}"));
        }
    }
    memset(&req, 0, sizeof(req));
    req.url = urlv;
    req.method = "GET";
    req.timeout_ms = 30000;
    req.max_response = 2097152;
    req.headers = (const char *const *)hdr_arr;
    req.nheaders = nheaders;
    rc = httpc_request(&req, &resp);
    free_header_arr(hdr_arr, nheaders);
    if (rc == HTTPC_TIMEOUT) {
        httpc_response_free(resp);
        return send_response(con, MHD_HTTP_GATEWAY_TIMEOUT, "application/json",
                             xstrdup("{\"error\":\"upstream timeout\",\"code\":\"timeout\"}"));
    }
    if (rc == HTTPC_ERR) {
        httpc_response_free(resp);
        return send_response(con, MHD_HTTP_BAD_GATEWAY, "application/json",
                             xstrdup("{\"error\":\"upstream fetch failed\",\"code\":\"bad_gateway\"}"));
    }
    if (rc == HTTPC_HTTP_ERROR || resp->status != 200) {
        status = resp->status;
        httpc_response_free(resp);
        strbuf_init(&sb);
        strbuf_appendf(&sb, "{\"error\":\"upstream returned HTTP %ld\",\"code\":\"bad_gateway\"}", status);
        body = strbuf_detach(&sb);
        return send_response(con, MHD_HTTP_BAD_GATEWAY, "application/json", body);
    }
    ctx.data = resp->body;
    ctx.data_len = resp->body_len;
    if (conv->run(&ctx, &out_yaml) != 0) {
        httpc_response_free(resp);
        return send_response(con, MHD_HTTP_UNPROCESSABLE_CONTENT, "application/json",
                             xstrdup("{\"error\":\"source data is not convertible\",\"code\":\"unprocessable\"}"));
    }
    httpc_response_free(resp);
    return send_response(con, MHD_HTTP_OK, "application/yaml", out_yaml);
}

static enum MHD_Result handle_convert_manual(struct MHD_Connection *con, const char *method,
                                             strbuf *body)
{
    struct json_object *obj;

    if (strcmp(method, MHD_HTTP_METHOD_POST) != 0) {
        return method_not_allowed(con);
    }
    obj = jsonx_parse(body->data ? body->data : "", body->len);
    if (obj) {
        char *echo = strbuf_detach(body);

        json_object_put(obj);
        return send_response(con, MHD_HTTP_OK, "application/json", echo);
    }
    return send_response(con, MHD_HTTP_BAD_REQUEST, "application/json",
                         xstrdup("{\"error\":\"invalid JSON body\",\"code\":\"bad_request\"}"));
}

static enum MHD_Result route_request(struct MHD_Connection *con, const char *url,
                                     const char *method, strbuf *body)
{
    const char *q = strchr(url, '?');
    size_t plen = q ? (size_t)(q - url) : strlen(url);

    if (plen == 1 && memcmp(url, "/", 1) == 0) {
        return handle_root(con, method);
    }
    if (plen == 7 && memcmp(url, "/health", 7) == 0) {
        return handle_health(con, method);
    }
    if (plen == 8 && memcmp(url, "/convert", 8) == 0) {
        return handle_convert(con, method);
    }
    if (plen == 15 && memcmp(url, "/convert/manual", 15) == 0) {
        return handle_convert_manual(con, method, body);
    }
    return send_response(con, MHD_HTTP_NOT_FOUND, "application/json",
                         xstrdup("{\"error\":\"not found\",\"code\":\"not_found\"}"));
}

static enum MHD_Result handler_cb(void *cls, struct MHD_Connection *con,
                                  const char *url, const char *method,
                                  const char *version, const char *upload_data,
                                  size_t *upload_data_size, void **con_cls)
{
    strbuf *sb;

    (void)cls;
    (void)version;
    if (!*con_cls) {
        sb = malloc(sizeof(*sb));
        if (!sb) {
            return MHD_NO;
        }
        strbuf_init(sb);
        *con_cls = sb;
        return MHD_YES;
    }
    sb = *con_cls;
    if (*upload_data_size > 0) {
        strbuf_append_bytes(sb, upload_data, *upload_data_size);
        *upload_data_size = 0;
        return MHD_YES;
    }
    {
        enum MHD_Result ret = route_request(con, url, method, sb);

        strbuf_free(sb);
        free(sb);
        *con_cls = NULL;
        return ret;
    }
}

static void cleanup_cb(void *cls, struct MHD_Connection *con, void **con_cls,
                       enum MHD_RequestTerminationCode toe)
{
    strbuf *sb;

    (void)cls;
    (void)con;
    (void)toe;
    sb = *con_cls;
    if (sb) {
        strbuf_free(sb);
        free(sb);
    }
}

server *server_create(const server_config *cfg)
{
    server *s;

    s = calloc(1, sizeof(*s));
    if (!s) {
        return NULL;
    }
    if (cfg) {
        s->port = cfg->port;
    }
    s->host = (cfg && cfg->listen_host) ? xstrdup(cfg->listen_host) : xstrdup("0.0.0.0");
    return s;
}

int server_start(server *s)
{
    if (!s) {
        return -1;
    }
    s->daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_POLL,
                                 (uint16_t)s->port, NULL, NULL, handler_cb, s,
                                 MHD_OPTION_CONNECTION_TIMEOUT, 30,
                                 MHD_OPTION_NOTIFY_COMPLETED, cleanup_cb, NULL,
                                 MHD_OPTION_END);
    return s->daemon ? 0 : -1;
}

void server_stop(server *s)
{
    if (!s) {
        return;
    }
    if (s->daemon) {
        MHD_stop_daemon(s->daemon);
    }
    free(s->host);
    free(s);
}

int server_get_port(const server *s)
{
    const union MHD_DaemonInfo *info;

    if (!s || !s->daemon) {
        return -1;
    }
    info = MHD_get_daemon_info(s->daemon, MHD_DAEMON_INFO_BIND_PORT);
    if (!info) {
        return -1;
    }
    return (int)info->port;
}