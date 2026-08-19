#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "base64.h"
#include "converter.h"
#include "util.h"

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

    c = converter_find("base64");
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQUAL_STRING("base64", c->name);
    TEST_ASSERT_NULL(converter_find("nope"));
    TEST_ASSERT_NULL(converter_find(NULL));
}

static void test_golden(void)
{
    const converter *c;
    char *in;
    char *exp;
    conv_ctx ctx;
    char *out = NULL;

    in = read_whole_file(SUB_CONV_SOURCE_DIR "/tests/fixtures/base64/raw.txt", NULL);
    exp = read_whole_file(SUB_CONV_SOURCE_DIR "/tests/fixtures/expected/base64.yaml", NULL);
    TEST_ASSERT_NOT_NULL(in);
    TEST_ASSERT_NOT_NULL(exp);
    c = converter_find("base64");
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

static void test_plain_list(void)
{
    static const char *input =
        "vless://00000000-0000-4000-8000-000000000001@h1.example.com:443?"
        "encryption=none&type=ws&security=none#Node%20A\n"
        "ss://aes-256-gcm:pw@2.3.4.5:8388#SS%20Node\n";
    const converter *c;
    conv_ctx ctx;
    char *out = NULL;

    c = converter_find("base64");
    TEST_ASSERT_NOT_NULL(c);
    ctx.data = input;
    ctx.data_len = strlen(input);
    TEST_ASSERT_EQUAL_INT(0, c->run(&ctx, &out));
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_NOT_NULL(strstr(out, "type: vless"));
    TEST_ASSERT_NOT_NULL(strstr(out, "type: ss"));
    TEST_ASSERT_NOT_NULL(strstr(out, "name: Node A\n"));
    TEST_ASSERT_NOT_NULL(strstr(out, "name: SS Node\n"));
    TEST_ASSERT_NOT_NULL(strstr(out,
        "  - name: PROXY\n"
        "    type: select\n"
        "    proxies:\n"
        "      - Node A\n"
        "      - SS Node\n"
        "      - DIRECT\n"));
    free(out);
}

static void test_vmess_trojan(void)
{
    static const char *json =
        "{\"v\":\"2\",\"ps\":\"V%20Node\",\"add\":\"vm.example.com\",\"port\":\"443\","
        "\"id\":\"id1\",\"aid\":\"0\",\"net\":\"ws\",\"type\":\"none\","
        "\"host\":\"cdn.example.com\",\"path\":\"/w\",\"tls\":\"tls\","
        "\"sni\":\"cdn.example.com\"}";
    strbuf enc;
    strbuf input;
    const converter *c;
    conv_ctx ctx;
    char *out = NULL;

    strbuf_init(&enc);
    strbuf_init(&input);
    TEST_ASSERT_EQUAL_INT(0, base64_encode(json, strlen(json), &enc));
    strbuf_append_str(&input, "vmess://");
    strbuf_append_bytes(&input, enc.data, enc.len);
    strbuf_append_str(&input,
        "\ntrojan://tpw@t.example.com:443?security=tls&sni=t.example.com#T%20Node\n");

    c = converter_find("base64");
    TEST_ASSERT_NOT_NULL(c);
    ctx.data = input.data;
    ctx.data_len = input.len;
    TEST_ASSERT_EQUAL_INT(0, c->run(&ctx, &out));
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_NOT_NULL(strstr(out, "type: vmess"));
    TEST_ASSERT_NOT_NULL(strstr(out, "alterId: 0"));
    TEST_ASSERT_NOT_NULL(strstr(out, "cipher: auto"));
    TEST_ASSERT_NOT_NULL(strstr(out, "type: trojan"));
    TEST_ASSERT_NOT_NULL(strstr(out, "password: tpw"));
    TEST_ASSERT_NOT_NULL(strstr(out, "sni: t.example.com"));
    TEST_ASSERT_NOT_NULL(strstr(out, "tls: true"));
    free(out);
    strbuf_free(&enc);
    strbuf_free(&input);
}

static void test_b64_blob(void)
{
    static const char *link = "vless://u@h:443?encryption=none&type=ws&security=none#B64%20Node";
    strbuf enc;
    strbuf input;
    const converter *c;
    conv_ctx ctx;
    char *out = NULL;

    strbuf_init(&enc);
    strbuf_init(&input);
    TEST_ASSERT_EQUAL_INT(0, base64_encode(link, strlen(link), &enc));
    strbuf_append_bytes(&input, enc.data, enc.len);

    c = converter_find("base64");
    TEST_ASSERT_NOT_NULL(c);
    ctx.data = input.data;
    ctx.data_len = input.len;
    TEST_ASSERT_EQUAL_INT(0, c->run(&ctx, &out));
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_NOT_NULL(strstr(out, "name: B64 Node\n"));
    free(out);
    strbuf_free(&enc);
    strbuf_free(&input);
}

static void test_errors(void)
{
    const converter *c;
    conv_ctx ctx;
    char *out = NULL;

    c = converter_find("base64");
    TEST_ASSERT_NOT_NULL(c);

    ctx.data = "!!!not a link";
    ctx.data_len = strlen((const char *)ctx.data);
    TEST_ASSERT_EQUAL_INT(-1, c->run(&ctx, &out));

    ctx.data = "";
    ctx.data_len = 0;
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
    RUN_TEST(test_plain_list);
    RUN_TEST(test_vmess_trojan);
    RUN_TEST(test_b64_blob);
    RUN_TEST(test_errors);
    return UNITY_END();
}
