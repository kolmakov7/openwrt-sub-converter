#include "base64.h"
#include "converter.h"
#include "model.h"
#include "node_emit.h"
#include "util.h"
#include "yaml_emit.h"

#include <stdlib.h>
#include <string.h>

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

static int line_blank(const char *line, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        char c = line[i];

        if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != '\v' && c != '\f') {
            return 0;
        }
    }
    return 1;
}

static int nodes_push(proxy_node **nodes, size_t *nlen, size_t *ncap, proxy_node *node)
{
    if (*nlen == *ncap) {
        size_t ncap2 = *ncap ? *ncap * 2 : 16;
        proxy_node *nn = realloc(*nodes, ncap2 * sizeof(*nn));

        if (!nn) {
            return -1;
        }
        *nodes = nn;
        *ncap = ncap2;
    }
    (*nodes)[(*nlen)++] = *node;
    return 0;
}

static int conv_base64_run(const conv_ctx *ctx, char **out_yaml)
{
    proxy_node *nodes = NULL;
    char **used = NULL;
    size_t nlen = 0;
    size_t ncap = 0;
    size_t nused = 0;
    strbuf work;
    strbuf sb;
    char *src = NULL;
    int allocated = 0;
    size_t start;
    size_t i;
    int rc = -1;

    if (!ctx || !ctx->data || !out_yaml) {
        return -1;
    }
    strbuf_init(&work);
    strbuf_init(&sb);
    if (base64_looks_like(ctx->data, ctx->data_len)) {
        if (base64_decode(ctx->data, ctx->data_len, &work) != 0) {
            goto out;
        }
        src = work.data;
    } else {
        src = malloc(ctx->data_len + 1);
        if (!src) {
            goto out;
        }
        allocated = 1;
        memcpy(src, ctx->data, ctx->data_len);
        src[ctx->data_len] = '\0';
    }

    start = 0;
    for (i = 0;; i++) {
        if (src[i] != '\n' && src[i] != '\0') {
            continue;
        }
        if (i > start) {
            proxy_node node;

            if (!line_blank(src + start, i - start) &&
                node_parse_share(src + start, i - start, &node) == 0) {
                if (nodes_push(&nodes, &nlen, &ncap, &node) != 0) {
                    node_free(&node);
                    goto out;
                }
            }
        }
        if (src[i] == '\0') {
            break;
        }
        start = i + 1;
    }
    if (nlen == 0) {
        goto out;
    }
    for (i = 0; i < nlen; i++) {
        const char *base = nodes[i].name ? nodes[i].name : "";
        char *name = assign_unique_name(&used, &nused, base);

        if (!name) {
            goto out;
        }
        free(nodes[i].name);
        nodes[i].name = name;
    }

    yaml_emit_kv(&sb, 0, "proxies", NULL);
    for (i = 0; i < nlen; i++) {
        if (node_emit_yaml(&sb, &nodes[i]) != 0) {
            goto out;
        }
    }
    yaml_emit_kv(&sb, 0, "proxy-groups", NULL);
    yaml_emit_dash_kv(&sb, 2, "name", "PROXY");
    yaml_emit_kv(&sb, 4, "type", "select");
    yaml_emit_kv(&sb, 4, "proxies", NULL);
    for (i = 0; i < nlen; i++) {
        yaml_emit_dash(&sb, 6, nodes[i].name);
    }
    yaml_emit_dash(&sb, 6, "DIRECT");
    *out_yaml = xstrdup(sb.data);
    rc = 0;

out:
    strbuf_free(&work);
    strbuf_free(&sb);
    if (allocated) {
        free(src);
    }
    for (i = 0; i < nlen; i++) {
        node_free(&nodes[i]);
    }
    free(nodes);
    for (i = 0; i < nused; i++) {
        free(used[i]);
    }
    free(used);
    return rc;
}

const converter conv_base64 = {
    "base64",
    conv_base64_run,
};
