/**
 * 基于epoll和thread的接口实现echo服务器
 * Author: Xuqin Zhang
 * Date: 2023.9.19
 */

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 12345

int main() {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    exit(1);
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;    // IPv4
  server_addr.sin_port = htons(PORT);  // 端口
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("bind");
    exit(1);
  }
  if (listen(server_fd, 10) < 0) {
    perror("listen");
    exit(1);
  }

  int epoll_fd = epoll_create(1);
  if (epoll_fd < 0) {
    perror("epoll_create");
    exit(1);
  }

  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = server_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
    perror("epoll_ctl");
    exit(1);
  }

  struct epoll_event events[10];
  while (1) {
    int nfds = epoll_wait(epoll_fd, events, 10, -1);
    if (nfds < 0) {
      perror("epoll_wait");
      exit(1);
    }
    for (int i = 0; i < nfds; i++) {
      if (events[i].data.fd == server_fd) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd =
            accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
          perror("accept");
          exit(1);
        }
        ev.events = EPOLLIN;
        ev.data.fd = client_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
          perror("epoll_ctl");
          exit(1);
        }
      } else {
        char buf[1024];
        memset(buf, 0, sizeof(buf));
        int n = read(events[i].data.fd, buf, sizeof(buf));
        if (n < 0) {
          perror("read");
          exit(1);
        }
        if (n == 0) {
          close(events[i].data.fd);
          epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
          continue;
        }
        printf("recv: %s\n", buf);
        n = write(events[i].data.fd, buf, n);
        if (n < 0) {
          perror("write");
          exit(1);
        }
      }
    }
  }
}