#include "base64.h"

#include <string.h>

#include "util.h"

static const char b64_std_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char b64_url_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static int b64_is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int b64_char_val(const char *alphabet, unsigned char c)
{
    const char *p = strchr(alphabet, (char)c);

    if (!p) {
        return -1;
    }
    return (int)(p - alphabet);
}

static int b64_encode_core(const void *data, size_t len, strbuf *out,
                           const char *alphabet, int with_padding)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t i = 0;

    if (len == 0) {
        strbuf_clear(out);
        return 0;
    }
    if (!data) {
        return -1;
    }
    for (; i + 2 < len; i += 3) {
        unsigned int v = ((unsigned int)p[i] << 16) |
                         ((unsigned int)p[i + 1] << 8) |
                         (unsigned int)p[i + 2];

        strbuf_append_char(out, alphabet[(v >> 18) & 0x3F]);
        strbuf_append_char(out, alphabet[(v >> 12) & 0x3F]);
        strbuf_append_char(out, alphabet[(v >> 6) & 0x3F]);
        strbuf_append_char(out, alphabet[v & 0x3F]);
    }
    if (len - i == 1) {
        strbuf_append_char(out, alphabet[((unsigned int)p[i] >> 2) & 0x3F]);
        strbuf_append_char(out, alphabet[((unsigned int)p[i] & 0x03) << 4]);
        if (with_padding) {
            strbuf_append_str(out, "==");
        }
    } else if (len - i == 2) {
        strbuf_append_char(out, alphabet[((unsigned int)p[i] >> 2) & 0x3F]);
        strbuf_append_char(out, alphabet[(((unsigned int)p[i] & 0x03) << 4) |
                                         ((unsigned int)p[i + 1] >> 4)]);
        strbuf_append_char(out, alphabet[((unsigned int)p[i + 1] & 0x0F) << 2]);
        if (with_padding) {
            strbuf_append_char(out, '=');
        }
    }
    return 0;
}

static int b64_decode_core(const char *in, size_t inlen, strbuf *out,
                           const char *alphabet)
{
    unsigned char quad[4];
    size_t qn = 0;
    size_t pad = 0;
    size_t i;

    if (!in && inlen > 0) {
        return -1;
    }
    for (i = 0; i < inlen; i++) {
        unsigned char c = (unsigned char)in[i];
        int v;

        if (b64_is_ws(c)) {
            continue;
        }
        if (c == '=') {
            if (qn < 2 || qn > 3) {
                return -1;
            }
            pad++;
            if (qn + pad > 4) {
                return -1;
            }
            continue;
        }
        v = b64_char_val(alphabet, c);
        if (v < 0) {
            return -1;
        }
        if (pad > 0) {
            return -1;
        }
        quad[qn++] = (unsigned char)v;
        if (qn == 4) {
            strbuf_append_char(out, (char)((quad[0] << 2) | (quad[1] >> 4)));
            strbuf_append_char(out, (char)(((quad[1] & 0x0F) << 4) | (quad[2] >> 2)));
            strbuf_append_char(out, (char)(((quad[2] & 0x03) << 6) | quad[3]));
            qn = 0;
        }
    }
    if (qn == 1) {
        return -1;
    }
    if (qn == 2) {
        if (pad != 0 && pad != 2) {
            return -1;
        }
        strbuf_append_char(out, (char)((quad[0] << 2) | (quad[1] >> 4)));
    } else if (qn == 3) {
        if (pad != 0 && pad != 1) {
            return -1;
        }
        strbuf_append_char(out, (char)((quad[0] << 2) | (quad[1] >> 4)));
        strbuf_append_char(out, (char)(((quad[1] & 0x0F) << 4) | (quad[2] >> 2)));
    }
    return 0;
}

int base64_encode(const void *data, size_t len, struct strbuf *out)
{
    return b64_encode_core(data, len, out, b64_std_chars, 1);
}

int base64_decode(const char *in, size_t inlen, struct strbuf *out)
{
    return b64_decode_core(in, inlen, out, b64_std_chars);
}

int base64_encode_url(const void *data, size_t len, struct strbuf *out)
{
    return b64_encode_core(data, len, out, b64_url_chars, 0);
}

int base64_decode_url(const char *in, size_t inlen, struct strbuf *out)
{
    return b64_decode_core(in, inlen, out, b64_url_chars);
}

int base64_looks_like(const char *in, size_t inlen)
{
    size_t n = 0;
    size_t i;

    if (!in) {
        return 0;
    }
    for (i = 0; i < inlen; i++) {
        unsigned char c = (unsigned char)in[i];

        if (b64_is_ws(c)) {
            continue;
        }
        if (c != '=' && b64_char_val(b64_std_chars, c) < 0) {
            return 0;
        }
        n++;
    }
    return n >= 4 && n % 4 == 0;
}
