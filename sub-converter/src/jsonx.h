#ifndef SUB_CONV_JSONX_H
#define SUB_CONV_JSONX_H

#include <stddef.h>
#include <stdint.h>

struct json_object;
struct strbuf;

/* Parse JSON from memory. Returns new reference or NULL on error. */
struct json_object *jsonx_parse(const char *data, size_t len);

/* Type predicates (0 or 1). */
int jsonx_is_object(const struct json_object *obj);
int jsonx_is_array(const struct json_object *obj);
int jsonx_is_string(const struct json_object *obj);
int jsonx_is_int(const struct json_object *obj);
int jsonx_is_bool(const struct json_object *obj);
int jsonx_is_null(const struct json_object *obj);

/* Borrowed accessors (no new reference). */
struct json_object *jsonx_obj_get(const struct json_object *obj, const char *key);
struct json_object *jsonx_arr_get(const struct json_object *obj, int index);
int jsonx_arr_len(const struct json_object *obj);
const char *jsonx_str(const struct json_object *obj);
int64_t jsonx_int(const struct json_object *obj);
double jsonx_double(const struct json_object *obj);
int jsonx_bool(const struct json_object *obj);

/* Convenience typed getters: 0 on success, -1 if missing or wrong type. */
int jsonx_get_string(const struct json_object *obj, const char *key, const char **out);
int jsonx_get_int(const struct json_object *obj, const char *key, int64_t *out);
int jsonx_get_bool(const struct json_object *obj, const char *key, int *out);
int jsonx_get_obj(const struct json_object *obj, const char *key, struct json_object **out);

/* Iteration over object entries. Returns 0 while more entries remain; sets key/val (borrowed).
   Caller keeps calling with the same iterator index starting at 0. */
int jsonx_obj_entries(const struct json_object *obj, int *index, const char **key, struct json_object **val);

/* Constructors: return new reference. */
struct json_object *jsonx_new_string(const char *s);
struct json_object *jsonx_new_int(int64_t v);
struct json_object *jsonx_new_bool(int b);
struct json_object *jsonx_new_null(void);
struct json_object *jsonx_new_object(void);
struct json_object *jsonx_new_array(void);
void jsonx_array_add(struct json_object *arr, struct json_object *item);
void jsonx_object_set(struct json_object *obj, const char *key, struct json_object *val);
void jsonx_object_set_string(struct json_object *obj, const char *key, const char *val);
void jsonx_object_set_int(struct json_object *obj, const char *key, int64_t val);

/* Serialize compact (no pretty-print) to strbuf. Returns 0 on success. */
int jsonx_serialize(const struct json_object *obj, struct strbuf *out);

/* Deep clone: returns new reference or NULL. */
struct json_object *jsonx_clone(const struct json_object *obj);

#endif
