#include "model.h"

#include <stdlib.h>
#include <string.h>

#include <json-c/json.h>

#include "base64.h"
#include "jsonx.h"
#include "util.h"

void node_init(proxy_node *n)
{
    if (n) {
        memset(n, 0, sizeof(*n));
    }
}

void node_free(proxy_node *n)
{
    if (!n) {
        return;
    }
    free(n->name);
    free(n->server);
    free(n->uuid);
    free(n->password);
    free(n->cipher);
    free(n->sni);
    free(n->alpn);
    free(n->fp);
    free(n->reality_pbk);
    free(n->reality_sid);
    free(n->ws_path);
    free(n->ws_host);
    free(n->xhttp_host);
    free(n->xhttp_path);
    free(n->xhttp_mode);
    free(n->grpc_service);
    free(n->plugin);
    free(n->plugin_opts);
    memset(n, 0, sizeof(*n));
}

void node_clear(proxy_node *n)
{
    node_free(n);
    node_init(n);
}

proxy_node *node_new(void)
{
    proxy_node *n = malloc(sizeof(*n));

    if (!n) {
        return NULL;
    }
    node_init(n);
    return n;
}

const char *node_type_name(node_type t)
{
    switch (t) {
    case NODE_TYPE_VLESS:
        return "vless";
    case NODE_TYPE_VMESS:
        return "vmess";
    case NODE_TYPE_SHADOWSOCKS:
        return "shadowsocks";
    case NODE_TYPE_TROJAN:
        return "trojan";
    case NODE_TYPE_SOCKS5:
        return "socks5";
    case NODE_TYPE_HTTP:
        return "http";
    case NODE_TYPE_HYSTERIA2:
        return "hysteria2";
    default:
        return "unknown";
    }
}

const char *node_transport_name(node_transport t)
{
    switch (t) {
    case NODE_TRANSPORT_WS:
        return "ws";
    case NODE_TRANSPORT_XHTTP:
        return "xhttp";
    case NODE_TRANSPORT_GRPC:
        return "grpc";
    case NODE_TRANSPORT_HTTP:
        return "http";
    default:
        return "none";
    }
}

typedef struct kv_pair {
    char *key;
    char *value;
} kv_pair;

static void kv_free(kv_pair *pairs, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        free(pairs[i].key);
        free(pairs[i].value);
    }
    free(pairs);
}

static void set_str(char **dst, const char *val)
{
    free(*dst);
    *dst = xstrdup(val);
}

static void trim_crlf(const char *line, size_t *len)
{
    while (*len > 0 && (line[*len - 1] == '\n' || line[*len - 1] == '\r')) {
        (*len)--;
    }
}

static int prefix_match(const char *line, size_t len, const char *scheme)
{
    size_t sl = strlen(scheme);

    return len >= sl && memcmp(line, scheme, sl) == 0;
}

static int parse_query(const char *s, size_t len, kv_pair **out_pairs, size_t *out_n)
{
    kv_pair *pairs = NULL;
    size_t npairs = 0;
    size_t i = 0;
    strbuf ksb;
    strbuf vsb;

    strbuf_init(&ksb);
    strbuf_init(&vsb);
    while (i < len) {
        size_t eq = 0;
        size_t amp = 0;
        size_t seg_end;
        size_t j;

        for (j = i; j < len; j++) {
            if (s[j] == '=') {
                eq = j;
                break;
            }
        }
        for (j = i; j < len; j++) {
            if (s[j] == '&') {
                amp = j;
                break;
            }
        }
        seg_end = amp ? amp : len;
        if (eq && eq < seg_end) {
            kv_pair *np = realloc(pairs, (npairs + 1) * sizeof(*np));

            if (!np) {
                goto fail;
            }
            pairs = np;
            strbuf_clear(&ksb);
            strbuf_clear(&vsb);
            if (url_decode(s + i, eq - i, &ksb) != 0 ||
                url_decode(s + eq + 1, seg_end - eq - 1, &vsb) != 0) {
                goto fail;
            }
            pairs[npairs].key = xstrdup(ksb.data);
            pairs[npairs].value = xstrdup(vsb.data);
            npairs++;
        }
        if (!amp) {
            break;
        }
        i = amp + 1;
    }
    strbuf_free(&ksb);
    strbuf_free(&vsb);
    *out_pairs = pairs;
    *out_n = npairs;
    return 0;

fail:
    strbuf_free(&ksb);
    strbuf_free(&vsb);
    kv_free(pairs, npairs);
    return -1;
}

static int parse_host_port(const char *s, size_t len, char **out_server, int *out_port)
{
    const char *host;
    size_t hostlen;
    const char *port_str;
    size_t portlen;
    char portbuf[16];
    char *end;
    long port;
    size_t i;
    strbuf sb;

    if (len > 0 && s[0] == '[') {
        int found = 0;

        for (i = 1; i < len; i++) {
            if (s[i] == ']') {
                if (i + 1 >= len || s[i + 1] != ':') {
                    return -1;
                }
                host = s + 1;
                hostlen = i - 1;
                port_str = s + i + 2;
                portlen = len - i - 2;
                found = 1;
                break;
            }
        }
        if (!found) {
            return -1;
        }
    } else {
        size_t colon = 0;
        int found = 0;

        for (i = 0; i < len; i++) {
            if (s[i] == ':') {
                colon = i;
                found = 1;
            }
        }
        if (!found) {
            return -1;
        }
        host = s;
        hostlen = colon;
        port_str = s + colon + 1;
        portlen = len - colon - 1;
    }

    if (hostlen == 0 || portlen == 0 || portlen >= sizeof(portbuf)) {
        return -1;
    }
    memcpy(portbuf, port_str, portlen);
    portbuf[portlen] = '\0';
    end = NULL;
    port = strtol(portbuf, &end, 10);
    if (end == portbuf || *end != '\0' || port < 1 || port > 65535) {
        return -1;
    }
    strbuf_init(&sb);
    strbuf_append_bytes(&sb, host, hostlen);
    *out_server = xstrdup(sb.data);
    strbuf_free(&sb);
    *out_port = (int)port;
    return 0;
}

static void apply_query(const kv_pair *pairs, size_t npairs, proxy_node *n)
{
    size_t i;

    for (i = 0; i < npairs; i++) {
        if (strcmp(pairs[i].key, "type") == 0) {
            const char *v = pairs[i].value;

            n->transport = NODE_TRANSPORT_NONE;
            if (strcmp(v, "ws") == 0) {
                n->transport = NODE_TRANSPORT_WS;
            } else if (strcmp(v, "xhttp") == 0) {
                n->transport = NODE_TRANSPORT_XHTTP;
            } else if (strcmp(v, "grpc") == 0) {
                n->transport = NODE_TRANSPORT_GRPC;
            } else if (strcmp(v, "http") == 0 || strcmp(v, "h2") == 0) {
                n->transport = NODE_TRANSPORT_HTTP;
            }
        } else if (strcmp(pairs[i].key, "security") == 0) {
            const char *v = pairs[i].value;

            n->tls = NODE_TLS_NONE;
            if (strcmp(v, "tls") == 0) {
                n->tls = NODE_TLS_TLS;
            } else if (strcmp(v, "reality") == 0) {
                n->tls = NODE_TLS_REALITY;
            }
        }
    }
    for (i = 0; i < npairs; i++) {
        if (strcmp(pairs[i].key, "path") == 0) {
            if (n->transport == NODE_TRANSPORT_WS) {
                set_str(&n->ws_path, pairs[i].value);
            } else {
                set_str(&n->xhttp_path, pairs[i].value);
            }
        } else if (strcmp(pairs[i].key, "host") == 0) {
            if (n->transport == NODE_TRANSPORT_WS) {
                set_str(&n->ws_host, pairs[i].value);
            } else {
                set_str(&n->xhttp_host, pairs[i].value);
            }
        } else if (strcmp(pairs[i].key, "mode") == 0) {
            set_str(&n->xhttp_mode, pairs[i].value);
        } else if (strcmp(pairs[i].key, "sni") == 0) {
            set_str(&n->sni, pairs[i].value);
        } else if (strcmp(pairs[i].key, "fp") == 0) {
            set_str(&n->fp, pairs[i].value);
        } else if (strcmp(pairs[i].key, "pbk") == 0) {
            set_str(&n->reality_pbk, pairs[i].value);
        } else if (strcmp(pairs[i].key, "sid") == 0) {
            set_str(&n->reality_sid, pairs[i].value);
        } else if (strcmp(pairs[i].key, "alpn") == 0) {
            set_str(&n->alpn, pairs[i].value);
        } else if (strcmp(pairs[i].key, "serviceName") == 0) {
            set_str(&n->grpc_service, pairs[i].value);
        }
    }
}

static void apply_ss_query(const kv_pair *pairs, size_t npairs, proxy_node *n)
{
    size_t i;

    for (i = 0; i < npairs; i++) {
        if (strcmp(pairs[i].key, "plugin") == 0) {
            set_str(&n->plugin, pairs[i].value);
        } else if (strcmp(pairs[i].key, "plugin-opts") == 0 ||
                   strcmp(pairs[i].key, "pluginOpts") == 0) {
            set_str(&n->plugin_opts, pairs[i].value);
        }
    }
}

static int set_name_from_fragment(const char *s, size_t len, size_t frag, proxy_node *n)
{
    strbuf sb;

    if (frag >= len) {
        return -1;
    }
    strbuf_init(&sb);
    if (url_decode(s + frag + 1, len - frag - 1, &sb) != 0) {
        strbuf_free(&sb);
        return -1;
    }
    n->name = xstrdup(sb.data);
    strbuf_free(&sb);
    return 0;
}

static void set_name_fallback(proxy_node *n)
{
    strbuf sb;

    strbuf_init(&sb);
    strbuf_appendf(&sb, "%s:%d", n->server, n->port);
    free(n->name);
    n->name = xstrdup(sb.data);
    strbuf_free(&sb);
}

static int parse_vless_like(const char *s, size_t len, node_type type, proxy_node *n)
{
    size_t frag = len;
    size_t query = len;
    size_t at = len;
    size_t i;
    char *host = NULL;
    int port = 0;
    kv_pair *pairs = NULL;
    size_t npairs = 0;
    strbuf sb;

    for (i = 0; i < len; i++) {
        if (s[i] == '#') {
            frag = i;
            break;
        }
    }
    for (i = 0; i < frag; i++) {
        if (s[i] == '?') {
            query = i;
            break;
        }
    }
    for (i = 0; i < query; i++) {
        if (s[i] == '@') {
            at = i;
            break;
        }
    }
    if (at == len) {
        return -1;
    }
    n->type = type;
    n->udp = 1;
    if (type == NODE_TYPE_TROJAN) {
        n->tls = NODE_TLS_TLS;
    }
    strbuf_init(&sb);
    if (url_decode(s, at, &sb) != 0) {
        strbuf_free(&sb);
        return -1;
    }
    if (type == NODE_TYPE_TROJAN) {
        n->password = xstrdup(sb.data);
    } else {
        n->uuid = xstrdup(sb.data);
    }
    strbuf_free(&sb);

    if (parse_host_port(s + at + 1, (query < frag ? query : frag) - at - 1, &host, &port) != 0) {
        return -1;
    }
    n->server = host;
    n->port = port;

    if (query < frag) {
        if (parse_query(s + query + 1, frag - query - 1, &pairs, &npairs) != 0) {
            return -1;
        }
        apply_query(pairs, npairs, n);
        kv_free(pairs, npairs);
    }
    if (set_name_from_fragment(s, len, frag, n) != 0) {
        set_name_fallback(n);
    }
    return 0;
}

static int parse_method_pass(const char *seg, size_t seglen, char **out_method, char **out_password)
{
    const char *body = seg;
    size_t bodylen = seglen;
    size_t colon = bodylen;
    size_t i;
    strbuf dec;
    strbuf tmp;

    strbuf_init(&dec);
    if (base64_decode(seg, seglen, &dec) == 0 && memchr(dec.data, ':', dec.len)) {
        body = dec.data;
        bodylen = dec.len;
    }
    for (i = 0; i < bodylen; i++) {
        if (body[i] == ':') {
            colon = i;
            break;
        }
    }
    if (colon == bodylen || colon == 0) {
        strbuf_free(&dec);
        return -1;
    }
    strbuf_init(&tmp);
    strbuf_append_bytes(&tmp, body, colon);
    *out_method = xstrdup(tmp.data);
    strbuf_free(&tmp);
    strbuf_init(&tmp);
    strbuf_append_bytes(&tmp, body + colon + 1, bodylen - colon - 1);
    *out_password = xstrdup(tmp.data);
    strbuf_free(&tmp);
    strbuf_free(&dec);
    return 0;
}

static int parse_ss(const char *s, size_t len, proxy_node *n)
{
    size_t frag = len;
    size_t query = len;
    size_t limit;
    size_t at = len;
    size_t i;
    char *method = NULL;
    char *password = NULL;
    char *host = NULL;
    int port = 0;
    kv_pair *pairs = NULL;
    size_t npairs = 0;
    strbuf sb;

    for (i = 0; i < len; i++) {
        if (s[i] == '#') {
            frag = i;
            break;
        }
    }
    for (i = 0; i < frag; i++) {
        if (s[i] == '?') {
            query = i;
            break;
        }
    }
    limit = query < frag ? query : frag;
    for (i = 0; i < limit; i++) {
        if (s[i] == '@') {
            at = i;
            break;
        }
    }
    n->type = NODE_TYPE_SHADOWSOCKS;
    n->udp = 1;

    if (at < len) {
        if (parse_method_pass(s, at, &method, &password) != 0) {
            return -1;
        }
        if (parse_host_port(s + at + 1, limit - at - 1, &host, &port) != 0) {
            free(method);
            free(password);
            return -1;
        }
    } else {
        size_t last_at = 0;
        int has_at = 0;

        strbuf_init(&sb);
        if (base64_decode_url(s, limit, &sb) != 0) {
            strbuf_free(&sb);
            return -1;
        }
        for (i = 0; i < sb.len; i++) {
            if (sb.data[i] == '@') {
                last_at = i;
                has_at = 1;
            }
        }
        if (!has_at || last_at == 0) {
            strbuf_free(&sb);
            return -1;
        }
        if (parse_method_pass(sb.data, last_at, &method, &password) != 0 ||
            parse_host_port(sb.data + last_at + 1, sb.len - last_at - 1, &host, &port) != 0) {
            free(method);
            free(password);
            strbuf_free(&sb);
            return -1;
        }
        strbuf_free(&sb);
    }

    n->cipher = method;
    n->password = password;
    n->server = host;
    n->port = port;

    if (query < frag) {
        if (parse_query(s + query + 1, frag - query - 1, &pairs, &npairs) != 0) {
            return -1;
        }
        apply_ss_query(pairs, npairs, n);
        kv_free(pairs, npairs);
    }
    if (set_name_from_fragment(s, len, frag, n) != 0) {
        set_name_fallback(n);
    }
    return 0;
}

static int json_int_field(const struct json_object *obj, const char *key, int64_t *out)
{
    struct json_object *v = jsonx_obj_get(obj, key);
    char *end;
    long l;

    if (!v) {
        return -1;
    }
    if (jsonx_is_int(v)) {
        *out = jsonx_int(v);
        return 0;
    }
    if (jsonx_is_string(v)) {
        const char *sv = jsonx_str(v);

        l = strtol(sv, &end, 10);
        if (end != sv && *end == '\0') {
            *out = l;
            return 0;
        }
    }
    return -1;
}

static int parse_vmess_json(const char *s, size_t len, proxy_node *n)
{
    struct json_object *obj;
    struct json_object *v;
    const char *sv;
    char *end;
    long l;
    int64_t iv;
    int rc = -1;
    strbuf sb;

    strbuf_init(&sb);
    if (base64_decode(s, len, &sb) != 0) {
        strbuf_free(&sb);
        return -1;
    }
    obj = jsonx_parse(sb.data, sb.len);
    strbuf_free(&sb);
    if (!obj) {
        return -1;
    }

    n->type = NODE_TYPE_VMESS;
    n->udp = 1;
    n->cipher = xstrdup("auto");

    if (jsonx_get_string(obj, "add", &sv) == 0) {
        n->server = xstrdup(sv);
    }
    v = jsonx_obj_get(obj, "port");
    if (v && jsonx_is_int(v)) {
        iv = jsonx_int(v);
    } else if (v && jsonx_is_string(v)) {
        l = strtol(jsonx_str(v), &end, 10);
        if (end != jsonx_str(v) && *end == '\0') {
            iv = l;
        } else {
            iv = -1;
        }
    } else {
        iv = -1;
    }
    if (!n->server || iv < 1 || iv > 65535) {
        goto out;
    }
    n->port = (int)iv;

    if (jsonx_get_string(obj, "id", &sv) == 0) {
        n->uuid = xstrdup(sv);
    }
    if (json_int_field(obj, "aid", &iv) == 0) {
        n->alter_id = (int)iv;
    }
    v = jsonx_obj_get(obj, "scy");
    if (v && jsonx_is_string(v)) {
        set_str(&n->cipher, jsonx_str(v));
    }
    v = jsonx_obj_get(obj, "net");
    if (v && jsonx_is_string(v)) {
        const char *net = jsonx_str(v);

        n->transport = NODE_TRANSPORT_NONE;
        if (strcmp(net, "ws") == 0) {
            n->transport = NODE_TRANSPORT_WS;
        } else if (strcmp(net, "xhttp") == 0) {
            n->transport = NODE_TRANSPORT_XHTTP;
        } else if (strcmp(net, "grpc") == 0) {
            n->transport = NODE_TRANSPORT_GRPC;
        } else if (strcmp(net, "http") == 0 || strcmp(net, "h2") == 0) {
            n->transport = NODE_TRANSPORT_HTTP;
        }
    }
    v = jsonx_obj_get(obj, "tls");
    if (v && jsonx_is_string(v)) {
        const char *t = jsonx_str(v);

        if (strcmp(t, "tls") == 0) {
            n->tls = NODE_TLS_TLS;
        } else if (strcmp(t, "reality") == 0) {
            n->tls = NODE_TLS_REALITY;
        }
    }
    v = jsonx_obj_get(obj, "host");
    if (v && jsonx_is_string(v)) {
        if (n->transport == NODE_TRANSPORT_WS) {
            set_str(&n->ws_host, jsonx_str(v));
        } else {
            set_str(&n->xhttp_host, jsonx_str(v));
        }
    }
    v = jsonx_obj_get(obj, "path");
    if (v && jsonx_is_string(v)) {
        if (n->transport == NODE_TRANSPORT_WS) {
            set_str(&n->ws_path, jsonx_str(v));
        } else {
            set_str(&n->xhttp_path, jsonx_str(v));
        }
    }
    v = jsonx_obj_get(obj, "sni");
    if (v && jsonx_is_string(v)) {
        set_str(&n->sni, jsonx_str(v));
    }
    v = jsonx_obj_get(obj, "alpn");
    if (v && jsonx_is_string(v)) {
        set_str(&n->alpn, jsonx_str(v));
    }
    v = jsonx_obj_get(obj, "fp");
    if (v && jsonx_is_string(v)) {
        set_str(&n->fp, jsonx_str(v));
    }
    v = jsonx_obj_get(obj, "serviceName");
    if (v && jsonx_is_string(v)) {
        set_str(&n->grpc_service, jsonx_str(v));
    }
    v = jsonx_obj_get(obj, "ps");
    if (v && jsonx_is_string(v)) {
        strbuf nbuf;

        strbuf_init(&nbuf);
        if (url_decode(jsonx_str(v), strlen(jsonx_str(v)), &nbuf) == 0) {
            n->name = xstrdup(nbuf.data);
        } else {
            n->name = xstrdup(jsonx_str(v));
        }
        strbuf_free(&nbuf);
    }
    if (!n->name) {
        set_name_fallback(n);
    }
    rc = 0;

out:
    json_object_put(obj);
    return rc;
}

static int parse_vmess(const char *s, size_t len, proxy_node *n)
{
    size_t frag = len;
    size_t i;

    for (i = 0; i < len; i++) {
        if (s[i] == '#') {
            frag = i;
            break;
        }
    }
    for (i = 0; i < frag; i++) {
        if (s[i] == '@') {
            if (parse_vless_like(s, len, NODE_TYPE_VMESS, n) != 0) {
                return -1;
            }
            n->cipher = xstrdup("auto");
            return 0;
        }
    }
    return parse_vmess_json(s, frag, n);
}

int node_parse_share(const char *line, size_t len, proxy_node *n)
{
    int rc = -1;

    node_init(n);
    if (!line) {
        return -1;
    }
    trim_crlf(line, &len);
    if (prefix_match(line, len, "vless://")) {
        rc = parse_vless_like(line + 8, len - 8, NODE_TYPE_VLESS, n);
    } else if (prefix_match(line, len, "trojan://")) {
        rc = parse_vless_like(line + 9, len - 9, NODE_TYPE_TROJAN, n);
    } else if (prefix_match(line, len, "vmess://")) {
        rc = parse_vmess(line + 8, len - 8, n);
    } else if (prefix_match(line, len, "ss://")) {
        rc = parse_ss(line + 5, len - 5, n);
    }
    if (rc != 0) {
        node_clear(n);
    }
    return rc;
}
