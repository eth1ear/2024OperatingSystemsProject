#include <arpa/inet.h>
#include <co.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define PORT 12345
#define MAX_EVENTS 1024

static int server_fd = -1;
static int epoll_fd = -1;

void session(void *client_fd_ptr) {
  int client_fd = *((int *)client_fd_ptr);
  char buf[1024];

  while (1) {
    memset(buf, 0, sizeof(buf));
    int bytesRead = read(client_fd, buf, sizeof(buf));
    if (bytesRead <= 0) {
      if (bytesRead < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        co_yield ();  // If we are non-blocking, yield if no data.
        continue;
      }
      break;
    }
    printf("recv: %s\n", buf);
    write(client_fd, buf, bytesRead);
  }

  close(client_fd);
  epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
  free(client_fd_ptr);  // free the memory allocated for client_fd
}

void server() {
  struct sockaddr_in server_addr;
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket error");
    exit(1);
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("bind error");
    exit(1);
  }
  if (listen(server_fd, 10) < 0) {
    perror("listen error");
    exit(1);
  }

  epoll_fd = epoll_create1(0);
  if (epoll_fd < 0) {
    perror("epoll_create1");
    exit(1);
  }

  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = server_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
    perror("epoll_ctl");
    exit(1);
  }

  struct epoll_event events[MAX_EVENTS];
  while (1) {
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if (nfds < 0) {
      perror("epoll_wait");
      exit(1);
    }

    for (int i = 0; i < nfds; i++) {
      if (events[i].data.fd == server_fd) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
          perror("accept");
          continue;
        }

        // Set non-blocking
        int flags = fcntl(client_fd, F_GETFL, 0);
        flags |= O_NONBLOCK;
        fcntl(client_fd, F_SETFL, flags);

        ev.events = EPOLLIN;
        ev.data.fd = client_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
          perror("epoll_ctl");
          close(client_fd);
        } else {
          int *client_fd_ptr = malloc(sizeof(int));
          *client_fd_ptr = client_fd;
          co_start("session", session, client_fd_ptr);
        }
      } else {
        co_yield ();  // yield to let session co-routines handle the client
      }
    }
  }
}

int main() {
  signal(SIGPIPE, SIG_IGN);
  server();
  return 0;
}
