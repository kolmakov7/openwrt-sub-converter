#include "yaml_emit.h"

#include <stdint.h>
#include <string.h>

#include "util.h"

static int is_digit(unsigned char c)
{
    return c >= '0' && c <= '9';
}

static int is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static int is_leading_indicator(unsigned char c)
{
    switch (c) {
    case '-':
    case '?':
    case ':':
    case ',':
    case '[':
    case ']':
    case '{':
    case '}':
    case '#':
    case '&':
    case '*':
    case '!':
    case '|':
    case '>':
    case '\'':
    case '"':
    case '%':
    case '@':
    case '`':
        return 1;
    default:
        return 0;
    }
}

static int hexch(unsigned char c)
{
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int is_yaml_number(const char *s, size_t len)
{
    size_t i = 0;

    if (i < len && (s[i] == '+' || s[i] == '-')) {
        i++;
    }
    if (i >= len) {
        return 0;
    }
    if (s[i] == '0' && i + 1 < len) {
        unsigned char p = (unsigned char)s[i + 1];
        size_t j;
        int ok = 0;

        if (p == 'x' || p == 'X') {
            for (j = i + 2; j < len; j++) {
                if (!hexch((unsigned char)s[j])) {
                    return 0;
                }
                ok = 1;
            }
            return ok;
        }
        if (p == 'b' || p == 'B') {
            for (j = i + 2; j < len; j++) {
                if (s[j] != '0' && s[j] != '1') {
                    return 0;
                }
                ok = 1;
            }
            return ok;
        }
        if (p == 'o' || p == 'O') {
            for (j = i + 2; j < len; j++) {
                if (s[j] < '0' || s[j] > '7') {
                    return 0;
                }
                ok = 1;
            }
            return ok;
        }
    }
    if (!is_digit((unsigned char)s[i])) {
        return 0;
    }
    while (i < len && is_digit((unsigned char)s[i])) {
        i++;
    }
    if (i < len && s[i] == '.') {
        size_t j = i + 1;

        if (j >= len || !is_digit((unsigned char)s[j])) {
            return 0;
        }
        while (j < len && is_digit((unsigned char)s[j])) {
            j++;
        }
        i = j;
    }
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        size_t j = i + 1;

        if (j < len && (s[j] == '+' || s[j] == '-')) {
            j++;
        }
        if (j >= len || !is_digit((unsigned char)s[j])) {
            return 0;
        }
        while (j < len && is_digit((unsigned char)s[j])) {
            j++;
        }
        i = j;
    }
    return i == len;
}

static int is_yaml_date(const char *s, size_t len)
{
    if (len != 10) {
        return 0;
    }
    return is_digit((unsigned char)s[0]) &&
           is_digit((unsigned char)s[1]) &&
           is_digit((unsigned char)s[2]) &&
           is_digit((unsigned char)s[3]) &&
           s[4] == '-' &&
           is_digit((unsigned char)s[5]) &&
           is_digit((unsigned char)s[6]) &&
           s[7] == '-' &&
           is_digit((unsigned char)s[8]) &&
           is_digit((unsigned char)s[9]);
}

static int ascii_lower_equal(const char *s, size_t len, const char *word)
{
    size_t i;

    if (strlen(word) != len) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];

        if (c >= 'A' && c <= 'Z') {
            c = (unsigned char)(c + ('a' - 'A'));
        }
        if (c != (unsigned char)word[i]) {
            return 0;
        }
    }
    return 1;
}

static int is_resolved_word(const char *s, size_t len)
{
    static const char *words[] = {
        "true", "false", "yes", "no", "on", "off", "null",
    };
    size_t i;

    for (i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        if (ascii_lower_equal(s, len, words[i])) {
            return 1;
        }
    }
    return len == 1 && s[0] == '~';
}

static int yaml_plain_safe(const char *s, size_t len)
{
    size_t i;

    if (is_ws((unsigned char)s[0]) || is_ws((unsigned char)s[len - 1])) {
        return 0;
    }
    if (is_leading_indicator((unsigned char)s[0])) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];

        if (c < 0x20 || c == 0x7F) {
            return 0;
        }
    }
    for (i = 0; i + 1 < len; i++) {
        if ((s[i] == ':' && s[i + 1] == ' ') ||
            (s[i] == ' ' && s[i + 1] == '#')) {
            return 0;
        }
    }
    if (memchr(s, '"', len) || memchr(s, '\\', len)) {
        return 0;
    }
    if ((len == 1 && s[0] == '.') ||
        (len == 2 && s[0] == '.' && s[1] == '.') ||
        is_yaml_number(s, len) ||
        is_resolved_word(s, len) ||
        is_yaml_date(s, len)) {
        return 0;
    }
    if (is_digit((unsigned char)s[0]) && memchr(s, ':', len)) {
        return 0;
    }
    return 1;
}

static void append_xhex(struct strbuf *out, unsigned char c)
{
    static const char hexdig[] = "0123456789ABCDEF";

    strbuf_append_char(out, '\\');
    strbuf_append_char(out, 'x');
    strbuf_append_char(out, hexdig[c >> 4]);
    strbuf_append_char(out, hexdig[c & 15]);
}

static void append_quoted(const char *s, size_t len, struct strbuf *out)
{
    size_t i;

    strbuf_append_char(out, '"');
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];

        switch (c) {
        case '\\':
            strbuf_append_str(out, "\\\\");
            break;
        case '"':
            strbuf_append_str(out, "\\\"");
            break;
        case '\n':
            strbuf_append_str(out, "\\n");
            break;
        case '\t':
            strbuf_append_str(out, "\\t");
            break;
        case '\r':
            strbuf_append_str(out, "\\r");
            break;
        case '\0':
            strbuf_append_str(out, "\\0");
            break;
        default:
            if (c < 0x20 || c == 0x7F) {
                append_xhex(out, c);
            } else {
                strbuf_append_char(out, (char)c);
            }
            break;
        }
    }
    strbuf_append_char(out, '"');
}

int yaml_quote(const char *s, size_t len, struct strbuf *out)
{
    if (s == NULL || len == 0) {
        strbuf_append_str(out, "\"\"");
        return 0;
    }
    if (yaml_plain_safe(s, len)) {
        strbuf_append_bytes(out, s, len);
    } else {
        append_quoted(s, len, out);
    }
    return 0;
}

static void emit_indent(struct strbuf *out, int indent)
{
    while (indent-- > 0) {
        strbuf_append_char(out, ' ');
    }
}

int yaml_emit_kv(struct strbuf *out, int indent, const char *key, const char *value)
{
    emit_indent(out, indent);
    strbuf_append_str(out, key);
    if (value) {
        strbuf_append_str(out, ": ");
        yaml_quote(value, strlen(value), out);
    } else {
        strbuf_append_char(out, ':');
    }
    strbuf_append_char(out, '\n');
    return 0;
}

int yaml_emit_kv_int(struct strbuf *out, int indent, const char *key, int64_t value)
{
    emit_indent(out, indent);
    strbuf_append_str(out, key);
    strbuf_append_str(out, ": ");
    strbuf_appendf(out, "%lld", (long long)value);
    strbuf_append_char(out, '\n');
    return 0;
}

int yaml_emit_kv_bool(struct strbuf *out, int indent, const char *key, int value)
{
    emit_indent(out, indent);
    strbuf_append_str(out, key);
    strbuf_append_str(out, value ? ": true\n" : ": false\n");
    return 0;
}

int yaml_emit_dash(struct strbuf *out, int indent, const char *value)
{
    emit_indent(out, indent);
    strbuf_append_str(out, "- ");
    yaml_quote(value, strlen(value), out);
    strbuf_append_char(out, '\n');
    return 0;
}

int yaml_emit_dash_kv(struct strbuf *out, int indent, const char *key, const char *value)
{
    emit_indent(out, indent);
    strbuf_append_str(out, "- ");
    strbuf_append_str(out, key);
    strbuf_append_str(out, ": ");
    yaml_quote(value, strlen(value), out);
    strbuf_append_char(out, '\n');
    return 0;
}

int yaml_emit_dash_kv_open(struct strbuf *out, int indent, const char *key)
{
    emit_indent(out, indent);
    strbuf_append_str(out, "- ");
    strbuf_append_str(out, key);
    strbuf_append_str(out, ":\n");
    return 0;
}

int yaml_emit_flow_list(struct strbuf *out, const char *const *items, size_t n)
{
    size_t i;

    strbuf_append_char(out, '[');
    for (i = 0; i < n; i++) {
        if (i > 0) {
            strbuf_append_str(out, ", ");
        }
        yaml_quote(items[i], strlen(items[i]), out);
    }
    strbuf_append_str(out, "]\n");
    return 0;
}
