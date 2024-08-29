#include "stdio.h"
#include "ulib.h"

// -------------- debugging ----------------------
#define ASNI_FG_GREEN "\33[1;32m"
#define ASNI_FG_RED "\33[1;31m"
#define ASNI_NONE "\33[0m"
#define ASNI_FG_YELLOW "\33[1;33m"
#define panic_on(cond, s) \
  do {                    \
    if ((cond)) {         \
      printf(             \
          "\33[1;31m"s    \
          "\33[0m\n");    \
      exit(1);            \
    }                     \
  } while (0);
#define panic(s) panic_on(1, s)
#define PASS(s) printf(ASNI_FG_GREEN "[Passed]:" s ASNI_NONE "\n")
#define HINT(s) printf(ASNI_FG_YELLOW "[Hint]:" s ASNI_NONE "\n")
#define WIFEXITED(status) (((status) & 0x7f) == 0)
#define WEXITSTATUS(status) (((status) & 0xff00) >> 8)
static unsigned long int next = 1;
static int rand(void) {
  // RAND_MAX assumed to be 32767
  next = next * 1103515245 + 12345;
  return (unsigned int)(next / 65536) % 32768;
}
// ----------------------------------------------------

const char Hi[13] = "Hello World!\n";
void test_1() {  // 测试putchar和sleep
  for (int i = 0; i < 13; i++) {
    kputc(Hi[i]);
    sleep(0.1);
  }
  PASS("test_1");
}

void test_2() {
  //  简单时间测试：看起来不错
  int next = uptime() / 1000000 + 1;
  while (next <= 10) {
    while ((uptime() / 1000000) < next)
      ;
    printf("%d sec\n", next);
    next += 1;
  }
  PASS("test_2");
}

void test_3() {
  // 本测试用于fork
  int ret = fork();
  if (ret == 0) {  // 说明是子进程
    printf("This is child process in test_3\n");
    exit(0);
  } else {     // hh, 智能用putstr实在阴间
    sleep(1);  // 主要是为了更好的打印效果
    wait(NULL);
    printf("This is parent process and childId is %d\n", ret);
  }
  PASS("test_3");
}

void test_4() {
  // 简单的wait操作
  int r = rand();
  int p = r;
  int ret = fork();
  if (ret == 0) {
    printf("Hi, this is child process in test_4\n");
    sleep(2);
    p += 111;
    panic_on(p != r + 111, "test_4 fail");
    printf("child process is exiting\n");
    exit(128);
  } else {
    int xstatus;                     // 子进程的退出状态
    int child_pid = wait(&xstatus);  // wait的返回值是子进程的pid
    p += 111;
    panic_on(p != r + 111, "test_4 fail");
    panic_on(WIFEXITED(xstatus) != 1, "child not exited!");
    panic_on(WEXITSTATUS(xstatus) != 128 || child_pid != ret,
             "test_4 fail: wrong exit status!\n");
    printf("Hi, this is parent process!\n");
  }
  PASS("test_4");
}

void test_5() {
  // 简单的wait操作
  int ret = fork();
  if (ret == 0) {
    printf("Hi, this is child process in test_5\n");
    printf("Trying to kill child process\n");
    kill(getpid());  // 杀死子进程
    panic("Should not reach here!\n");
  } else {
    sleep(1);
    wait(NULL);  // wait的返回值是子进程的pid
    printf("Hi, this is parent process!\n");
  }
  PASS("test_5");
  kill(getpid());
  panic("Should not reach here!\n");
}

void test_6() {
  for (int i = 0; i < 256; i++) {
    int ret = fork();
    if (ret == 0) exit(0);
  }
  PASS("test_6::stress fork");
  // kill(getpid());
  // panic("Should not reach here!");
}

void test_7() {
  // 简单的wait操作:父进程可能会进入僵尸状态
  int ret = fork();
  if (ret == 0) {
    printf("Hi, this is child process in test_7\n");
    printf("child process is exiting\n");
    exit(128);
  } else {
    sleep(1);
    int xstatus;                     // 子进程的退出状态
    int child_pid = wait(&xstatus);  // wait的返回值是子进程的pid
    panic_on(WIFEXITED(xstatus) != 1, "child process should exit");
    panic_on(WEXITSTATUS(xstatus) != 128 || child_pid != ret, "test_7 fail\n");
    printf("Hi, this is parent process!\n");
    PASS("test_7");
  }
}

#define N (128)  // 比较强的测试
void test_8(void) {
  int n, pid;

  for (n = 0; n < N; n++) {
    pid = fork();
    if (pid < 0) break;
    if (pid == 0) {
      sleep(0.01);
      exit(0);
    }
  }

  if (n != N) {  // n == N?
    panic("fork claimed to work N times!\n");
  }

  for (; n > 0; n--) {
    if (wait(0) < 0) {
      panic("wait stopped early\n");
    }
  }

  if (wait(0) != -1) {
    panic("wait got too many\n");
  }
  sleep(0.5);
  PASS("test_8::fork and wait");
}

int test_9() {
  printf("%s\n", "Begin test_9");
  int pid = fork();

  if (pid != 0) {
    int status;
    int result = wait(&status);
    // 如果在调用wait()时子进程已经结束,
    // 则wait()会立即返回子进程结束状态值status(exit()的参数)：成功则为0,
    // 否则为对应的错误数字 如果执行成功则返回子进程识别码(PID),
    // 如果有错误发生则返回-1.
    // 用wait():1.希望子进程一定先完成2.父进程即将退出，为了避免僵尸进程
    if (result == -1 || status != 0) {
      panic("test_9 fail!");
    } else {
      printf("pass part 1\n");
    }

  } else {
    int second_pid = fork();
    if (second_pid != 0) {
      int new_status;
      int new_result = wait(&new_status);
      if (new_result == -1 || new_status != 0) {
        panic("test_9 fail!");
      } else {
        printf("pass part 2\n");
        exit(0);
      }

    } else {
      printf("pass part 3\n");
      exit(0);
    }
  }
  PASS("test_9");
  return 0;
}

void test_10() {
  HINT("1 should appear 5 times!");
  if (fork() || fork()) fork();
  printf("1 ");
  exit(0);
}

void test_11() {
  HINT("2 should appear 7 times!");
  if (fork() && (!fork())) {
    if (fork() || fork()) {
      fork();
    }
  }
  printf("2 ");
  exit(0);
}

void test_12() {
  HINT("@ should appear 257 times!");
  for (int i = 0; i < 8; i++) {
    fork();
  }
  printf("@");
  exit(0);
}

int test_13() {
  int procNum = 12;
  int loopNum = 10;
  int i, j;
  int pid;
  for (i = 0; i < procNum; ++i) {
    pid = fork();

    if (-1 == pid) {
      panic("fork err");
      return 0;
    }
    if (pid > 0) {
      ;
    }
    if (0 == pid) {
      for (j = 0; j < loopNum; ++j) {
        sleep(0.1);
      }
      return 0;
    }
  }
  PASS("test_13");
  return 0;
}

void test_18() {
  int p = 0;
  for (int i = 0; i < 6; i++) {
    fork();
    int r = rand();
    p = r + 555;
    panic_on(p != r + 555, "test_18 fail");
    // if(ret == 0) {
    //   exit(0);
    // }
  }
  if (getpid() == 0)
    PASS("test_18");
  else
    exit(0);
}
// ----------------------- test for mmap ----------------------
#define KiB (1024)
#define MiB (1024 * KiB)
#define GiB (1024 * MiB)
#define PG_SZ (4 * KiB)
#define M (512)
void test_14() {  // mmap easy test
  printf("Begin test_14\n");
  void *addr = (void *)(0x100000000000 + PG_SZ * 20);
  void *rladdr = mmap(addr, PG_SZ, PROT_READ | PROT_WRITE, MAP_SHARED);
  int *p = (int *)rladdr;
  *p = 0;
  for (int i = 0; i < M; i++) {
    int ret = fork();
    *p = *p + 1;
    if (ret == 0) exit(0);
  }
  for (int i = 0; i < M; i++) wait(0);
  if ((*p) != 2 * M) panic("test_14 fail");
  PASS("test_14");
}

#define R (512)
void test_15() {
  void *rladdr =
      mmap(NULL, 1 * GiB, PROT_READ | PROT_WRITE, MAP_PRIVATE);  // 1 GiB 的申请
  panic_on(rladdr == NULL, "mmap fail!");
  int *q = (int *)rladdr;
  *q = 555555;
  mmap(rladdr, 1 * GiB, PROT_READ | PROT_WRITE,
       MAP_UNMAP);  // 释放这128 GiB 的内存
  PASS("test_15(part 1)::1 GiB mmap size, single thread");

  rladdr = mmap(NULL, 1 * GiB, PROT_READ | PROT_WRITE, MAP_SHARED);
  int *p = (int *)rladdr;
  *p = R;
  for (int i = 0; i < R; i++) {
    int ret = fork();
    *p = *p + 1;
    if (ret == 0) exit(0);
  }
  for (int i = 0; i < R; i++)
    wait(0);  // 这一步是必要的，因为对于共享变量的读写必须确保子进程结束,
              // 下一行判断才是合理的
  if ((*p) != 3 * R) panic("test_15 fail");
  PASS("test_15(part 2)::1 GiB mmap size [shared], multi thread");
}

void test_16() {
  void *rladdr = mmap(NULL, 1 * GiB, PROT_READ | PROT_WRITE, MAP_PRIVATE);
  panic_on(rladdr == NULL, "mmap fail!");
  int *p = (int *)rladdr;
  int r = rand();
  *p = r;
  for (int i = 0; i < 128; i++) {
    int ret = fork();
    if (ret == 0) {
      *p += r;
      panic_on(*p != 2 * r, "test_16 fail");
      exit(0);
    }
  }
  for (int i = 0; i < 128; i++) wait(0);
  *p += 111;
  panic_on(*p != r + 111, "test_16 fail!");
  mmap(rladdr, 1 * GiB, PROT_READ | PROT_WRITE, MAP_UNMAP);
  if (getpid() != 0) exit(0);
  if (getpid() == 0) PASS("test_16::1 GiB mmap size [private], multi thread");
}

void test_17() {  // 不断的 map 和 unmap
  for (volatile int i = 0; i < 1; i++) {
    void *rladdr = mmap(NULL, 1 * GiB, PROT_READ | PROT_WRITE, MAP_PRIVATE);
    volatile int *p = (int *)(rladdr + rand() % 8192);
    int r = rand() % 1000;
    *p = r;
    *p = *p + 111;
    panic_on(*p != r + 111, "test_17 fail");
    mmap(rladdr, 1 * GiB, PROT_READ | PROT_WRITE, MAP_UNMAP);
  }
  PASS("test_17::stress 1 GiB mmap & unmap size [private], single thread");
}

void test_19() {
  fork();
  fork();
  for (int i = 0; i < R; i++) {
    int r1 = rand();
    int size = r1 * 10;
    void *rladdr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE);
    int *p = (int *)(rladdr);
    int r = rand();
    *p = r + 123456789;
    panic_on(*p != r + 123456789, "test_19 fail");
    mmap(rladdr, size, PROT_READ | PROT_WRITE, MAP_UNMAP);
  }
  if (getpid() == 0)
    PASS("test_19:stress rand size mmap[private], multi thread");
  else
    exit(0);
}
int main() {
  test_1();
  test_2();
  // test_3();
  // test_4();
  // test_5();
  // test_6();
  // test_7();
  // test_8();
  // test_9();
  // test_10();
  // test_11();
  // test_12();
  // test_13();
  // test_14();
  // test_15();
  // test_16();
  // test_17();
  // test_18();
  // test_19();
  return 0;
}
