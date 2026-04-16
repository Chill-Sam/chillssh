// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonathan Wåhrenberg <jonathan@wahrenberg.com>
#include "conn.h"
#include "epoll_ctx.h"
#include "server.h"

#include <assert.h>
#include <errno.h> // IWYU pragma: keep
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static conn_t conn_pool[MAX_CLIENTS];
static bool conn_pool_used[MAX_CLIENTS];

conn_t *conn_create(int fd) {
    assert(fd >= 0);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!conn_pool_used[i]) {
            conn_pool_used[i] = true;
            conn_pool[i]      = (conn_t){
                     .fd      = fd,
                     .buf_len = 0,
                     .ctx     = {.type = EPOLL_TYPE_CONN, .conn = &conn_pool[i]},
            };
            return &conn_pool[i];
        }
    }
    return NULL; // pool exhausted
}

void conn_destroy(conn_t *c) {
    assert(c >= conn_pool && c < conn_pool + MAX_CLIENTS);
    size_t i = (size_t)(c - conn_pool);
    assert(conn_pool_used[i]);
    conn_pool_used[i] = false;
}
