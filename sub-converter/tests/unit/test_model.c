#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "base64.h"
#include "model.h"
#include "util.h"
#include "yaml_emit.h"

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_lifecycle(void)
{
    proxy_node node;

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(NODE_TYPE_UNKNOWN, node.type);
    TEST_ASSERT_EQUAL_INT(0, node.port);
    TEST_ASSERT_EQUAL_INT(0, node.udp);
    TEST_ASSERT_EQUAL_INT(NODE_TRANSPORT_NONE, node.transport);
    TEST_ASSERT_EQUAL_INT(NODE_TLS_NONE, node.tls);
    TEST_ASSERT_NULL(node.name);
    TEST_ASSERT_NULL(node.server);
    TEST_ASSERT_NULL(node.uuid);
    TEST_ASSERT_NULL(node.password);
    TEST_ASSERT_NULL(node.cipher);

    node_free(&node);
    node_free(&node);
    node_clear(&node);
    node_clear(&node);

    {
        proxy_node *np = node_new();
        TEST_ASSERT_NOT_NULL(np);
        TEST_ASSERT_EQUAL_INT(NODE_TYPE_UNKNOWN, np->type);
        node_free(np);
        free(np);
    }
}

static void test_vless_ws(void)
{
    const char *link =
        "vless://00000000-0000-4000-8000-000000000001@ee10.proxy.example.com:50068"
        "?encryption=none&type=ws&security=none"
        "#%F0%9F%87%AA%F0%9F%87%AA%20%F0%9F%8E%AE%20%D0%AD%D1%81%D1%82%D0%BE"
        "%D0%BD%D0%B8%D1%8F%20%E2%99%BE%EF%B8%8F";
    proxy_node node;

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link, strlen(link), &node));
    TEST_ASSERT_EQUAL_INT(NODE_TYPE_VLESS, node.type);
    TEST_ASSERT_EQUAL_STRING("ee10.proxy.example.com", node.server);
    TEST_ASSERT_EQUAL_INT(50068, node.port);
    TEST_ASSERT_EQUAL_STRING("00000000-0000-4000-8000-000000000001", node.uuid);
    TEST_ASSERT_EQUAL_INT(NODE_TRANSPORT_WS, node.transport);
    TEST_ASSERT_EQUAL_INT(NODE_TLS_NONE, node.tls);
    TEST_ASSERT_EQUAL_INT(1, node.udp);
    node_free(&node);
}

static void test_vless_xhttp_reality(void)
{
    const char *link =
        "vless://00000000-0000-4000-8000-000000000001@ddnl0.proxy.example.com:50623"
        "?encryption=none&type=xhttp&path=%2FIStoreBrowseService%2FGetItems%2Fv1"
        "&host=cdn.example.com&mode=auto&security=reality"
        "&sni=cdn.example.com&fp=firefox&pbk=TESTREALITYPUBLICKEY00000000000000000000000"
        "&sid=000000000003"
        "#%F0%9F%87%B3%F0%9F%87%B1%20%E2%AD%90%EF%B8%8F%20%D0%9D%D0%B8%D0%B4"
        "%D0%B5%D1%80%D0%BB%D0%B0%D0%BD%D0%B4%D1%8B%20%E2%99%BE%EF%B8%8F";
    proxy_node node;

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link, strlen(link), &node));
    TEST_ASSERT_EQUAL_INT(NODE_TYPE_VLESS, node.type);
    TEST_ASSERT_EQUAL_STRING("ddnl0.proxy.example.com", node.server);
    TEST_ASSERT_EQUAL_INT(50623, node.port);
    TEST_ASSERT_EQUAL_STRING("00000000-0000-4000-8000-000000000001", node.uuid);
    TEST_ASSERT_EQUAL_INT(NODE_TRANSPORT_XHTTP, node.transport);
    TEST_ASSERT_EQUAL_INT(NODE_TLS_REALITY, node.tls);
    TEST_ASSERT_EQUAL_STRING("/IStoreBrowseService/GetItems/v1", node.xhttp_path);
    TEST_ASSERT_EQUAL_STRING("cdn.example.com", node.xhttp_host);
    TEST_ASSERT_EQUAL_STRING("auto", node.xhttp_mode);
    TEST_ASSERT_EQUAL_STRING("cdn.example.com", node.sni);
    TEST_ASSERT_EQUAL_STRING("firefox", node.fp);
    TEST_ASSERT_EQUAL_STRING("TESTREALITYPUBLICKEY00000000000000000000000", node.reality_pbk);
    TEST_ASSERT_EQUAL_STRING("000000000003", node.reality_sid);
    TEST_ASSERT_EQUAL_INT(1, node.udp);
    node_free(&node);
}

static void test_vless_ws_tls(void)
{
    const char *link =
        "vless://00000000-0000-4000-8000-000000000001@sport.proxy.example.com:443"
        "?encryption=none&type=ws&path=%2Fgateway%2Fv1%2Fws%2F00000000000000000000000000000000"
        "&security=tls&sni=sport.proxy.example.com&fp=firefox&alpn=http%2F1.1"
        "#%F0%9F%8F%B3%EF%B8%8F%20LTE";
    proxy_node node;

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link, strlen(link), &node));
    TEST_ASSERT_EQUAL_INT(NODE_TYPE_VLESS, node.type);
    TEST_ASSERT_EQUAL_STRING("sport.proxy.example.com", node.server);
    TEST_ASSERT_EQUAL_INT(443, node.port);
    TEST_ASSERT_EQUAL_INT(NODE_TRANSPORT_WS, node.transport);
    TEST_ASSERT_EQUAL_INT(NODE_TLS_TLS, node.tls);
    TEST_ASSERT_EQUAL_STRING("/gateway/v1/ws/00000000000000000000000000000000", node.ws_path);
    TEST_ASSERT_EQUAL_STRING("http/1.1", node.alpn);
    node_free(&node);
}

static void test_vless_names(void)
{
    const char *link1 =
        "vless://00000000-0000-4000-8000-000000000001@ee10.proxy.example.com:50068"
        "?encryption=none&type=ws&security=none"
        "#%F0%9F%87%AA%F0%9F%87%AA%20%F0%9F%8E%AE%20%D0%AD%D1%81%D1%82%D0%BE"
        "%D0%BD%D0%B8%D1%8F%20%E2%99%BE%EF%B8%8F";
    const char *link2 =
        "vless://00000000-0000-4000-8000-000000000001@ddnl0.proxy.example.com:50623"
        "?encryption=none&type=xhttp&security=reality"
        "#%F0%9F%87%B3%F0%9F%87%B1%20%E2%AD%90%EF%B8%8F%20%D0%9D%D0%B8%D0%B4"
        "%D0%B5%D1%80%D0%BB%D0%B0%D0%BD%D0%B4%D1%8B%20%E2%99%BE%EF%B8%8F";
    const char *link3 =
        "vless://00000000-0000-4000-8000-000000000001@sport.proxy.example.com:443"
        "?encryption=none&type=ws&security=tls"
        "#%F0%9F%8F%B3%EF%B8%8F%20LTE";
    proxy_node node;

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link1, strlen(link1), &node));
    TEST_ASSERT_EQUAL_STRING(
        "\xF0\x9F\x87\xAA\xF0\x9F\x87\xAA \xF0\x9F\x8E\xAE "
        "\xD0\xAD\xD1\x81\xD1\x82\xD0\xBE\xD0\xBD\xD0\xB8\xD1\x8F "
        "\xE2\x99\xBE\xEF\xB8\x8F",
        node.name);
    node_free(&node);

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link2, strlen(link2), &node));
    TEST_ASSERT_EQUAL_STRING(
        "\xF0\x9F\x87\xB3\xF0\x9F\x87\xB1 \xE2\xAD\x90\xEF\xB8\x8F "
        "\xD0\x9D\xD0\xB8\xD0\xB4\xD0\xB5\xD1\x80\xD0\xBB\xD0\xB0\xD0\xBD\xD0\xB4\xD1\x8B "
        "\xE2\x99\xBE\xEF\xB8\x8F",
        node.name);
    node_free(&node);

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link3, strlen(link3), &node));
    TEST_ASSERT_EQUAL_STRING("\xF0\x9F\x8F\xB3\xEF\xB8\x8F LTE", node.name);
    node_free(&node);
}

static void test_raw_name_yaml(void)
{
    const char *link =
        "vless://00000000-0000-4000-8000-000000000001@x.example.com:443"
        "?encryption=none&type=ws&security=none#TikTok%20%7C%20Gemini";
    proxy_node node;
    strbuf sb;

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link, strlen(link), &node));
    TEST_ASSERT_EQUAL_STRING("TikTok | Gemini", node.name);

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, yaml_quote(node.name, strlen(node.name), &sb));
    TEST_ASSERT_EQUAL_STRING("TikTok | Gemini", sb.data);
    strbuf_free(&sb);

    node_free(&node);
}

static void test_ss_plain(void)
{
    const char *link = "ss://aes-256-gcm:secret@1.2.3.4:8388#SS%20Node";
    proxy_node node;

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link, strlen(link), &node));
    TEST_ASSERT_EQUAL_INT(NODE_TYPE_SHADOWSOCKS, node.type);
    TEST_ASSERT_EQUAL_STRING("aes-256-gcm", node.cipher);
    TEST_ASSERT_EQUAL_STRING("secret", node.password);
    TEST_ASSERT_EQUAL_STRING("1.2.3.4", node.server);
    TEST_ASSERT_EQUAL_INT(8388, node.port);
    TEST_ASSERT_EQUAL_STRING("SS Node", node.name);
    TEST_ASSERT_EQUAL_INT(1, node.udp);
    node_free(&node);
}

static void test_ss_legacy_base64(void)
{
    const char *link = "ss://YWVzLTI1Ni1nY206c2VjcmV0@5.6.7.8:8388";
    proxy_node node;

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link, strlen(link), &node));
    TEST_ASSERT_EQUAL_INT(NODE_TYPE_SHADOWSOCKS, node.type);
    TEST_ASSERT_EQUAL_STRING("aes-256-gcm", node.cipher);
    TEST_ASSERT_EQUAL_STRING("secret", node.password);
    TEST_ASSERT_EQUAL_STRING("5.6.7.8", node.server);
    TEST_ASSERT_EQUAL_INT(8388, node.port);
    TEST_ASSERT_EQUAL_STRING("5.6.7.8:8388", node.name);
    node_free(&node);
}

static void test_ss_sip002(void)
{
    const char *authority = "aes-256-gcm:secret@9.9.9.9:8388";
    char link[512];
    strbuf sb;
    proxy_node node;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, base64_encode_url(authority, strlen(authority), &sb));
    snprintf(link, sizeof(link), "ss://%s?plugin=v2ray-plugin%%3Bmode%%3Dwebsocket#SIP%%20Node",
             sb.data);
    strbuf_free(&sb);

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link, strlen(link), &node));
    TEST_ASSERT_EQUAL_INT(NODE_TYPE_SHADOWSOCKS, node.type);
    TEST_ASSERT_EQUAL_STRING("aes-256-gcm", node.cipher);
    TEST_ASSERT_EQUAL_STRING("secret", node.password);
    TEST_ASSERT_EQUAL_STRING("9.9.9.9", node.server);
    TEST_ASSERT_EQUAL_INT(8388, node.port);
    TEST_ASSERT_EQUAL_STRING("v2ray-plugin;mode=websocket", node.plugin);
    TEST_ASSERT_EQUAL_STRING("SIP Node", node.name);
    node_free(&node);
}

static void test_trojan(void)
{
    const char *link =
        "trojan://secret@trojan.example.com:443?security=tls&sni=trojan.example.com"
        "&type=ws&path=%2Fws#Trojan%20Node";
    proxy_node node;

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link, strlen(link), &node));
    TEST_ASSERT_EQUAL_INT(NODE_TYPE_TROJAN, node.type);
    TEST_ASSERT_EQUAL_STRING("secret", node.password);
    TEST_ASSERT_EQUAL_STRING("trojan.example.com", node.server);
    TEST_ASSERT_EQUAL_INT(443, node.port);
    TEST_ASSERT_EQUAL_INT(NODE_TLS_TLS, node.tls);
    TEST_ASSERT_EQUAL_STRING("trojan.example.com", node.sni);
    TEST_ASSERT_EQUAL_INT(NODE_TRANSPORT_WS, node.transport);
    TEST_ASSERT_EQUAL_STRING("/ws", node.ws_path);
    TEST_ASSERT_EQUAL_STRING("Trojan Node", node.name);
    TEST_ASSERT_EQUAL_INT(1, node.udp);
    node_free(&node);
}

static void test_vmess_json(void)
{
    const char *json =
        "{\"v\":\"2\",\"ps\":\"VMess%20Node\",\"add\":\"vm.example.com\",\"port\":\"8443\","
        "\"id\":\"uuid-1\",\"aid\":\"0\",\"net\":\"ws\",\"type\":\"none\","
        "\"host\":\"cdn.example.com\",\"path\":\"/vmws\",\"tls\":\"tls\","
        "\"sni\":\"cdn.example.com\",\"alpn\":\"h2\"}";
    char link[512];
    strbuf sb;
    proxy_node node;

    strbuf_init(&sb);
    TEST_ASSERT_EQUAL_INT(0, base64_encode(json, strlen(json), &sb));
    snprintf(link, sizeof(link), "vmess://%s", sb.data);
    strbuf_free(&sb);

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link, strlen(link), &node));
    TEST_ASSERT_EQUAL_INT(NODE_TYPE_VMESS, node.type);
    TEST_ASSERT_EQUAL_STRING("vm.example.com", node.server);
    TEST_ASSERT_EQUAL_INT(8443, node.port);
    TEST_ASSERT_EQUAL_STRING("uuid-1", node.uuid);
    TEST_ASSERT_EQUAL_INT(0, node.alter_id);
    TEST_ASSERT_EQUAL_STRING("auto", node.cipher);
    TEST_ASSERT_EQUAL_INT(NODE_TRANSPORT_WS, node.transport);
    TEST_ASSERT_EQUAL_INT(NODE_TLS_TLS, node.tls);
    TEST_ASSERT_EQUAL_STRING("cdn.example.com", node.ws_host);
    TEST_ASSERT_EQUAL_STRING("/vmws", node.ws_path);
    TEST_ASSERT_EQUAL_STRING("cdn.example.com", node.sni);
    TEST_ASSERT_EQUAL_STRING("VMess Node", node.name);
    TEST_ASSERT_EQUAL_INT(1, node.udp);
    node_free(&node);
}

static void test_vmess_form_a(void)
{
    const char *link =
        "vmess://uuid-1@vm.example.com:8443?type=ws&path=%2Fvmws#FormA%20Node";
    proxy_node node;

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link, strlen(link), &node));
    TEST_ASSERT_EQUAL_INT(NODE_TYPE_VMESS, node.type);
    TEST_ASSERT_EQUAL_STRING("uuid-1", node.uuid);
    TEST_ASSERT_EQUAL_STRING("vm.example.com", node.server);
    TEST_ASSERT_EQUAL_INT(8443, node.port);
    TEST_ASSERT_EQUAL_STRING("auto", node.cipher);
    TEST_ASSERT_EQUAL_INT(NODE_TRANSPORT_WS, node.transport);
    TEST_ASSERT_EQUAL_STRING("/vmws", node.ws_path);
    TEST_ASSERT_EQUAL_STRING("FormA Node", node.name);
    node_free(&node);
}

static void test_ipv6(void)
{
    const char *link = "vless://uuid-1@[2001:db8::1]:8443#IPv6%20Node";
    proxy_node node;

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link, strlen(link), &node));
    TEST_ASSERT_EQUAL_INT(NODE_TYPE_VLESS, node.type);
    TEST_ASSERT_EQUAL_STRING("2001:db8::1", node.server);
    TEST_ASSERT_EQUAL_INT(8443, node.port);
    TEST_ASSERT_EQUAL_STRING("IPv6 Node", node.name);
    node_free(&node);
}

static void test_trailing_crlf(void)
{
    const char *link = "ss://aes-256-gcm:secret@1.2.3.4:8388#SS%20Node\r\n";
    proxy_node node;

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(0, node_parse_share(link, strlen(link), &node));
    TEST_ASSERT_EQUAL_INT(NODE_TYPE_SHADOWSOCKS, node.type);
    TEST_ASSERT_EQUAL_STRING("1.2.3.4", node.server);
    TEST_ASSERT_EQUAL_INT(8388, node.port);
    TEST_ASSERT_EQUAL_STRING("SS Node", node.name);
    node_free(&node);
}

static void test_errors(void)
{
    proxy_node node;

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(-1, node_parse_share("http://example.com", strlen("http://example.com"), &node));
    TEST_ASSERT_EQUAL_INT(NODE_TYPE_UNKNOWN, node.type);
    TEST_ASSERT_NULL(node.name);
    TEST_ASSERT_NULL(node.server);
    node_free(&node);

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(-1, node_parse_share("vless://", strlen("vless://"), &node));
    node_free(&node);

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(-1, node_parse_share("vless://uuid@host:abc", strlen("vless://uuid@host:abc"), &node));
    node_free(&node);

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(-1, node_parse_share("vmess://!!notbase64!!", strlen("vmess://!!notbase64!!"), &node));
    node_free(&node);

    node_init(&node);
    TEST_ASSERT_EQUAL_INT(-1, node_parse_share("", 0, &node));
    node_free(&node);
}

static void test_enum_names(void)
{
    node_type t;
    node_transport tr;

    for (t = NODE_TYPE_UNKNOWN; t <= NODE_TYPE_HYSTERIA2; t++) {
        TEST_ASSERT_NOT_NULL(node_type_name(t));
    }
    for (tr = NODE_TRANSPORT_NONE; tr <= NODE_TRANSPORT_HTTP; tr++) {
        TEST_ASSERT_NOT_NULL(node_transport_name(tr));
    }
    TEST_ASSERT_EQUAL_STRING("vless", node_type_name(NODE_TYPE_VLESS));
    TEST_ASSERT_EQUAL_STRING("vmess", node_type_name(NODE_TYPE_VMESS));
    TEST_ASSERT_EQUAL_STRING("shadowsocks", node_type_name(NODE_TYPE_SHADOWSOCKS));
    TEST_ASSERT_EQUAL_STRING("trojan", node_type_name(NODE_TYPE_TROJAN));
    TEST_ASSERT_EQUAL_STRING("socks5", node_type_name(NODE_TYPE_SOCKS5));
    TEST_ASSERT_EQUAL_STRING("http", node_type_name(NODE_TYPE_HTTP));
    TEST_ASSERT_EQUAL_STRING("hysteria2", node_type_name(NODE_TYPE_HYSTERIA2));
    TEST_ASSERT_EQUAL_STRING("unknown", node_type_name(NODE_TYPE_UNKNOWN));
    TEST_ASSERT_EQUAL_STRING("none", node_transport_name(NODE_TRANSPORT_NONE));
    TEST_ASSERT_EQUAL_STRING("ws", node_transport_name(NODE_TRANSPORT_WS));
    TEST_ASSERT_EQUAL_STRING("xhttp", node_transport_name(NODE_TRANSPORT_XHTTP));
    TEST_ASSERT_EQUAL_STRING("grpc", node_transport_name(NODE_TRANSPORT_GRPC));
    TEST_ASSERT_EQUAL_STRING("http", node_transport_name(NODE_TRANSPORT_HTTP));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lifecycle);
    RUN_TEST(test_vless_ws);
    RUN_TEST(test_vless_xhttp_reality);
    RUN_TEST(test_vless_ws_tls);
    RUN_TEST(test_vless_names);
    RUN_TEST(test_raw_name_yaml);
    RUN_TEST(test_ss_plain);
    RUN_TEST(test_ss_legacy_base64);
    RUN_TEST(test_ss_sip002);
    RUN_TEST(test_trojan);
    RUN_TEST(test_vmess_json);
    RUN_TEST(test_vmess_form_a);
    RUN_TEST(test_ipv6);
    RUN_TEST(test_trailing_crlf);
    RUN_TEST(test_errors);
    RUN_TEST(test_enum_names);
    return UNITY_END();
}

