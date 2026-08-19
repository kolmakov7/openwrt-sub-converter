#ifndef SUB_CONV_BASE64_H
#define SUB_CONV_BASE64_H

#include <stddef.h>

struct strbuf;

int base64_encode(const void *data, size_t len, struct strbuf *out);
int base64_decode(const char *in, size_t inlen, struct strbuf *out);
int base64_encode_url(const void *data, size_t len, struct strbuf *out);
int base64_decode_url(const char *in, size_t inlen, struct strbuf *out);
int base64_looks_like(const char *in, size_t inlen);

#endif
