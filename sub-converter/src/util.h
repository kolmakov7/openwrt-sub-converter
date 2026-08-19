#ifndef SUB_CONV_UTIL_H
#define SUB_CONV_UTIL_H

#include <stddef.h>

typedef struct strbuf {
    char *data;
    size_t len;
    size_t cap;
} strbuf;

void strbuf_init(strbuf *sb);
void strbuf_free(strbuf *sb);
int strbuf_reserve(strbuf *sb, size_t extra);
int strbuf_append_bytes(strbuf *sb, const void *data, size_t n);
int strbuf_append_str(strbuf *sb, const char *s);
int strbuf_append_char(strbuf *sb, char c);
int strbuf_appendf(strbuf *sb, const char *fmt, ...);
void strbuf_clear(strbuf *sb);

int url_decode(const char *in, size_t inlen, strbuf *out);
int url_encode(const char *in, size_t inlen, strbuf *out);

int utf8_validate(const char *s, size_t len);
int utf8_next_codepoint(const char *s, size_t len, size_t *pos);

char *str_trim(char *s);
int str_starts_with(const char *s, const char *prefix);
int str_ends_with(const char *s, const char *suffix);
int str_split(const char *s, char delim, strbuf ***out_parts, size_t *out_n);
void str_split_free(strbuf **parts, size_t n);
int str_replace_all(const char *in, const char *from, const char *to, strbuf *out);
char *xstrdup(const char *s);

#endif
