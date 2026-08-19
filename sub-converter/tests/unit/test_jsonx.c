#include <string.h>

#include <json-c/json.h>

#include "unity.h"
#include "jsonx.h"
#include "util.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_parse_valid_types(void)
{
    struct json_object *o;

    o = jsonx_parse("{\"a\":1}", strlen("{\"a\":1}"));
    TEST_ASSERT_NOT_NULL(o);
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_object(o));
    json_object_put(o);

    o = jsonx_parse("[1,2]", strlen("[1,2]"));
    TEST_ASSERT_NOT_NULL(o);
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_array(o));
    json_object_put(o);

    o = jsonx_parse("\"hi\"", strlen("\"hi\""));
    TEST_ASSERT_NOT_NULL(o);
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_string(o));
    json_object_put(o);

    o = jsonx_parse("42", strlen("42"));
    TEST_ASSERT_NOT_NULL(o);
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_int(o));
    json_object_put(o);

    o = jsonx_parse("true", strlen("true"));
    TEST_ASSERT_NOT_NULL(o);
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_bool(o));
    json_object_put(o);

    o = jsonx_parse("null", strlen("null"));
    TEST_ASSERT_NULL(o);
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_null(o));
    json_object_put(o);
}

static void test_parse_invalid(void)
{
    TEST_ASSERT_NULL(jsonx_parse(NULL, 0));
    TEST_ASSERT_NULL(jsonx_parse("", 0));
    TEST_ASSERT_NULL(jsonx_parse("not json at all!!", strlen("not json at all!!")));
    TEST_ASSERT_NULL(jsonx_parse("{\"a\":1}xxx", strlen("{\"a\":1}xxx")));
    TEST_ASSERT_NULL(jsonx_parse("{\"a\":1}xxx", 8));
    TEST_ASSERT_NULL(jsonx_parse("{", 1));
}

static void test_type_predicates(void)
{
    struct json_object *o = jsonx_parse("{\"a\":1}", strlen("{\"a\":1}"));

    TEST_ASSERT_EQUAL_INT(0, jsonx_is_object(NULL));
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_object(o));
    TEST_ASSERT_EQUAL_INT(0, jsonx_is_array(o));
    TEST_ASSERT_EQUAL_INT(0, jsonx_is_string(o));
    TEST_ASSERT_EQUAL_INT(0, jsonx_is_int(o));
    TEST_ASSERT_EQUAL_INT(0, jsonx_is_bool(o));
    TEST_ASSERT_EQUAL_INT(0, jsonx_is_null(o));
    json_object_put(o);

    o = jsonx_parse("\"s\"", strlen("\"s\""));
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_string(o));
    TEST_ASSERT_EQUAL_INT(0, jsonx_is_object(o));
    json_object_put(o);

    o = jsonx_parse("7", 1);
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_int(o));
    TEST_ASSERT_EQUAL_INT(0, jsonx_is_bool(o));
    json_object_put(o);

    o = jsonx_parse("false", strlen("false"));
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_bool(o));
    TEST_ASSERT_EQUAL_INT(0, jsonx_is_int(o));
    json_object_put(o);

    TEST_ASSERT_EQUAL_INT(1, jsonx_is_null(NULL));
    TEST_ASSERT_EQUAL_INT(0, jsonx_is_object(NULL));
}

static void test_obj_get_and_typed(void)
{
    struct json_object *o = jsonx_parse("{\"name\":\"alice\",\"age\":30,\"active\":true,\"child\":{\"x\":1}}",
                                        strlen("{\"name\":\"alice\",\"age\":30,\"active\":true,\"child\":{\"x\":1}}"));
    struct json_object *v;
    const char *s;
    int64_t i;
    int b;

    TEST_ASSERT_NOT_NULL(o);
    v = jsonx_obj_get(o, "name");
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_string(v));
    TEST_ASSERT_NULL(jsonx_obj_get(o, "missing"));
    TEST_ASSERT_NULL(jsonx_obj_get(NULL, "name"));

    TEST_ASSERT_EQUAL_INT(0, jsonx_get_string(o, "name", &s));
    TEST_ASSERT_EQUAL_STRING("alice", s);
    TEST_ASSERT_EQUAL_INT(-1, jsonx_get_string(o, "age", &s));
    TEST_ASSERT_EQUAL_INT(-1, jsonx_get_string(o, "missing", &s));
    TEST_ASSERT_EQUAL_INT(-1, jsonx_get_string(NULL, "name", &s));

    TEST_ASSERT_EQUAL_INT(0, jsonx_get_int(o, "age", &i));
    TEST_ASSERT_EQUAL_INT(30, (int)i);
    TEST_ASSERT_EQUAL_INT(-1, jsonx_get_int(o, "name", &i));
    TEST_ASSERT_EQUAL_INT(-1, jsonx_get_int(o, "missing", &i));

    TEST_ASSERT_EQUAL_INT(0, jsonx_get_bool(o, "active", &b));
    TEST_ASSERT_EQUAL_INT(1, b);
    TEST_ASSERT_EQUAL_INT(-1, jsonx_get_bool(o, "age", &b));
    TEST_ASSERT_EQUAL_INT(-1, jsonx_get_bool(o, "missing", &b));

    TEST_ASSERT_EQUAL_INT(0, jsonx_get_obj(o, "child", &v));
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_object(v));
    TEST_ASSERT_EQUAL_INT(-1, jsonx_get_obj(o, "name", &v));
    TEST_ASSERT_EQUAL_INT(-1, jsonx_get_obj(o, "missing", &v));

    json_object_put(o);
}

static void test_array_access(void)
{
    struct json_object *o = jsonx_parse("[10,\"a\",[1],null]", strlen("[10,\"a\",[1],null]"));
    struct json_object *v;

    TEST_ASSERT_NOT_NULL(o);
    TEST_ASSERT_EQUAL_INT(4, jsonx_arr_len(o));
    v = jsonx_arr_get(o, 0);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(10, (int)jsonx_int(v));
    v = jsonx_arr_get(o, 1);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_STRING("a", jsonx_str(v));
    v = jsonx_arr_get(o, 2);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_array(v));
    v = jsonx_arr_get(o, 3);
    TEST_ASSERT_NULL(v);
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_null(v));
    TEST_ASSERT_NULL(jsonx_arr_get(o, 4));
    TEST_ASSERT_NULL(jsonx_arr_get(o, -1));
    TEST_ASSERT_EQUAL_INT(0, jsonx_arr_len(NULL));
    json_object_put(o);
}

static void test_obj_entries(void)
{
    struct json_object *o = jsonx_parse("{\"a\":1,\"b\":\"x\",\"c\":[1,2]}",
                                        strlen("{\"a\":1,\"b\":\"x\",\"c\":[1,2]}"));
    struct json_object *val;
    const char *key;
    int index = 0;
    int seen_a = 0;
    int seen_b = 0;
    int seen_c = 0;
    int n = 0;

    TEST_ASSERT_NOT_NULL(o);
    while (jsonx_obj_entries(o, &index, &key, &val) == 0) {
        if (strcmp(key, "a") == 0) {
            seen_a = 1;
            TEST_ASSERT_EQUAL_INT(1, jsonx_is_int(val));
        } else if (strcmp(key, "b") == 0) {
            seen_b = 1;
            TEST_ASSERT_EQUAL_STRING("x", jsonx_str(val));
        } else if (strcmp(key, "c") == 0) {
            seen_c = 1;
            TEST_ASSERT_EQUAL_INT(1, jsonx_is_array(val));
        }
        n++;
    }
    TEST_ASSERT_EQUAL_INT(3, n);
    TEST_ASSERT_EQUAL_INT(1, seen_a);
    TEST_ASSERT_EQUAL_INT(1, seen_b);
    TEST_ASSERT_EQUAL_INT(1, seen_c);
    TEST_ASSERT_EQUAL_INT(-1, jsonx_obj_entries(o, &index, &key, &val));
    TEST_ASSERT_EQUAL_INT(-1, jsonx_obj_entries(NULL, &index, &key, &val));
    json_object_put(o);
}

static void test_constructors_serialize_roundtrip(void)
{
    struct json_object *o = jsonx_new_object();
    struct json_object *arr = jsonx_new_array();
    struct json_object *reparsed;
    struct strbuf sb;
    const char *s;
    int64_t i;

    TEST_ASSERT_NOT_NULL(o);
    TEST_ASSERT_NOT_NULL(arr);
    jsonx_array_add(arr, jsonx_new_int(1));
    jsonx_array_add(arr, jsonx_new_string("two"));
    jsonx_object_set_string(o, "name", "bob");
    jsonx_object_set_int(o, "age", 41);
    jsonx_object_set(o, "active", jsonx_new_bool(1));
    jsonx_object_set(o, "nothing", jsonx_new_null());
    jsonx_object_set(o, "list", arr);

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, jsonx_serialize(o, &sb));
    TEST_ASSERT_TRUE(sb.len > 0);
    reparsed = jsonx_parse(sb.data, sb.len);
    TEST_ASSERT_NOT_NULL(reparsed);

    TEST_ASSERT_EQUAL_INT(0, jsonx_get_string(reparsed, "name", &s));
    TEST_ASSERT_EQUAL_STRING("bob", s);
    TEST_ASSERT_EQUAL_INT(0, jsonx_get_int(reparsed, "age", &i));
    TEST_ASSERT_EQUAL_INT(41, (int)i);
    TEST_ASSERT_EQUAL_INT(1, jsonx_bool(jsonx_obj_get(reparsed, "active")));
    TEST_ASSERT_EQUAL_INT(1, jsonx_is_null(jsonx_obj_get(reparsed, "nothing")));
    TEST_ASSERT_EQUAL_INT(2, jsonx_arr_len(jsonx_obj_get(reparsed, "list")));

    json_object_put(reparsed);
    strbuf_free(&sb);
    json_object_put(o);
}

static void test_serialize_null_handling(void)
{
    struct strbuf sb;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(-1, jsonx_serialize(NULL, &sb));
    strbuf_free(&sb);
}

static void test_clone_independent(void)
{
    struct json_object *o = jsonx_new_object();
    struct json_object *c;

    TEST_ASSERT_NOT_NULL(o);
    jsonx_object_set_string(o, "k", "orig");
    c = jsonx_clone(o);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQUAL_STRING("orig", jsonx_str(jsonx_obj_get(c, "k")));

    json_object_put(o);
    TEST_ASSERT_EQUAL_STRING("orig", jsonx_str(jsonx_obj_get(c, "k")));

    TEST_ASSERT_NULL(jsonx_clone(NULL));
    json_object_put(c);
}

static void test_str_on_non_string(void)
{
    struct json_object *o = jsonx_parse("{\"i\":5,\"b\":true}", strlen("{\"i\":5,\"b\":true}"));
    const char *s;

    TEST_ASSERT_NOT_NULL(o);
    s = jsonx_str(jsonx_obj_get(o, "i"));
    TEST_ASSERT_NOT_NULL(s);
    s = jsonx_str(jsonx_obj_get(o, "b"));
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_NULL(jsonx_str(NULL));
    json_object_put(o);
}

static void test_null_graceful(void)
{
    TEST_ASSERT_EQUAL_INT(0, jsonx_int(NULL));
    TEST_ASSERT_EQUAL_INT(0, jsonx_bool(NULL));
    TEST_ASSERT_EQUAL_INT(0, jsonx_arr_len(NULL));
    TEST_ASSERT_NULL(jsonx_obj_get(NULL, "k"));
    TEST_ASSERT_NULL(jsonx_arr_get(NULL, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_valid_types);
    RUN_TEST(test_parse_invalid);
    RUN_TEST(test_type_predicates);
    RUN_TEST(test_obj_get_and_typed);
    RUN_TEST(test_array_access);
    RUN_TEST(test_obj_entries);
    RUN_TEST(test_constructors_serialize_roundtrip);
    RUN_TEST(test_serialize_null_handling);
    RUN_TEST(test_clone_independent);
    RUN_TEST(test_str_on_non_string);
    RUN_TEST(test_null_graceful);
    return UNITY_END();
}
