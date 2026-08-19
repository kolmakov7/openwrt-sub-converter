#ifndef SUB_CONV_CONVERTER_H
#define SUB_CONV_CONVERTER_H

#include <stddef.h>

typedef struct conv_ctx {
    const void *data;
    size_t data_len;
} conv_ctx;

typedef struct converter {
    const char *name;
    int (*run)(const conv_ctx *ctx, char **out_yaml);
} converter;

const converter *converter_find(const char *name);
const converter *const *converter_all(void);
size_t converter_count(void);

#endif