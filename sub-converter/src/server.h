#ifndef SUB_CONV_SERVER_H
#define SUB_CONV_SERVER_H

typedef struct server server;
typedef struct server_config {
    const char *listen_host;
    int port;
} server_config;

server *server_create(const server_config *cfg);
int server_start(server *s);
void server_stop(server *s);
int server_get_port(const server *s);

#endif