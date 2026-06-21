// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonathan Wåhrenberg <jonathan@wahrenberg.com>

#include "crypto/chacha20.h"
#include "crypto/poly1305.h"
#include "log.h"
#include "server.h"

#include <assert.h>
#include <errno.h> // IWYU pragma: keep
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void handle_stop_signal(int signum) {
    (void)signum;
    running = 0;
}

static bool setup_signals(void) {
    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = handle_stop_signal;
    sa.sa_flags   = 0; // no SA_RESTART — we want epoll_wait to return EINTR

    if (sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1) {
        LOG_ERROR("sigaction() failed: %s", strerror(errno));
        return false;
    }

    return true;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        LOG_ERROR("Usage: ./chillssh [PORT_NUMBER]");
        return EXIT_FAILURE;
    }

    char *end;
    long port = strtol(argv[1], &end, 10);
    if (*end != '\0' || port <= 0 || port > 65535) {
        LOG_ERROR("invalid port number: %s", argv[1]);
        return EXIT_FAILURE;
    }

    if (!setup_signals()) {
        return EXIT_FAILURE;
    }

#ifndef NDEBUG
    chacha20_selftest();
    poly1305_selftest();
#endif

    server_t *server = server_create(port);
    assert(server != NULL);

    server_status_t ret = server_start(server);
    if (ret != SERVER_OK) {
        LOG_SERVER_ERROR("server_start", ret, server_get_errno(server));
        server_destroy(server);
        return EXIT_FAILURE;
    }

    while (running) {
        server_status_t r = server_poll(server);
        if (r == SERVER_ERR_INTERRUPTED) {
            break;
        }
        if (r != SERVER_OK) {
            LOG_ERROR("server_poll() failed: %s", server_status_str(r));
            break;
        }
    }

    LOG_INFO("Received stop signal, shutting down...");
    server_destroy(server);
    return EXIT_SUCCESS;
}
