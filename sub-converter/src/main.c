#define _DEFAULT_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "server.h"
#include "version.h"

static volatile sig_atomic_t g_stop = 0;

static void handle_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s [--host H] [--port N]\n", prog);
}

int main(int argc, char **argv)
{
    const char *host = "0.0.0.0";
    int port = 9099;
    server_config cfg;
    server *s;
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
            if (port <= 0) {
                usage(argv[0]);
                return 1;
            }
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.listen_host = host;
    cfg.port = port;
    s = server_create(&cfg);
    if (!s) {
        fprintf(stderr, "subconvd: failed to start server\n");
        return 1;
    }
    if (server_start(s) < 0) {
        fprintf(stderr, "subconvd: failed to start server\n");
        server_stop(s);
        return 1;
    }

    printf("sub-converter %s listening on %s:%d\n", subconv_version(), host,
           server_get_port(s));
    fflush(stdout);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    while (!g_stop) {
        sleep(1);
    }

    server_stop(s);
    return 0;
}
