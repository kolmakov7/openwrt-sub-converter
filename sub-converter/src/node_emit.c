#include "node_emit.h"

#include "model.h"
#include "util.h"
#include "yaml_emit.h"

static const char *nvl(const char *p)
{
    return p ? p : "";
}

static const char *network_name(node_transport t)
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
        return "tcp";
    }
}

static void emit_ws_opts(struct strbuf *out, const proxy_node *n)
{
    yaml_emit_kv(out, 4, "ws-opts", NULL);
    yaml_emit_kv(out, 6, "path", nvl(n->ws_path));
    yaml_emit_kv(out, 6, "headers", NULL);
    yaml_emit_kv(out, 8, "Host", nvl(n->ws_host));
}

static void emit_xhttp_opts(struct strbuf *out, const proxy_node *n)
{
    yaml_emit_kv(out, 4, "xhttp-opts", NULL);
    yaml_emit_kv(out, 6, "host", nvl(n->xhttp_host));
    yaml_emit_kv(out, 6, "path", nvl(n->xhttp_path));
    yaml_emit_kv(out, 6, "mode", nvl(n->xhttp_mode));
}

static void emit_reality(struct strbuf *out, const proxy_node *n)
{
    if (n->fp && n->fp[0]) {
        yaml_emit_kv(out, 4, "client-fingerprint", n->fp);
    }
    if (n->reality_pbk || n->reality_sid) {
        yaml_emit_kv(out, 4, "reality-opts", NULL);
        if (n->reality_pbk) {
            yaml_emit_kv(out, 6, "public-key", n->reality_pbk);
        }
        if (n->reality_sid) {
            yaml_emit_kv(out, 6, "short-id", n->reality_sid);
        }
    }
}

static void emit_vless(struct strbuf *out, const proxy_node *n)
{
    yaml_emit_dash_kv(out, 2, "name", nvl(n->name));
    yaml_emit_kv(out, 4, "type", "vless");
    yaml_emit_kv(out, 4, "server", nvl(n->server));
    yaml_emit_kv_int(out, 4, "port", n->port);
    yaml_emit_kv(out, 4, "uuid", nvl(n->uuid));
    yaml_emit_kv_bool(out, 4, "udp", n->udp);
    yaml_emit_kv(out, 4, "network", network_name(n->transport));
    yaml_emit_kv_bool(out, 4, "tls", n->tls != NODE_TLS_NONE);
    if (n->tls != NODE_TLS_NONE && n->sni && n->sni[0]) {
        yaml_emit_kv(out, 4, "servername", n->sni);
    }
    if (n->tls == NODE_TLS_REALITY) {
        emit_reality(out, n);
    }
    if (n->transport == NODE_TRANSPORT_WS) {
        emit_ws_opts(out, n);
    }
    if (n->transport == NODE_TRANSPORT_XHTTP) {
        emit_xhttp_opts(out, n);
    }
}

static void emit_vmess(struct strbuf *out, const proxy_node *n)
{
    yaml_emit_dash_kv(out, 2, "name", nvl(n->name));
    yaml_emit_kv(out, 4, "type", "vmess");
    yaml_emit_kv(out, 4, "server", nvl(n->server));
    yaml_emit_kv_int(out, 4, "port", n->port);
    yaml_emit_kv(out, 4, "uuid", nvl(n->uuid));
    yaml_emit_kv_int(out, 4, "alterId", n->alter_id);
    yaml_emit_kv(out, 4, "cipher", n->cipher && n->cipher[0] ? n->cipher : "auto");
    yaml_emit_kv_bool(out, 4, "udp", n->udp);
    yaml_emit_kv(out, 4, "network", network_name(n->transport));
    yaml_emit_kv_bool(out, 4, "tls", n->tls != NODE_TLS_NONE);
    if (n->tls != NODE_TLS_NONE && n->sni && n->sni[0]) {
        yaml_emit_kv(out, 4, "servername", n->sni);
    }
    if (n->transport == NODE_TRANSPORT_WS) {
        emit_ws_opts(out, n);
    }
}

static void emit_ss(struct strbuf *out, const proxy_node *n)
{
    yaml_emit_dash_kv(out, 2, "name", nvl(n->name));
    yaml_emit_kv(out, 4, "type", "ss");
    yaml_emit_kv(out, 4, "server", nvl(n->server));
    yaml_emit_kv_int(out, 4, "port", n->port);
    yaml_emit_kv(out, 4, "cipher", nvl(n->cipher));
    yaml_emit_kv(out, 4, "password", nvl(n->password));
    yaml_emit_kv_bool(out, 4, "udp", n->udp);
    if (n->plugin && n->plugin[0]) {
        yaml_emit_kv(out, 4, "plugin", n->plugin);
    }
    if (n->plugin_opts && n->plugin_opts[0]) {
        yaml_emit_kv(out, 4, "plugin-opts", n->plugin_opts);
    }
}

static void emit_trojan(struct strbuf *out, const proxy_node *n)
{
    yaml_emit_dash_kv(out, 2, "name", nvl(n->name));
    yaml_emit_kv(out, 4, "type", "trojan");
    yaml_emit_kv(out, 4, "server", nvl(n->server));
    yaml_emit_kv_int(out, 4, "port", n->port);
    yaml_emit_kv(out, 4, "password", nvl(n->password));
    yaml_emit_kv_bool(out, 4, "udp", n->udp);
    yaml_emit_kv(out, 4, "network", network_name(n->transport));
    if (n->tls != NODE_TLS_NONE && n->sni && n->sni[0]) {
        yaml_emit_kv(out, 4, "sni", n->sni);
    }
    yaml_emit_kv_bool(out, 4, "tls", n->tls != NODE_TLS_NONE);
    if (n->tls == NODE_TLS_REALITY) {
        emit_reality(out, n);
    }
    if (n->transport == NODE_TRANSPORT_WS) {
        emit_ws_opts(out, n);
    }
}

int node_emit_yaml(struct strbuf *out, const struct proxy_node *n)
{
    if (!out || !n) {
        return -1;
    }
    switch (n->type) {
    case NODE_TYPE_VLESS:
        emit_vless(out, n);
        return 0;
    case NODE_TYPE_VMESS:
        emit_vmess(out, n);
        return 0;
    case NODE_TYPE_SHADOWSOCKS:
        emit_ss(out, n);
        return 0;
    case NODE_TYPE_TROJAN:
        emit_trojan(out, n);
        return 0;
    default:
        return -1;
    }
}
