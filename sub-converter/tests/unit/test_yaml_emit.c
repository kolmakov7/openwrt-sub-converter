#include <string.h>

#include "unity.h"
#include "util.h"
#include "yaml_emit.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_kv_null_value(void)
{
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv(&sb, 0, "proxies", NULL));
    TEST_ASSERT_EQUAL_STRING("proxies:\n", sb.data);
    strbuf_free(&sb);
}

static void test_kv_plain_value(void)
{
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv(&sb, 2, "name", "TikTok | Gemini"));
    TEST_ASSERT_EQUAL_STRING("  name: TikTok | Gemini\n", sb.data);
    strbuf_free(&sb);
}

static void test_kv_quoted_value(void)
{
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv(&sb, 4, "name", "Node: America"));
    TEST_ASSERT_EQUAL_STRING("    name: \"Node: America\"\n", sb.data);
    strbuf_free(&sb);
}

static void test_kv_int(void)
{
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv_int(&sb, 0, "port", 443));
    TEST_ASSERT_EQUAL_STRING("port: 443\n", sb.data);
    strbuf_free(&sb);
}

static void test_kv_bool(void)
{
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv_bool(&sb, 0, "udp", 1));
    TEST_ASSERT_EQUAL_STRING("udp: true\n", sb.data);
    strbuf_clear(&sb);
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv_bool(&sb, 0, "udp", 0));
    TEST_ASSERT_EQUAL_STRING("udp: false\n", sb.data);
    strbuf_free(&sb);
}

static void test_dash(void)
{
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_dash(&sb, 2, "Server 1"));
    TEST_ASSERT_EQUAL_STRING("  - Server 1\n", sb.data);
    strbuf_free(&sb);
}

static void test_dash_kv(void)
{
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_dash_kv(&sb, 2, "name", "A: B"));
    TEST_ASSERT_EQUAL_STRING("  - name: \"A: B\"\n", sb.data);
    strbuf_free(&sb);
}

static void test_dash_kv_open(void)
{
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_dash_kv_open(&sb, 2, "ws-opts"));
    TEST_ASSERT_EQUAL_STRING("  - ws-opts:\n", sb.data);
    strbuf_free(&sb);
}

static void test_flow_list(void)
{
    static const char *items[] = {"a", "b", "c: d"};
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_flow_list(&sb, items, 3));
    TEST_ASSERT_EQUAL_STRING("[a, b, \"c: d\"]\n", sb.data);
    strbuf_clear(&sb);
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_flow_list(&sb, NULL, 0));
    TEST_ASSERT_EQUAL_STRING("[]\n", sb.data);
    strbuf_free(&sb);
}

static void test_full_subscription(void)
{
    static const char *expected =
        "proxies:\n"
        "  - name: TikTok | Gemini\n"
        "    type: ss\n"
        "    server: 1.2.3.4\n"
        "    port: 443\n"
        "    cipher: aes-256-gcm\n"
        "    password: secret\n"
        "proxy-groups:\n"
        "  - name: select\n"
        "    type: select\n"
        "    proxies:\n"
        "      - TikTok | Gemini\n";
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv(&sb, 0, "proxies", NULL));
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_dash_kv(&sb, 2, "name", "TikTok | Gemini"));
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv(&sb, 4, "type", "ss"));
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv(&sb, 4, "server", "1.2.3.4"));
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv_int(&sb, 4, "port", 443));
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv(&sb, 4, "cipher", "aes-256-gcm"));
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv(&sb, 4, "password", "secret"));
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv(&sb, 0, "proxy-groups", NULL));
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_dash_kv(&sb, 2, "name", "select"));
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv(&sb, 4, "type", "select"));
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_kv(&sb, 4, "proxies", NULL));
    TEST_ASSERT_EQUAL_INT(0, yaml_emit_dash(&sb, 6, "TikTok | Gemini"));
    TEST_ASSERT_EQUAL_STRING(expected, sb.data);
    strbuf_free(&sb);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_kv_null_value);
    RUN_TEST(test_kv_plain_value);
    RUN_TEST(test_kv_quoted_value);
    RUN_TEST(test_kv_int);
    RUN_TEST(test_kv_bool);
    RUN_TEST(test_dash);
    RUN_TEST(test_dash_kv);
    RUN_TEST(test_dash_kv_open);
    RUN_TEST(test_flow_list);
    RUN_TEST(test_full_subscription);
    return UNITY_END();
}
