#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "converter.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static char *read_whole_file(const char *path, size_t *out_len)
{
    FILE *f;
    long sz;
    char *buf;
    size_t rd;

    f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) {
        free(buf);
        return NULL;
    }
    buf[rd] = '\0';
    if (out_len) {
        *out_len = rd;
    }
    return buf;
}

static void test_registry(void)
{
    const converter *c;

    c = converter_find("singbox");
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQUAL_STRING("singbox", c->name);
    TEST_ASSERT_NULL(converter_find("nope"));
    TEST_ASSERT_NULL(converter_find(NULL));
    TEST_ASSERT_EQUAL_INT(2, (int)converter_count());
    TEST_ASSERT_NOT_NULL(converter_all());
    TEST_ASSERT_EQUAL_STRING("singbox", converter_all()[0]->name);
}

static void test_golden(void)
{
    const converter *c;
    char *in;
    char *exp;
    conv_ctx ctx;
    char *out = NULL;

    in = read_whole_file(SUB_CONV_SOURCE_DIR "/tests/fixtures/singbox/example.json", NULL);
    exp = read_whole_file(SUB_CONV_SOURCE_DIR "/tests/fixtures/expected/singbox.yaml", NULL);
    TEST_ASSERT_NOT_NULL(in);
    TEST_ASSERT_NOT_NULL(exp);
    c = converter_find("singbox");
    TEST_ASSERT_NOT_NULL(c);
    ctx.data = in;
    ctx.data_len = strlen(in);
    TEST_ASSERT_EQUAL_INT(0, c->run(&ctx, &out));
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_STRING(exp, out);
    free(out);
    free(exp);
    free(in);
}

static void test_synthetic(void)
{
    static const char *json =
        "[{\"remarks\":\"A\",\"outbounds\":[{\"tag\":\"proxy\",\"protocol\":\"vless\","
        "\"settings\":{\"vnext\":[{\"address\":\"h1.example.com\",\"port\":443,"
        "\"users\":[{\"id\":\"u-1\",\"encryption\":\"none\",\"flow\":\"\"}]}]},"
        "\"streamSettings\":{\"network\":\"ws\",\"wsSettings\":{\"path\":\"/w\","
        "\"headers\":{\"Host\":\"h1.example.com\"}},\"security\":\"none\"}}]}]";
    const converter *c;
    conv_ctx ctx;
    char *out = NULL;
    char *pos;

    c = converter_find("singbox");
    TEST_ASSERT_NOT_NULL(c);
    ctx.data = json;
    ctx.data_len = strlen(json);
    TEST_ASSERT_EQUAL_INT(0, c->run(&ctx, &out));
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_NOT_NULL(strstr(out, "type: vless"));
    TEST_ASSERT_NOT_NULL(strstr(out, "server: h1.example.com"));
    TEST_ASSERT_NOT_NULL(strstr(out, "port: 443"));
    TEST_ASSERT_NOT_NULL(strstr(out, "name: A\n"));
    pos = strstr(out,
                 "  - name: A\n"
                 "    type: select\n"
                 "    proxies:\n"
                 "      - A\n"
                 "      - DIRECT\n");
    TEST_ASSERT_NOT_NULL(pos);
    pos = strstr(out,
                 "  - name: PROXY\n"
                 "    type: select\n"
                 "    proxies:\n"
                 "      - A\n"
                 "      - DIRECT\n");
    TEST_ASSERT_NOT_NULL(pos);
    free(out);
}

static void test_errors(void)
{
    const converter *c;
    conv_ctx ctx;
    char *out = NULL;

    c = converter_find("singbox");
    TEST_ASSERT_NOT_NULL(c);

    ctx.data = "{\"not\":\"array\"}";
    ctx.data_len = strlen((const char *)ctx.data);
    TEST_ASSERT_EQUAL_INT(-1, c->run(&ctx, &out));

    ctx.data = "garbage!!";
    ctx.data_len = strlen((const char *)ctx.data);
    TEST_ASSERT_EQUAL_INT(-1, c->run(&ctx, &out));

    ctx.data = "[]";
    ctx.data_len = strlen((const char *)ctx.data);
    TEST_ASSERT_EQUAL_INT(-1, c->run(&ctx, &out));

    ctx.data = NULL;
    ctx.data_len = 0;
    TEST_ASSERT_EQUAL_INT(-1, c->run(&ctx, &out));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_registry);
    RUN_TEST(test_golden);
    RUN_TEST(test_synthetic);
    RUN_TEST(test_errors);
    return UNITY_END();
}