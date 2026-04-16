// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonathan Wåhrenberg <jonathan@wahrenberg.com>
#include "conn.h"
#include "epoll_ctx.h"
#include "log.h"

#include <assert.h>
#include <errno.h> // IWYU pragma: keep
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

conn_t *conn_create(int fd) {
    assert(fd >= 0);

    conn_t *c = malloc(sizeof(conn_t));
    if (c == NULL) {
        return NULL;
    }

    *c = (conn_t){
        .fd = fd, .buf_len = 0, .ctx = {.type = EPOLL_TYPE_CONN, .conn = c}};
    return c;
}

void conn_destroy(conn_t *c) {
    assert(c != NULL);

    if (c->fd != -1) {
        if (close(c->fd) == -1) {
            LOG_WARNING("conn_destroy: close() failed: %s", strerror(errno));
        }
        c->fd = -1;
    }

    free(c);
}
