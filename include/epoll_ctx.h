// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonathan Wåhrenberg <jonathan@wahrenberg.com>
#pragma once

typedef enum {
    EPOLL_TYPE_UNKNOWN = 0,
    EPOLL_TYPE_SERVER,
    EPOLL_TYPE_CONN,
} epoll_type_t;

typedef struct conn_t conn_t;
typedef struct server_t server_t;

typedef struct {
    epoll_type_t type;
    union {
        server_t *server;
        conn_t *conn;
    };
} epoll_ctx_t;
