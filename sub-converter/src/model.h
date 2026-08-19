#ifndef SUB_CONV_MODEL_H
#define SUB_CONV_MODEL_H

#include <stddef.h>

typedef enum {
    NODE_TYPE_UNKNOWN = 0,
    NODE_TYPE_VLESS,
    NODE_TYPE_VMESS,
    NODE_TYPE_SHADOWSOCKS,
    NODE_TYPE_TROJAN,
    NODE_TYPE_SOCKS5,
    NODE_TYPE_HTTP,
    NODE_TYPE_HYSTERIA2
} node_type;

typedef enum {
    NODE_TRANSPORT_NONE = 0,
    NODE_TRANSPORT_WS,
    NODE_TRANSPORT_XHTTP,
    NODE_TRANSPORT_GRPC,
    NODE_TRANSPORT_HTTP
} node_transport;

typedef enum {
    NODE_TLS_NONE = 0,
    NODE_TLS_TLS,
    NODE_TLS_REALITY
} node_tls;

typedef struct proxy_node {
    node_type type;
    char *name;
    char *server;
    int port;
    char *uuid;
    char *password;
    char *cipher;
    int alter_id;
    int udp;
    node_transport transport;
    node_tls tls;
    char *sni;
    char *alpn;
    char *fp;
    char *reality_pbk;
    char *reality_sid;
    char *ws_path;
    char *ws_host;
    char *xhttp_host;
    char *xhttp_path;
    char *xhttp_mode;
    char *grpc_service;
    char *plugin;
    char *plugin_opts;
} proxy_node;

void node_init(proxy_node *n);
void node_free(proxy_node *n);
void node_clear(proxy_node *n);
proxy_node *node_new(void);

const char *node_type_name(node_type t);
const char *node_transport_name(node_transport t);

/* Parse a share link (vless://, vmess://, ss://, trojan://) into n.
   On success returns 0 and fills n (caller must node_free it after use).
   On error returns -1 and leaves n initialized-but-empty (still safe to node_free). */
int node_parse_share(const char *line, size_t len, proxy_node *n);

#endif
