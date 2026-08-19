#ifndef SUB_CONV_NODE_EMIT_H
#define SUB_CONV_NODE_EMIT_H

struct strbuf;
struct proxy_node;

/* Emit one mihomo proxy block ("  - name: ...\n" plus keys at indent 4..) into out.
   Returns 0 on success. */
int node_emit_yaml(struct strbuf *out, const struct proxy_node *n);

#endif
