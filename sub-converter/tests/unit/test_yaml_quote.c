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

static void assert_quote(const char *s, size_t len, const char *expected)
{
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, yaml_quote(s, len, &sb));
    TEST_ASSERT_EQUAL_STRING(expected, sb.data);
    strbuf_free(&sb);
}

static void assert_quote_str(const char *s, const char *expected)
{
    assert_quote(s, strlen(s), expected);
}

static void test_plain_pipe_in_middle(void)
{
    assert_quote_str("TikTok | Gemini", "TikTok | Gemini");
}

static void test_colon_space(void)
{
    assert_quote_str("Node: America", "\"Node: America\"");
}

static void test_space_hash(void)
{
    assert_quote_str("abc #def", "\"abc #def\"");
}

static void test_leading_indicators(void)
{
    assert_quote_str("#start", "\"#start\"");
    assert_quote_str("-foo", "\"-foo\"");
}

static void test_resolved_scalars(void)
{
    assert_quote_str("123", "\"123\"");
    assert_quote_str("-1.5", "\"-1.5\"");
    assert_quote_str("1e10", "\"1e10\"");
    assert_quote_str("0x1F", "\"0x1F\"");
    assert_quote_str("true", "\"true\"");
    assert_quote_str("false", "\"false\"");
    assert_quote_str("YES", "\"YES\"");
    assert_quote_str("null", "\"null\"");
    assert_quote_str("~", "\"~\"");
    assert_quote_str("2024-01-15", "\"2024-01-15\"");
    assert_quote_str(".", "\".\"");
    assert_quote_str("..", "\"..\"");
}

static void test_empty(void)
{
    assert_quote("", 0, "\"\"");
    assert_quote(NULL, 0, "\"\"");
}

static void test_plain_name(void)
{
    assert_quote_str("plain-name", "plain-name");
}

static void test_whitespace_edges(void)
{
    assert_quote_str(" with space", "\" with space\"");
    assert_quote_str("with space ", "\"with space \"");
    assert_quote_str("tab\tend", "\"tab\\tend\"");
}

static void test_emoji_plain(void)
{
    assert_quote_str("\xF0\x9F\x87\xAA\xF0\x9F\x87\xBA \xD0\x90\xD0\xB2\xD1\x82\xD0\xBE",
                     "\xF0\x9F\x87\xAA\xF0\x9F\x87\xBA \xD0\x90\xD0\xB2\xD1\x82\xD0\xBE");
    assert_quote_str("\xF0\x9F\x87\xAA\xF0\x9F\x87\xAA \xF0\x9F\x8E\xAE \xD0\xAD\xD1\x81\xD1\x82\xD0\xBE\xD0\xBD\xD0\xB8\xD1\x8F \xE2\x99\xBE\xEF\xB8\x8F",
                     "\xF0\x9F\x87\xAA\xF0\x9F\x87\xAA \xF0\x9F\x8E\xAE \xD0\xAD\xD1\x81\xD1\x82\xD0\xBE\xD0\xBD\xD0\xB8\xD1\x8F \xE2\x99\xBE\xEF\xB8\x8F");
}

static void test_control_chars(void)
{
    assert_quote("a\nb", 3, "\"a\\nb\"");
    assert_quote("a\tb", 3, "\"a\\tb\"");
    assert_quote("a\0b", 3, "\"a\\0b\"");
    assert_quote("\x01", 1, "\"\\x01\"");
    assert_quote("\x7F", 1, "\"\\x7F\"");
}

static void test_quote_and_backslash(void)
{
    assert_quote_str("a\"b", "\"a\\\"b\"");
    assert_quote_str("a\\b", "\"a\\\\b\"");
}

static int hexval(unsigned char c)
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
    return 0;
}

static void unescape_emitted(const char *s, strbuf *out)
{
    size_t n = strlen(s);
    size_t i = 0;

    if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
        n -= 1;
        i = 1;
    }
    for (; i < n; i++) {
        if (s[i] == '\\' && i + 1 < n) {
            switch (s[i + 1]) {
            case '\\':
                strbuf_append_char(out, '\\');
                i++;
                break;
            case '"':
                strbuf_append_char(out, '"');
                i++;
                break;
            case 'n':
                strbuf_append_char(out, '\n');
                i++;
                break;
            case 't':
                strbuf_append_char(out, '\t');
                i++;
                break;
            case 'r':
                strbuf_append_char(out, '\r');
                i++;
                break;
            case '0':
                strbuf_append_char(out, '\0');
                i++;
                break;
            case 'x':
                strbuf_append_char(out, (char)((hexval((unsigned char)s[i + 2]) << 4) |
                                               hexval((unsigned char)s[i + 3])));
                i += 3;
                break;
            default:
                strbuf_append_char(out, s[i]);
                break;
            }
        } else {
            strbuf_append_char(out, s[i]);
        }
    }
}

static void assert_roundtrip(const char *s)
{
    strbuf quoted;
    strbuf decoded;

    strbuf_init(&quoted);
    strbuf_init(&decoded);
    TEST_ASSERT_EQUAL_INT(0, yaml_quote(s, strlen(s), &quoted));
    unescape_emitted(quoted.data, &decoded);
    TEST_ASSERT_EQUAL_INT((int)strlen(s), (int)decoded.len);
    TEST_ASSERT_EQUAL_INT(0, memcmp(s, decoded.data, decoded.len));
    strbuf_free(&decoded);
    strbuf_free(&quoted);
}

static void test_roundtrip(void)
{
    assert_roundtrip("TikTok | Gemini");
    assert_roundtrip("Node: America");
    assert_roundtrip("a\nb");
    assert_roundtrip("\xF0\x9F\x87\xAA\xF0\x9F\x87\xBA \xD0\x90\xD0\xB2\xD1\x82\xD0\xBE");
    assert_roundtrip("123");
    assert_roundtrip("");
    assert_roundtrip("a\\b");
    assert_roundtrip("a\"b");
    assert_roundtrip("with space ");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_plain_pipe_in_middle);
    RUN_TEST(test_colon_space);
    RUN_TEST(test_space_hash);
    RUN_TEST(test_leading_indicators);
    RUN_TEST(test_resolved_scalars);
    RUN_TEST(test_empty);
    RUN_TEST(test_plain_name);
    RUN_TEST(test_whitespace_edges);
    RUN_TEST(test_emoji_plain);
    RUN_TEST(test_control_chars);
    RUN_TEST(test_quote_and_backslash);
    RUN_TEST(test_roundtrip);
    return UNITY_END();
}
