#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "version.h"

int main(void)
{
    const char *v = subconv_version();
    assert(v != NULL);
    assert(v[0] != '\0');
    printf("OK: subconv_version() = \"%s\"\n", v);
    return 0;
}
