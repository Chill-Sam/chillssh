#include "log.h"
#include <errno.h> // IWYU pragma: keep
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    LOG_ERROR("Usage: ./chillssh [PORT_NUMBER]");
    exit(-1);
  }

  int16_t server_port = atoi(argv[1]);

  int tcp_socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (tcp_socket_fd < 0) {
    LOG_ERROR("socket() call failed with errno: %d", errno);
    exit(-1);
  }

  int opt = 1;
  if (setsockopt(tcp_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
      -1) {
    LOG_ERROR("setsockopt() call failed with errno: %d", errno);
    exit(-1);
  }

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(server_port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(tcp_socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    LOG_ERROR("bind() call failed with errno: %d", errno);
    exit(-1);
  }

  if (listen(tcp_socket_fd, SOMAXCONN) == -1) {
    LOG_ERROR("listen() call failed with errno: %d", errno);
    exit(-1);
  }

  while (true) {
    int new_socket;
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    if ((new_socket = accept(tcp_socket_fd, (struct sockaddr *)&client,
                             &client_len)) == -1) {
      LOG_WARNING("accept() failed with errno: %d", errno);
      continue;
    }

    LOG_INFO("Connection recieved!");

    char *msg = "Hello World!\n";
    if (write(new_socket, msg, strlen(msg)) == -1) {
      LOG_WARNING("write() failed with errno: %d", errno);
    }
    close(new_socket);
  }

  close(tcp_socket_fd);
  return 0;
}
