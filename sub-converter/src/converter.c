#include "converter.h"

#include <string.h>

extern const converter conv_singbox;
extern const converter conv_base64;

static const converter *const g_converters[] = {
    &conv_singbox,
    &conv_base64,
};

const converter *converter_find(const char *name)
{
    size_t i;

    if (!name) {
        return NULL;
    }
    for (i = 0; i < sizeof(g_converters) / sizeof(g_converters[0]); i++) {
        if (strcmp(g_converters[i]->name, name) == 0) {
            return g_converters[i];
        }
    }
    return NULL;
}

const converter *const *converter_all(void)
{
    return g_converters;
}

size_t converter_count(void)
{
    return sizeof(g_converters) / sizeof(g_converters[0]);
}