#include <string.h>

#include "unity.h"
#include "base64.h"
#include "util.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void assert_b64_encode(const char *in, const char *expected)
{
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, base64_encode(in, strlen(in), &sb));
    TEST_ASSERT_EQUAL_INT((int)strlen(expected), (int)sb.len);
    TEST_ASSERT_EQUAL_INT(0, memcmp(expected, sb.data, sb.len));
    strbuf_free(&sb);
}

static void test_encode_standard(void)
{
    assert_b64_encode("", "");
    assert_b64_encode("f", "Zg==");
    assert_b64_encode("fo", "Zm8=");
    assert_b64_encode("foo", "Zm9v");
    assert_b64_encode("foob", "Zm9vYg==");
    assert_b64_encode("fooba", "Zm9vYmE=");
    assert_b64_encode("foobar", "Zm9vYmFy");
}

static void test_decode_roundtrip(void)
{
    static const char *strs[] = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
    size_t i;

    for (i = 0; i < sizeof(strs) / sizeof(strs[0]); i++) {
        strbuf enc;
        strbuf dec;

        strbuf_init(&enc);
        strbuf_init(&dec);
        TEST_ASSERT_EQUAL_INT(0, base64_encode(strs[i], strlen(strs[i]), &enc));
        TEST_ASSERT_EQUAL_INT(0, base64_decode(enc.data, enc.len, &dec));
        TEST_ASSERT_EQUAL_INT((int)strlen(strs[i]), (int)dec.len);
        TEST_ASSERT_EQUAL_INT(0, memcmp(strs[i], dec.data, dec.len));
        strbuf_free(&enc);
        strbuf_free(&dec);
    }
}

static void test_decode_whitespace(void)
{
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, base64_decode("Zm9v\nYmFy\r\n", strlen("Zm9v\nYmFy\r\n"), &sb));
    TEST_ASSERT_EQUAL_STRING("foobar", sb.data);
    strbuf_free(&sb);
}

static void test_decode_unpadded(void)
{
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, base64_decode("Zm9v", 4, &sb));
    TEST_ASSERT_EQUAL_STRING("foo", sb.data);
    strbuf_free(&sb);
}

static void test_decode_rejects_invalid(void)
{
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(-1, base64_decode("!!!!", 4, &sb));
    TEST_ASSERT_EQUAL_INT(-1, base64_decode("Zm9vY", 5, &sb));
    TEST_ASSERT_EQUAL_INT(-1, base64_decode("Zm9v====", 8, &sb));
    TEST_ASSERT_EQUAL_INT(-1, base64_decode("Yg=", 3, &sb));
    strbuf_free(&sb);
}

static void test_url_roundtrip(void)
{
    static const unsigned char bytes[] = {0xFB, 0xEF, 0xBB, 0xBF, 0xF8};
    strbuf enc;
    strbuf dec;

    strbuf_init(&enc);
    strbuf_init(&dec);
    TEST_ASSERT_EQUAL_INT(0, base64_encode_url(bytes, sizeof(bytes), &enc));
    TEST_ASSERT_EQUAL_STRING("---7v_g", enc.data);
    TEST_ASSERT_EQUAL_INT(0, base64_decode_url(enc.data, enc.len, &dec));
    TEST_ASSERT_EQUAL_INT((int)sizeof(bytes), (int)dec.len);
    TEST_ASSERT_EQUAL_INT(0, memcmp(bytes, dec.data, dec.len));
    strbuf_free(&enc);
    strbuf_free(&dec);
}

static void test_url_fixed_vector(void)
{
    static const unsigned char bytes[] = {0xFE, 0xEF, 0xBB, 0xBF, 0xF8};
    strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, base64_encode_url(bytes, sizeof(bytes), &sb));
    TEST_ASSERT_EQUAL_STRING("_u-7v_g", sb.data);
    strbuf_free(&sb);

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, base64_decode_url("_u-7v_g", 7, &sb));
    TEST_ASSERT_EQUAL_INT((int)sizeof(bytes), (int)sb.len);
    TEST_ASSERT_EQUAL_INT(0, memcmp(bytes, sb.data, sb.len));
    strbuf_free(&sb);

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, base64_decode_url("_u-7v_g=", 8, &sb));
    TEST_ASSERT_EQUAL_INT((int)sizeof(bytes), (int)sb.len);
    TEST_ASSERT_EQUAL_INT(0, memcmp(bytes, sb.data, sb.len));
    strbuf_free(&sb);
}

static void test_decode_utf8(void)
{
    const char *plain = "TikTok | Gemini";
    strbuf enc;
    strbuf dec;

    strbuf_init(&enc);
    strbuf_init(&dec);
    TEST_ASSERT_EQUAL_INT(0, base64_encode(plain, strlen(plain), &enc));
    TEST_ASSERT_EQUAL_INT(0, base64_decode(enc.data, enc.len, &dec));
    TEST_ASSERT_EQUAL_STRING(plain, dec.data);
    strbuf_free(&enc);
    strbuf_free(&dec);
}

static void test_looks_like(void)
{
    TEST_ASSERT_EQUAL_INT(1, base64_looks_like("Zm9vYmFy", 8));
    TEST_ASSERT_EQUAL_INT(0, base64_looks_like("hello world", 11));
    TEST_ASSERT_EQUAL_INT(0, base64_looks_like("", 0));
    TEST_ASSERT_EQUAL_INT(0, base64_looks_like("abc!", 4));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encode_standard);
    RUN_TEST(test_decode_roundtrip);
    RUN_TEST(test_decode_whitespace);
    RUN_TEST(test_decode_unpadded);
    RUN_TEST(test_decode_rejects_invalid);
    RUN_TEST(test_url_roundtrip);
    RUN_TEST(test_url_fixed_vector);
    RUN_TEST(test_decode_utf8);
    RUN_TEST(test_looks_like);
    return UNITY_END();
}
