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

typedef enum conn_status_t {
    CONN_OK = 0,
    CONN_ERR_INTERRUPTED,
    CONN_ERR_FCNTL,
    CONN_ERR_EPOLL_CTL,
    CONN_ERR_EPOLL_WAIT,
} conn_status_t;

conn_t *conn_create(int fd);
void conn_destroy(conn_t *c);
