// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonathan Wåhrenberg <jonathan@wahrenberg.com>
#pragma once

#include "conn.h"
#include "log.h"

#include <stdint.h>
#include <string.h> // IWYU pragma: keep

typedef struct server_t server_t;

typedef enum server_status_t {
    SERVER_OK = 0,
    SERVER_ERR_INTERRUPTED,
    SERVER_ERR_SOCKET,
    SERVER_ERR_SETSOCKOPT,
    SERVER_ERR_BIND,
    SERVER_ERR_LISTEN,
    SERVER_ERR_FCNTL,
    SERVER_ERR_EPOLL_CREATE,
    SERVER_ERR_EPOLL_CTL,
    SERVER_ERR_EPOLL_WAIT,
    SERVER_ERR_ACCEPT,
    SERVER_ERR_MAX_CONNECTIONS,
} server_status_t;

const char *server_status_str(server_status_t s);

#define LOG_SERVER_ERROR(ctx, ret, sys_errno)                                  \
    do {                                                                       \
        if ((sys_errno) != 0) {                                                \
            LOG_ERROR("%s: %s (%s)", ctx, server_status_str(ret),              \
                      strerror(sys_errno));                                    \
        } else {                                                               \
            LOG_ERROR("%s: %s", ctx, server_status_str(ret));                  \
        }                                                                      \
    } while (0)

server_t *server_create(uint16_t port);
void server_destroy(server_t *s);
server_status_t server_start(server_t *s);

server_status_t server_add_conn(server_t *s, conn_t *c);
server_status_t server_remove_conn(server_t *s, conn_t *c);
server_status_t server_poll(server_t *s);

int server_get_errno(const server_t *s);
