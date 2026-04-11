#include "log.h"
#include <asm-generic/errno-base.h>
#include <errno.h> // IWYU pragma: keep
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTS 64
#define BUF_SIZE 4096

int main(int argc, char *argv[]) {
  if (argc != 2) {
    LOG_ERROR("Usage: ./chillssh [PORT_NUMBER]");
    exit(-1);
  }

  int server_port = atoi(argv[1]);

  int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_fd < 0) {
    LOG_ERROR("socket() call failed with errno: %d", errno);
    exit(-1);
  }

  int opt = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
      -1) {
    LOG_ERROR("setsockopt() call failed with errno: %d", errno);
    exit(-1);
  }

  struct sockaddr_in s_addr;
  s_addr.sin_family = AF_INET;
  s_addr.sin_port = htons(server_port);
  s_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(socket_fd, (struct sockaddr *)&s_addr, sizeof(s_addr)) == -1) {
    LOG_ERROR("bind() call failed with errno: %d", errno);
    exit(-1);
  }

  if (listen(socket_fd, SOMAXCONN) == -1) {
    LOG_ERROR("listen() call failed with errno: %d", errno);
    exit(-1);
  }

  int flags;
  if ((flags = fcntl(socket_fd, F_GETFL)) == -1) {
    LOG_ERROR("fctnl() call failed with errno: %d", errno);
    exit(-1);
  }
  if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK)) {
    LOG_ERROR("fctnl() call failed with errno: %d", errno);
    exit(-1);
  }

  int epoll_fd;
  if ((epoll_fd = epoll_create1(0)) == -1) {
    LOG_ERROR("epoll_create1() call failed with errno: %d", errno);
    exit(-1);
  }

  struct epoll_event ev = {.events = EPOLLIN, .data.fd = socket_fd};
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &ev) == -1) {
    LOG_ERROR("epoll_ctl() call failed with errno: %d", errno);
    close(socket_fd);
    exit(-1);
  }

  struct epoll_event events[MAX_EVENTS];
  char buf[BUF_SIZE];

  while (true) {
    int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

    for (int i = 0; i < n; i++) {
      int fd = events[i].data.fd;

      if (fd == socket_fd) {
        // New client
        int client_fd;
        if ((client_fd = accept(socket_fd, NULL, NULL)) == -1) {
          LOG_WARNING("accept() failed with errno: %d", errno);
          continue;
        }

        int client_fl;
        if ((client_fl = fcntl(socket_fd, F_GETFL)) == -1) {
          LOG_WARNING("fctnl() call failed with errno: %d", errno);
          close(client_fd);
          continue;
        }
        if (fcntl(client_fd, F_SETFL, client_fl | O_NONBLOCK)) {
          LOG_WARNING("fctnl() call failed with errno: %d", errno);
          close(client_fd);
          continue;
        }

        struct epoll_event ev = {.events = EPOLLIN, .data.fd = client_fd};
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
          LOG_WARNING("epoll_ctl() call failed with errno: %d", errno);
          close(client_fd);
          continue;
        }

        LOG_INFO("Connection recieved: %d", client_fd);
      } else {
        // Data from connection
        int bytes;
        if ((bytes = read(fd, buf, sizeof(buf))) == -1) {
          if (errno == EAGAIN) {
            // No data to be read, so we just continue
            continue;
          }

          LOG_WARNING("read() call failed with errno: %d", errno);

          // Disconnect client
          if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) {
            LOG_WARNING("epoll_ctl() call failed with errno: %d", errno);
            close(fd);
            continue;
          }

          close(fd);

          continue;
        }

        if (bytes == 0) {
          LOG_INFO("Client disconnected: %d", fd);
          if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) {
            LOG_WARNING("epoll_ctl() call failed with errno: %d", errno);
            close(fd);
            continue;
          }

          close(fd);
        } else {
          if (write(fd, buf, bytes) == -1) {
            LOG_WARNING("write() call failed with errno: %d", errno);
            continue;
          }
        }
      }
    }
  }

  close(socket_fd);
  close(epoll_fd);
  return 0;
}
