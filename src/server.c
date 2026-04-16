// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonathan Wåhrenberg <jonathan@wahrenberg.com>
#include "server.h"
#include "conn.h"
#include "epoll_ctx.h"
#include "log.h"

#include <assert.h>
#include <errno.h> // IWYU pragma: keep
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTS 64

typedef struct server_t {
    epoll_ctx_t self_ctx;
    struct epoll_event events[MAX_EVENTS];
    conn_t *clients[MAX_CLIENTS];
    size_t client_count;
    uint16_t port;
    int fd;
    int epoll_fd;

    int last_errno;
} server_t;

static server_t g_server;

const char *server_status_str(server_status_t s) {
    switch (s) {
    case SERVER_OK:
        return "ok";
    case SERVER_ERR_SOCKET:
        return "failed to create socket";
    case SERVER_ERR_BIND:
        return "failed to bind";
    case SERVER_ERR_LISTEN:
        return "failed to listen";
    case SERVER_ERR_FCNTL:
        return "failed to set socket flags";
    case SERVER_ERR_EPOLL_CREATE:
        return "failed to create epoll instance";
    case SERVER_ERR_EPOLL_CTL:
        return "failed to add socket to epoll instance";
    case SERVER_ERR_ACCEPT:
        return "failed to accept connection";
    case SERVER_ERR_SETSOCKOPT:
        return "failed to set socket options";
    case SERVER_ERR_EPOLL_WAIT:
        return "failed to wait for events";
    case SERVER_ERR_INTERRUPTED:
        return "interrupted";
    case SERVER_ERR_MAX_CONNECTIONS:
        return "max connections reached";
    default:
        return "unknown error";
    }
}

static int server_find_free_slot(server_t *s) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (s->clients[i] == NULL)
            return i;
    }
    return -1;
}

static void server_untrack_conn(server_t *s, conn_t *c) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (s->clients[i] == c) {
            s->clients[i] = NULL;
            s->client_count--;
            return;
        }
    }
    assert(0 && "connection not found in tracking array");
}

server_t *server_create(uint16_t port) {
    assert(port > 0);

    g_server = (server_t){.port = port, .fd = -1, .epoll_fd = -1};
    return &g_server;
}

void server_destroy(server_t *s) {
    assert(s == &g_server);

    if (s->epoll_fd != -1) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (s->clients[i] != NULL) {
                if (epoll_ctl(s->epoll_fd, EPOLL_CTL_DEL, s->clients[i]->fd,
                              NULL) == -1) {
                    LOG_WARNING("epoll_ctl() call failed: %s", strerror(errno));
                }
                conn_destroy(s->clients[i]);
                s->clients[i] = NULL;
            }
        }

        if (close(s->epoll_fd) == -1) {
            LOG_WARNING("server_destroy: close() failed: %s", strerror(errno));
        }
        s->epoll_fd = -1;
    }

    if (s->fd != -1) {
        if (close(s->fd) == -1) {
            LOG_WARNING("server_destroy: close() failed: %s", strerror(errno));
        }
        s->fd = -1;
    }

    free(s);
}

server_status_t server_start(server_t *s) {
    assert(s != NULL);
    assert(s->fd == -1);
    assert(s->epoll_fd == -1);
    assert(s->port > 0);

    s->last_errno = 0;

    s->fd         = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s->fd < 0) {
        s->last_errno = errno;
        return SERVER_ERR_SOCKET;
    }

    int opt = 1;
    if (setsockopt(s->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        s->last_errno = errno;
        return SERVER_ERR_SETSOCKOPT;
    }

    struct sockaddr_in s_addr = {0};
    s_addr.sin_family         = AF_INET;
    s_addr.sin_port           = htons(s->port);
    s_addr.sin_addr.s_addr    = htonl(INADDR_ANY);

    if (bind(s->fd, (struct sockaddr *)&s_addr, sizeof(s_addr)) == -1) {
        s->last_errno = errno;
        return SERVER_ERR_BIND;
    }

    if (listen(s->fd, SOMAXCONN) == -1) {
        s->last_errno = errno;
        return SERVER_ERR_LISTEN;
    }

    int flags;
    if ((flags = fcntl(s->fd, F_GETFL)) == -1) {
        s->last_errno = errno;
        return SERVER_ERR_FCNTL;
    }
    if (fcntl(s->fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        s->last_errno = errno;
        return SERVER_ERR_FCNTL;
    }

    if ((s->epoll_fd = epoll_create1(0)) == -1) {
        s->last_errno = errno;
        return SERVER_ERR_EPOLL_CREATE;
    }

    s->self_ctx = (epoll_ctx_t){.type = EPOLL_TYPE_SERVER, .server = s};
    struct epoll_event ev = {.events = EPOLLIN, .data.ptr = &s->self_ctx};
    if (epoll_ctl(s->epoll_fd, EPOLL_CTL_ADD, s->fd, &ev) == -1) {
        s->last_errno = errno;
        return SERVER_ERR_EPOLL_CTL;
    }

    return SERVER_OK;
}

server_status_t server_add_conn(server_t *s, conn_t *c) {
    assert(s != NULL);
    assert(s->fd != -1);
    assert(s->epoll_fd != -1);
    assert(s->port > 0);
    assert(c != NULL);
    assert(c->fd != -1);

    s->last_errno = 0;

    int flags;
    if ((flags = fcntl(c->fd, F_GETFL)) == -1) {
        s->last_errno = errno;
        return SERVER_ERR_FCNTL;
    }
    if (fcntl(c->fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        s->last_errno = errno;
        return SERVER_ERR_FCNTL;
    }

    struct epoll_event ev = {.events = EPOLLIN, .data.ptr = &c->ctx};
    if (epoll_ctl(s->epoll_fd, EPOLL_CTL_ADD, c->fd, &ev) == -1) {
        s->last_errno = errno;
        return SERVER_ERR_EPOLL_CTL;
    }

    int slot = server_find_free_slot(s);
    if (slot == -1) {
        LOG_WARNING("max connections reached, dropping connection");
        return SERVER_ERR_MAX_CONNECTIONS;
    }
    s->clients[slot] = c;
    s->client_count++;

    return SERVER_OK;
}

server_status_t server_remove_conn(server_t *s, conn_t *c) {
    assert(s != NULL);
    assert(s->fd != -1);
    assert(s->epoll_fd != -1);
    assert(s->port > 0);
    assert(c != NULL);
    assert(c->fd != -1);

    s->last_errno = 0;

    if (epoll_ctl(s->epoll_fd, EPOLL_CTL_DEL, c->fd, NULL) == -1) {
        s->last_errno = errno;
        return SERVER_ERR_EPOLL_CTL;
    }

    server_untrack_conn(s, c);

    return SERVER_OK;
}

static void handle_server_event(server_t *s) {
    int client_fd;
    if ((client_fd = accept(s->fd, NULL, NULL)) == -1) {
        if (errno == EAGAIN) {
            return;
        }
        s->last_errno = errno;
        LOG_WARNING("accept() call failed: %s", strerror(s->last_errno));
        return;
    }

    conn_t *client = conn_create(client_fd);
    if (client == NULL) {
        s->last_errno = errno;
        LOG_WARNING("conn_create() call failed: %s", strerror(s->last_errno));
        close(client_fd);
        return;
    }

    server_status_t ret = server_add_conn(s, client);
    if (ret != SERVER_OK) {
        LOG_SERVER_ERROR("server_add_conn", ret, s->last_errno);
        conn_destroy(client);
        return;
    }

    LOG_INFO("Connection recieved: %d", client_fd);
}

static void handle_conn_event(server_t *s, conn_t *conn) {
    ssize_t bytes;
    if ((bytes = read(conn->fd, conn->buf, sizeof(conn->buf))) == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }

        s->last_errno = errno;
        LOG_WARNING("read() call failed: %s", strerror(s->last_errno));
        server_remove_conn(s, conn);
        conn_destroy(conn);
        return;
    }

    if (bytes == 0) {
        LOG_INFO("Client disconnected: %d", conn->fd);
        server_remove_conn(s, conn);
        conn_destroy(conn);
    } else {
        conn->buf_len = bytes;
        if (write(conn->fd, conn->buf, conn->buf_len) == -1) {
            s->last_errno = errno;
            LOG_WARNING("write() call failed: %s", strerror(s->last_errno));
            server_remove_conn(s, conn);
            conn_destroy(conn);
            return;
        }
    }
}

server_status_t server_poll(server_t *s) {
    assert(s != NULL);
    assert(s->fd != -1);
    assert(s->epoll_fd != -1);
    assert(s->port > 0);

    s->last_errno = 0;

    int n         = epoll_wait(s->epoll_fd, s->events, MAX_EVENTS, -1);
    if (n < 0) {
        s->last_errno = errno;
        if (errno == EINTR) {
            return SERVER_ERR_INTERRUPTED;
        }
        return SERVER_ERR_EPOLL_WAIT;
    }

    for (int i = 0; i < n; i++) {
        epoll_ctx_t *ctx = s->events[i].data.ptr;
        assert(ctx != NULL);
        assert(ctx->type != EPOLL_TYPE_UNKNOWN);

        switch (ctx->type) {
        case EPOLL_TYPE_SERVER: {
            assert(ctx->server != NULL);
            handle_server_event(ctx->server);
            break;
        }

        case EPOLL_TYPE_CONN: {
            assert(ctx->conn != NULL);
            handle_conn_event(s, ctx->conn);
            break;
        }

        default:
            assert(0 && "unhandled epoll_type_t");
            break;
        }
    }

    return SERVER_OK;
}

int server_get_errno(const server_t *s) {
    assert(s != NULL);
    return s->last_errno;
}
