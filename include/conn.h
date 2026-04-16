// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonathan Wåhrenberg <jonathan@wahrenberg.com>
#pragma once

#include "epoll_ctx.h"

#include <stddef.h>

#define BUF_SIZE 4096

typedef struct conn_t {
    epoll_ctx_t ctx;
    char buf[BUF_SIZE];
    size_t buf_len;
    int fd;
} conn_t;

conn_t *conn_create(int fd);
void conn_destroy(conn_t *c);
