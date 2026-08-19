#ifndef SUB_CONV_YAML_EMIT_H
#define SUB_CONV_YAML_EMIT_H

#include <stddef.h>
#include <stdint.h>

struct strbuf;

/* Quote/escape a scalar so it is valid YAML regardless of content.
   Uses plain (unquoted) form when safe, otherwise double-quoted with escapes.
   UTF-8 bytes pass through as-is inside double quotes. Returns 0 on success. */
int yaml_quote(const char *s, size_t len, struct strbuf *out);

/* Emit "key: value\n" at given indent (spaces). value is quoted via yaml_quote.
   If value==NULL emits "key:\n" (block opener). Returns 0 on success. */
int yaml_emit_kv(struct strbuf *out, int indent, const char *key, const char *value);

/* Emit "key: <int>\n". */
int yaml_emit_kv_int(struct strbuf *out, int indent, const char *key, int64_t value);

/* Emit "key: true\n" / "key: false\n". */
int yaml_emit_kv_bool(struct strbuf *out, int indent, const char *key, int value);

/* Emit a list item scalar "- value\n" (quoted). */
int yaml_emit_dash(struct strbuf *out, int indent, const char *value);

/* Emit "- key: value\n" (mapping entry as list item). */
int yaml_emit_dash_kv(struct strbuf *out, int indent, const char *key, const char *value);

/* Emit "- key:\n" (block opener as list item). */
int yaml_emit_dash_kv_open(struct strbuf *out, int indent, const char *key);

/* Emit a flow sequence "[a, b, c]\n"; each item quoted via yaml_quote. */
int yaml_emit_flow_list(struct strbuf *out, const char *const *items, size_t n);

#endif
