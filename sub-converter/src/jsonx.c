#include "jsonx.h"
#include "util.h"

#include <json-c/json.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct json_object *jsonx_parse(const char *data, size_t len)
{
    struct json_tokener *tok;
    struct json_object *o;
    char *copy;
    enum json_tokener_error err;
    size_t end;

    if (!data || len > (size_t)INT_MAX) {
        return NULL;
    }
    copy = malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, data, len);
    copy[len] = '\0';
    tok = json_tokener_new();
    if (!tok) {
        free(copy);
        return NULL;
    }
    json_tokener_set_flags(tok, JSON_TOKENER_STRICT);
    o = json_tokener_parse_ex(tok, copy, (int)(len + 1));
    err = json_tokener_get_error(tok);
    end = json_tokener_get_parse_end(tok);
    json_tokener_free(tok);
    free(copy);
    if (!o && err == json_tokener_success && end == len) {
        return NULL;
    }
    if (err != json_tokener_success || end != len) {
        if (o) {
            json_object_put(o);
        }
        return NULL;
    }
    return o;
}

int jsonx_is_object(const struct json_object *obj)
{
    return obj && json_object_is_type(obj, json_type_object);
}

int jsonx_is_array(const struct json_object *obj)
{
    return obj && json_object_is_type(obj, json_type_array);
}

int jsonx_is_string(const struct json_object *obj)
{
    return obj && json_object_is_type(obj, json_type_string);
}

int jsonx_is_int(const struct json_object *obj)
{
    return obj && json_object_is_type(obj, json_type_int);
}

int jsonx_is_bool(const struct json_object *obj)
{
    return obj && json_object_is_type(obj, json_type_boolean);
}

int jsonx_is_null(const struct json_object *obj)
{
    return json_object_is_type(obj, json_type_null);
}

struct json_object *jsonx_obj_get(const struct json_object *obj, const char *key)
{
    struct json_object *val = NULL;

    if (!obj || !key) {
        return NULL;
    }
    json_object_object_get_ex(obj, key, &val);
    return val;
}

struct json_object *jsonx_arr_get(const struct json_object *obj, int index)
{
    if (!obj || !json_object_is_type(obj, json_type_array) || index < 0) {
        return NULL;
    }
    return json_object_array_get_idx(obj, index);
}

int jsonx_arr_len(const struct json_object *obj)
{
    if (!obj || !json_object_is_type(obj, json_type_array)) {
        return 0;
    }
    return json_object_array_length(obj);
}

const char *jsonx_str(const struct json_object *obj)
{
    if (!obj) {
        return NULL;
    }
    return json_object_get_string((struct json_object *)obj);
}

int64_t jsonx_int(const struct json_object *obj)
{
    if (!obj) {
        return 0;
    }
    return json_object_get_int64(obj);
}

double jsonx_double(const struct json_object *obj)
{
    if (!obj) {
        return 0.0;
    }
    return json_object_get_double(obj);
}

int jsonx_bool(const struct json_object *obj)
{
    if (!obj) {
        return 0;
    }
    return json_object_get_boolean(obj);
}

int jsonx_get_string(const struct json_object *obj, const char *key, const char **out)
{
    struct json_object *v;

    if (!obj || !key || !out) {
        return -1;
    }
    if (!json_object_object_get_ex(obj, key, &v) || !json_object_is_type(v, json_type_string)) {
        return -1;
    }
    *out = json_object_get_string(v);
    return 0;
}

int jsonx_get_int(const struct json_object *obj, const char *key, int64_t *out)
{
    struct json_object *v;

    if (!obj || !key || !out) {
        return -1;
    }
    if (!json_object_object_get_ex(obj, key, &v) || !json_object_is_type(v, json_type_int)) {
        return -1;
    }
    *out = json_object_get_int64(v);
    return 0;
}

int jsonx_get_bool(const struct json_object *obj, const char *key, int *out)
{
    struct json_object *v;

    if (!obj || !key || !out) {
        return -1;
    }
    if (!json_object_object_get_ex(obj, key, &v) || !json_object_is_type(v, json_type_boolean)) {
        return -1;
    }
    *out = json_object_get_boolean(v);
    return 0;
}

int jsonx_get_obj(const struct json_object *obj, const char *key, struct json_object **out)
{
    struct json_object *v;

    if (!obj || !key || !out) {
        return -1;
    }
    if (!json_object_object_get_ex(obj, key, &v) || !json_object_is_type(v, json_type_object)) {
        return -1;
    }
    *out = v;
    return 0;
}

int jsonx_obj_entries(const struct json_object *obj, int *index, const char **key,
                      struct json_object **val)
{
    int n;
    int i = 0;

    if (!obj || !json_object_is_type(obj, json_type_object) || !index || !key || !val) {
        return -1;
    }
    n = json_object_object_length(obj);
    if (*index < 0 || *index >= n) {
        return -1;
    }
    json_object_object_foreach(obj, k, v) {
        if (i == *index) {
            *key = k;
            *val = v;
            *index += 1;
            return 0;
        }
        i++;
    }
    return -1;
}

struct json_object *jsonx_new_string(const char *s)
{
    return json_object_new_string(s ? s : "");
}

struct json_object *jsonx_new_int(int64_t v)
{
    return json_object_new_int64(v);
}

struct json_object *jsonx_new_bool(int b)
{
    return json_object_new_boolean(b ? 1 : 0);
}

struct json_object *jsonx_new_null(void)
{
    return json_object_new_null();
}

struct json_object *jsonx_new_object(void)
{
    return json_object_new_object();
}

struct json_object *jsonx_new_array(void)
{
    return json_object_new_array();
}

void jsonx_array_add(struct json_object *arr, struct json_object *item)
{
    if (!arr) {
        return;
    }
    json_object_array_add(arr, item);
}

void jsonx_object_set(struct json_object *obj, const char *key, struct json_object *val)
{
    if (!obj || !key) {
        return;
    }
    json_object_object_add(obj, key, val);
}

void jsonx_object_set_string(struct json_object *obj, const char *key, const char *val)
{
    if (!obj || !key) {
        return;
    }
    json_object_object_add(obj, key, json_object_new_string(val ? val : ""));
}

void jsonx_object_set_int(struct json_object *obj, const char *key, int64_t val)
{
    if (!obj || !key) {
        return;
    }
    json_object_object_add(obj, key, json_object_new_int64(val));
}

int jsonx_serialize(const struct json_object *obj, struct strbuf *out)
{
    const char *s;

    if (!obj || !out) {
        return -1;
    }
    s = json_object_to_json_string_ext((struct json_object *)obj, JSON_C_TO_STRING_PLAIN);
    if (!s) {
        return -1;
    }
    return strbuf_append_str(out, s);
}

struct json_object *jsonx_clone(const struct json_object *obj)
{
    if (!obj) {
        return NULL;
    }
    return json_object_get((struct json_object *)obj);
}
