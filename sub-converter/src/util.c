#include "util.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void oom_abort(void)
{
    fputs("sub-converter: out of memory\n", stderr);
    abort();
}

static int strbuf_grow(strbuf *sb, size_t need)
{
    size_t ncap;
    char *nd;

    if (need <= sb->cap) {
        return 0;
    }
    ncap = sb->cap ? sb->cap : 32;
    while (ncap < need) {
        if (ncap > SIZE_MAX / 2) {
            oom_abort();
        }
        ncap *= 2;
    }
    nd = realloc(sb->data, ncap);
    if (!nd) {
        oom_abort();
    }
    sb->data = nd;
    sb->cap = ncap;
    return 0;
}

void strbuf_init(strbuf *sb)
{
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

void strbuf_free(strbuf *sb)
{
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

int strbuf_reserve(strbuf *sb, size_t extra)
{
    if (extra >= SIZE_MAX - sb->len) {
        oom_abort();
    }
    return strbuf_grow(sb, sb->len + extra + 1);
}

int strbuf_append_bytes(strbuf *sb, const void *data, size_t n)
{
    strbuf_reserve(sb, n);
    if (n) {
        memcpy(sb->data + sb->len, data, n);
    }
    sb->len += n;
    sb->data[sb->len] = '\0';
    return 0;
}

int strbuf_append_str(strbuf *sb, const char *s)
{
    return strbuf_append_bytes(sb, s, strlen(s));
}

int strbuf_append_char(strbuf *sb, char c)
{
    return strbuf_append_bytes(sb, &c, 1);
}

int strbuf_appendf(strbuf *sb, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) {
        return -1;
    }
    strbuf_reserve(sb, (size_t)n);
    va_start(ap, fmt);
    vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, ap);
    va_end(ap);
    sb->len += (size_t)n;
    return n;
}

void strbuf_clear(strbuf *sb)
{
    sb->len = 0;
    if (sb->data) {
        sb->data[0] = '\0';
    }
}

static int hex_val(unsigned char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

int url_decode(const char *in, size_t inlen, strbuf *out)
{
    size_t i;

    for (i = 0; i < inlen; i++) {
        unsigned char c = (unsigned char)in[i];

        if (c == '%') {
            int hi, lo;

            if (i + 2 >= inlen) {
                return -1;
            }
            hi = hex_val((unsigned char)in[i + 1]);
            lo = hex_val((unsigned char)in[i + 2]);
            if (hi < 0 || lo < 0) {
                return -1;
            }
            strbuf_append_char(out, (char)((hi << 4) | lo));
            i += 2;
        } else {
            strbuf_append_char(out, (char)c);
        }
    }
    return 0;
}

static int is_unreserved(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') {
        return 1;
    }
    if (c >= 'a' && c <= 'z') {
        return 1;
    }
    if (c >= '0' && c <= '9') {
        return 1;
    }
    return c == '-' || c == '_' || c == '.' || c == '~';
}

int url_encode(const char *in, size_t inlen, strbuf *out)
{
    static const char hexdig[] = "0123456789ABCDEF";
    size_t i;

    for (i = 0; i < inlen; i++) {
        unsigned char c = (unsigned char)in[i];

        if (is_unreserved(c)) {
            strbuf_append_char(out, (char)c);
        } else {
            strbuf_append_char(out, '%');
            strbuf_append_char(out, hexdig[c >> 4]);
            strbuf_append_char(out, hexdig[c & 15]);
        }
    }
    return 0;
}

int utf8_next_codepoint(const char *s, size_t len, size_t *pos)
{
    size_t i = *pos;
    unsigned char c;
    unsigned long v;
    size_t extra;
    size_t mincp;
    size_t j;

    if (i >= len) {
        return -1;
    }
    c = (unsigned char)s[i];
    if (c < 0x80) {
        v = c;
        extra = 0;
        mincp = 0;
    } else if ((c & 0xE0) == 0xC0) {
        v = c & 0x1F;
        extra = 1;
        mincp = 0x80;
    } else if ((c & 0xF0) == 0xE0) {
        v = c & 0x0F;
        extra = 2;
        mincp = 0x800;
    } else if ((c & 0xF8) == 0xF0) {
        v = c & 0x07;
        extra = 3;
        mincp = 0x10000;
    } else {
        return -1;
    }
    if (i + extra >= len) {
        return -1;
    }
    for (j = 1; j <= extra; j++) {
        unsigned char cc = (unsigned char)s[i + j];

        if ((cc & 0xC0) != 0x80) {
            return -1;
        }
        v = (v << 6) | (cc & 0x3F);
    }
    if (v < mincp) {
        return -1;
    }
    if (v > 0x10FFFF) {
        return -1;
    }
    if (v >= 0xD800 && v <= 0xDFFF) {
        return -1;
    }
    *pos = i + extra + 1;
    return 0;
}

int utf8_validate(const char *s, size_t len)
{
    size_t pos = 0;

    while (pos < len) {
        if (utf8_next_codepoint(s, len, &pos) < 0) {
            return 0;
        }
    }
    return 1;
}

static int is_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

char *str_trim(char *s)
{
    char *start = s;
    char *end;

    if (!s) {
        return NULL;
    }
    while (*start && is_space((unsigned char)*start)) {
        start++;
    }
    end = start + strlen(start);
    while (end > start && is_space((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
    if (start != s) {
        memmove(s, start, (size_t)(end - start) + 1);
    }
    return s;
}

int str_starts_with(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

int str_ends_with(const char *s, const char *suffix)
{
    size_t sl = strlen(s);
    size_t pl = strlen(suffix);

    if (pl > sl) {
        return 0;
    }
    return memcmp(s + sl - pl, suffix, pl) == 0;
}

int str_split(const char *s, char delim, strbuf ***out_parts, size_t *out_n)
{
    size_t nparts = 0;
    size_t i;
    size_t start;
    strbuf **parts;

    if (!s || !out_parts || !out_n) {
        return -1;
    }
    *out_parts = NULL;
    *out_n = 0;

    for (i = 0; s[i]; i++) {
        if (s[i] == delim) {
            nparts++;
        }
    }
    nparts++;
    parts = calloc(nparts, sizeof(strbuf *));
    if (!parts) {
        oom_abort();
    }

    nparts = 0;
    start = 0;
    for (i = 0;; i++) {
        if (s[i] == delim || s[i] == '\0') {
            size_t plen = i - start;
            strbuf *p = calloc(1, sizeof(strbuf));

            if (!p) {
                oom_abort();
            }
            strbuf_init(p);
            strbuf_append_bytes(p, s + start, plen);
            parts[nparts++] = p;
            if (s[i] == '\0') {
                break;
            }
            start = i + 1;
        }
    }

    *out_parts = parts;
    *out_n = nparts;
    return 0;
}

void str_split_free(strbuf **parts, size_t n)
{
    size_t i;

    if (!parts) {
        return;
    }
    for (i = 0; i < n; i++) {
        strbuf_free(parts[i]);
        free(parts[i]);
    }
    free(parts);
}

int str_replace_all(const char *in, const char *from, const char *to, strbuf *out)
{
    size_t inlen = strlen(in);
    size_t flen = strlen(from);
    size_t tlen = strlen(to);
    size_t i = 0;

    if (flen == 0) {
        strbuf_append_str(out, in);
        return 0;
    }
    while (i < inlen) {
        if (inlen - i >= flen && memcmp(in + i, from, flen) == 0) {
            strbuf_append_bytes(out, to, tlen);
            i += flen;
        } else {
            strbuf_append_char(out, in[i]);
            i++;
        }
    }
    return 0;
}

char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);

    if (!p) {
        oom_abort();
    }
    memcpy(p, s, n);
    return p;
}
