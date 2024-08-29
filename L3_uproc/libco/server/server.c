#include <arpa/inet.h>
#include <co.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#define PORT 12345

static int client_fd = -1, server_fd = -1;
static int buffer_read;

void accept_wrapper(void *arg) {
  while (1) {
    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd >= 0) {
      printf("accept a client!\n");
      break;  // break out of loop if accept was successful
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
      Panic_On(1, "accept error");
    }
    // If no client yet, give up the CPU and check again later
    co_yield ();
  }
}

// static bool client_closed = false;
typedef struct recv_t {
  int fd;
  char *buf;
  int len;
} recv_t;

void recv_wrapper(void *arg) {
  recv_t *recv_arg = (recv_t *)arg;

  int bytesRead = 0;
  while (1) {
    bytesRead = recv(recv_arg->fd, recv_arg->buf, recv_arg->len, 0);
    buffer_read = bytesRead;
    if (bytesRead < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        co_yield ();  // No data available, yield and try again later
        continue;
      } else {
        // client_closed = true;
        break;  // broken pipe if client closed connection
      }
    } else {
      // Client closed the connection
      break;
    }
  }
}

typedef struct send_t {
  int fd;
  char *buf;
  int len;
} send_t;

void send_wrapper(void *arg) {
  send_t *send_arg = (send_t *)arg;

  int totalSent = 0;
  while (totalSent < send_arg->len) {
    int bytesSent = send(send_arg->fd, send_arg->buf + totalSent,
                         send_arg->len - totalSent, 0);
    if (bytesSent < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        co_yield ();  // Socket buffer full, yield and try again later
        continue;
      } else {
        // client_closed = true;
        break;  // broken pipe if client closed connection
      }
    } else {
      totalSent += bytesSent;
    }
  }
  printf("send: %s\n", send_arg->buf);
}

void session(void *arg) {
  // 分别创建recv_co和send_co协程
  char buf[1024];
  int session_fd = (int)arg;
  // 设置session_fd为非阻塞
  int flags = fcntl(session_fd, F_GETFL, 0);
  if (flags == -1) {
    Panic_On(1, "Failed to get file descriptor flags");
  }
  flags |= O_NONBLOCK;
  if (fcntl(session_fd, F_SETFL, flags) == -1) {
    Panic_On(1, "Failed to set file descriptor to non-blocking");
  }
  while (1) {
    memset(buf, 0, sizeof(buf));
    recv_t recv_arg = {session_fd, buf, sizeof(buf)};
    printf("recv_arg.fd: %d\n", recv_arg.fd);
    co_t *recv_co = co_start("recv", recv_wrapper, &recv_arg);
    co_wait(recv_co);
    if (buffer_read == 0) {
      break;
    }
    printf("recv: %s\n", buf);
    send_t send_arg = {session_fd, buf, buffer_read};
    co_t *send_co = co_start("send", send_wrapper, &send_arg);
    co_wait(send_co);
  }
  printf("session end\n");
}

void server() {
  // 创建一个socket
  struct sockaddr_in server_addr;
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  Panic_On(socket(AF_INET, SOCK_STREAM, 0) < 0, "socket error");
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  // 允许端口复用
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  // 绑定和监听
  Panic_On(
      bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0,
      "bind error");
  Panic_On(listen(server_fd, 10) < 0, "listen error");

  int flags = fcntl(server_fd, F_GETFL, 0);
  Panic_On(flags < 0, "fcntl F_GETFL error");
  flags |= O_NONBLOCK;
  Panic_On(fcntl(server_fd, F_SETFL, flags) < 0, "fcntl F_SETFL error");

  while (1) {
    co_t *co = co_start("accept", accept_wrapper, NULL);
    co_wait(co);
    printf("open a session to handle!\n");
    co = co_start("session", session, (void *)client_fd);
  }
}

int main() {
  signal(SIGPIPE, SIG_IGN);
  co_t *co = co_start("server", server, NULL);
  co_wait(co);
  return 0;
}