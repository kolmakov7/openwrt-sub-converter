#include "converter.h"
#include "jsonx.h"
#include "model.h"
#include "node_emit.h"
#include "util.h"
#include "yaml_emit.h"

#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>

typedef struct elem_meta {
    size_t first;
    size_t count;
    char *group_name;
    int has_balancer;
    char *balancer_tag;
} elem_meta;

static int parse_vless_outbound(struct json_object *ob, proxy_node *n, const char **out_tag)
{
    struct json_object *settings;
    struct json_object *vnext;
    struct json_object *v0;
    struct json_object *users;
    struct json_object *u0;
    struct json_object *ss;
    struct json_object *ws;
    struct json_object *xh;
    struct json_object *re;
    struct json_object *tls;
    const char *proto = NULL;
    const char *tag = NULL;
    const char *address = NULL;
    const char *id = NULL;
    const char *network = NULL;
    const char *security = NULL;
    int64_t port;

    if (jsonx_get_string(ob, "protocol", &proto) != 0 ||
        strcmp(proto, "vless") != 0) {
        return -1;
    }
    jsonx_get_string(ob, "tag", &tag);
    if (jsonx_get_obj(ob, "settings", &settings) != 0) {
        return -1;
    }
    vnext = jsonx_obj_get(settings, "vnext");
    if (!vnext || !jsonx_is_array(vnext)) {
        return -1;
    }
    v0 = jsonx_arr_get(vnext, 0);
    if (!v0 || !jsonx_is_object(v0)) {
        return -1;
    }
    if (jsonx_get_string(v0, "address", &address) != 0 ||
        jsonx_get_int(v0, "port", &port) != 0) {
        return -1;
    }
    users = jsonx_obj_get(v0, "users");
    if (!users || !jsonx_is_array(users)) {
        return -1;
    }
    u0 = jsonx_arr_get(users, 0);
    if (!u0 || !jsonx_is_object(u0) || jsonx_get_string(u0, "id", &id) != 0) {
        return -1;
    }

    node_init(n);
    n->type = NODE_TYPE_VLESS;
    n->udp = 1;
    n->server = xstrdup(address);
    n->port = (int)port;
    n->uuid = xstrdup(id);

    ss = jsonx_obj_get(ob, "streamSettings");
    if (ss && jsonx_is_object(ss)) {
        jsonx_get_string(ss, "network", &network);
        jsonx_get_string(ss, "security", &security);
        if (security) {
            if (strcmp(security, "reality") == 0) {
                n->tls = NODE_TLS_REALITY;
            } else if (strcmp(security, "tls") == 0) {
                n->tls = NODE_TLS_TLS;
            }
        }
        if (network) {
            if (strcmp(network, "ws") == 0) {
                n->transport = NODE_TRANSPORT_WS;
            } else if (strcmp(network, "xhttp") == 0) {
                n->transport = NODE_TRANSPORT_XHTTP;
            }
        }
        if (jsonx_get_obj(ss, "wsSettings", &ws) == 0) {
            struct json_object *p = jsonx_obj_get(ws, "path");
            struct json_object *hdrs = jsonx_obj_get(ws, "headers");
            const char *host = "";

            n->ws_path = xstrdup(p && jsonx_is_string(p) ? jsonx_str(p) : "");
            if (hdrs && jsonx_is_object(hdrs)) {
                const char *h = NULL;

                if (jsonx_get_string(hdrs, "Host", &h) == 0) {
                    host = h;
                }
            }
            n->ws_host = xstrdup(host);
        }
        if (jsonx_get_obj(ss, "xhttpSettings", &xh) == 0) {
            const char *host = "";
            const char *path = "";
            const char *mode = "";

            jsonx_get_string(xh, "host", &host);
            jsonx_get_string(xh, "path", &path);
            jsonx_get_string(xh, "mode", &mode);
            n->xhttp_host = xstrdup(host);
            n->xhttp_path = xstrdup(path);
            n->xhttp_mode = xstrdup(mode);
        }
        if (jsonx_get_obj(ss, "realitySettings", &re) == 0) {
            const char *sn = "";
            const char *pk = "";
            const char *sid = "";
            const char *fp = "";

            jsonx_get_string(re, "serverName", &sn);
            jsonx_get_string(re, "publicKey", &pk);
            jsonx_get_string(re, "shortId", &sid);
            jsonx_get_string(re, "fingerprint", &fp);
            n->sni = xstrdup(sn);
            n->reality_pbk = xstrdup(pk);
            n->reality_sid = xstrdup(sid);
            n->fp = xstrdup(fp);
        }
        if (jsonx_get_obj(ss, "tlsSettings", &tls) == 0) {
            const char *sn = "";

            jsonx_get_string(tls, "serverName", &sn);
            n->sni = xstrdup(sn);
        }
    }
    if (out_tag) {
        *out_tag = tag;
    }
    return 0;
}

static int name_used(char *const *used, size_t nused, const char *name)
{
    size_t i;

    for (i = 0; i < nused; i++) {
        if (strcmp(used[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static char *assign_unique_name(char ***usedp, size_t *nused, const char *base)
{
    char **used = *usedp;
    size_t n = *nused;
    int suffix = 2;
    char *cand = xstrdup(base);
    char **nu;

    while (name_used(used, n, cand)) {
        strbuf sb;
        char *next;

        strbuf_init(&sb);
        strbuf_appendf(&sb, "%s - %d", base, suffix++);
        next = xstrdup(sb.data);
        strbuf_free(&sb);
        free(cand);
        cand = next;
    }
    nu = realloc(used, (n + 1) * sizeof(*nu));
    if (!nu) {
        free(cand);
        return NULL;
    }
    *usedp = nu;
    (*usedp)[n] = xstrdup(cand);
    *nused = n + 1;
    return cand;
}

static int conv_singbox_run(const conv_ctx *ctx, char **out_yaml)
{
    struct json_object *root = NULL;
    proxy_node *nodes = NULL;
    char **tags = NULL;
    size_t nlen = 0;
    size_t ncap = 0;
    elem_meta *meta = NULL;
    char **used = NULL;
    size_t nused = 0;
    int nelem = 0;
    int i;
    size_t j;
    strbuf sb;
    int rc = -1;

    if (!ctx || !ctx->data || !out_yaml) {
        return -1;
    }
    strbuf_init(&sb);
    root = jsonx_parse(ctx->data, ctx->data_len);
    if (!root || !jsonx_is_array(root)) {
        goto out;
    }
    nelem = jsonx_arr_len(root);
    if (nelem > 0) {
        meta = calloc((size_t)nelem, sizeof(*meta));
        if (!meta) {
            goto out;
        }
    }
    for (i = 0; i < nelem; i++) {
        struct json_object *elem = jsonx_arr_get(root, i);
        struct json_object *obs = NULL;
        const char *remarks = "";
        int k;
        int klen;

        if (!elem || !jsonx_is_object(elem)) {
            continue;
        }
        jsonx_get_string(elem, "remarks", &remarks);
        meta[i].group_name = xstrdup(remarks);
        meta[i].first = nlen;
        obs = jsonx_obj_get(elem, "outbounds");
        if (obs && jsonx_is_array(obs)) {
            klen = jsonx_arr_len(obs);
            for (k = 0; k < klen; k++) {
                struct json_object *ob = jsonx_arr_get(obs, k);
                proxy_node node;
                const char *tag = NULL;

                if (!ob || !jsonx_is_object(ob)) {
                    continue;
                }
                if (parse_vless_outbound(ob, &node, &tag) != 0) {
                    continue;
                }
                if (nlen == ncap) {
                    size_t ncap2 = ncap ? ncap * 2 : 16;
                    proxy_node *nn = realloc(nodes, ncap2 * sizeof(*nn));
                    char **nt;

                    if (!nn) {
                        node_free(&node);
                        goto out;
                    }
                    nodes = nn;
                    nt = realloc(tags, ncap2 * sizeof(*nt));
                    if (!nt) {
                        node_free(&node);
                        goto out;
                    }
                    tags = nt;
                    ncap = ncap2;
                }
                nodes[nlen] = node;
                tags[nlen] = xstrdup(tag ? tag : "");
                nlen++;
            }
        }
        meta[i].count = nlen - meta[i].first;
        if (jsonx_get_obj(elem, "routing", &obs) == 0 && jsonx_is_object(obs)) {
            struct json_object *bals = NULL;

            bals = jsonx_obj_get(obs, "balancers");
            if (bals && jsonx_is_array(bals)) {
                int bi;
                int bn = jsonx_arr_len(bals);

                for (bi = 0; bi < bn; bi++) {
                    struct json_object *b = jsonx_arr_get(bals, bi);
                    struct json_object *strategy = NULL;
                    const char *type = NULL;
                    const char *btag = NULL;

                    if (b && jsonx_is_object(b) &&
                        jsonx_get_obj(b, "strategy", &strategy) == 0 &&
                        jsonx_get_string(strategy, "type", &type) == 0 &&
                        strcmp(type, "leastPing") == 0) {
                        jsonx_get_string(b, "tag", &btag);
                        meta[i].has_balancer = 1;
                        meta[i].balancer_tag = xstrdup(btag ? btag : "");
                        break;
                    }
                }
            }
        }
    }
    if (nlen == 0) {
        goto out;
    }
    for (i = 0; i < nelem; i++) {
        if (meta[i].count == 0) {
            continue;
        }
        if (meta[i].count == 1) {
            size_t idx = meta[i].first;

            nodes[idx].name = assign_unique_name(&used, &nused, meta[i].group_name);
        } else {
            for (j = meta[i].first; j < meta[i].first + meta[i].count; j++) {
                strbuf nm;
                char *base;
                char *name;

                strbuf_init(&nm);
                strbuf_appendf(&nm, "%s - %s", meta[i].group_name, tags[j]);
                base = xstrdup(nm.data);
                strbuf_free(&nm);
                name = assign_unique_name(&used, &nused, base);
                free(base);
                nodes[j].name = name;
            }
        }
    }
    yaml_emit_kv(&sb, 0, "proxies", NULL);
    for (j = 0; j < nlen; j++) {
        node_emit_yaml(&sb, &nodes[j]);
    }
    yaml_emit_kv(&sb, 0, "proxy-groups", NULL);
    for (i = 0; i < nelem; i++) {
        if (meta[i].count == 0) {
            continue;
        }
        yaml_emit_dash_kv(&sb, 2, "name", meta[i].group_name);
        yaml_emit_kv(&sb, 4, "type", "select");
        yaml_emit_kv(&sb, 4, "proxies", NULL);
        for (j = meta[i].first; j < meta[i].first + meta[i].count; j++) {
            yaml_emit_dash(&sb, 6, nodes[j].name);
        }
        yaml_emit_dash(&sb, 6, "DIRECT");
        if (meta[i].has_balancer) {
            strbuf gn;

            strbuf_init(&gn);
            strbuf_appendf(&gn, "%s - %s", meta[i].group_name, meta[i].balancer_tag);
            yaml_emit_dash_kv(&sb, 2, "name", gn.data);
            yaml_emit_kv(&sb, 4, "type", "url-test");
            yaml_emit_kv(&sb, 4, "url", "http://www.gstatic.com/generate_204");
            yaml_emit_kv_int(&sb, 4, "interval", 300);
            yaml_emit_kv_int(&sb, 4, "tolerance", 50);
            yaml_emit_kv(&sb, 4, "proxies", NULL);
            for (j = meta[i].first; j < meta[i].first + meta[i].count; j++) {
                yaml_emit_dash(&sb, 6, nodes[j].name);
            }
            strbuf_free(&gn);
        }
    }
    yaml_emit_dash_kv(&sb, 2, "name", "PROXY");
    yaml_emit_kv(&sb, 4, "type", "select");
    yaml_emit_kv(&sb, 4, "proxies", NULL);
    for (i = 0; i < nelem; i++) {
        if (meta[i].count == 0) {
            continue;
        }
        yaml_emit_dash(&sb, 6, meta[i].group_name);
    }
    yaml_emit_dash(&sb, 6, "DIRECT");
    *out_yaml = xstrdup(sb.data);
    rc = 0;

out:
    json_object_put(root);
    strbuf_free(&sb);
    for (j = 0; j < nlen; j++) {
        node_free(&nodes[j]);
        free(tags[j]);
    }
    free(nodes);
    free(tags);
    if (meta) {
        for (i = 0; i < nelem; i++) {
            free(meta[i].group_name);
            free(meta[i].balancer_tag);
        }
        free(meta);
    }
    for (j = 0; j < nused; j++) {
        free(used[j]);
    }
    free(used);
    return rc;
}

const converter conv_singbox = {
    "singbox",
    conv_singbox_run,
};