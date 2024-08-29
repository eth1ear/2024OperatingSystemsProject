#include<stdio.h>
#include<stdlib.h>
#include<stddef.h>
#include<unistd.h>
#include<sys/wait.h>
#include<assert.h>

#define ASNI_FG_GREEN   "\33[1;32m"
#define ASNI_FG_RED     "\33[1;31m"
#define ASNI_NONE       "\33[0m"
#define panic_on(cond, s) \
  do {\
    if(!(cond)){\
    printf(ASNI_FG_RED s ASNI_NONE);\
    exit(1);\
    }\
  }while(0);
#define panic(s) panic_on(1, s)
#define PASS(s) \
  printf(ASNI_FG_GREEN"[Passed]:" s ASNI_NONE"\n")
// #define WIFEXITED(status) (((status) & 0x7f) == 0)
// #define WEXITSTATUS(status) (((status) & 0xff00) >> 8)

void test_3() {
  // 本测试用于fork
  int ret = fork();
  if(ret == 0) {  // 说明是子进程
    printf("This is child process in test_3\n");
  } else { // hh, 智能用putstr实在阴间
    sleep(1); // 主要是为了更好的打印效果
    wait(NULL);
    printf("This is parent process and childId is %d\n", ret);
  }
}

void test_4() {
  // 简单的wait操作
  int ret = fork();
  if(ret == 0) {
    printf("Hi, this is child process in test_4\n");
    sleep(2);
    printf("child process is exiting\n");
    exit(128);
  } else {
    int xstatus; // 子进程的退出状态
    int child_pid = wait(&xstatus); // wait的返回值是子进程的pid
    assert(WIFEXITED(xstatus));
    printf("Hi, this is parent process!\n");
    if(WEXITSTATUS(xstatus) != 128 || child_pid != ret) {
      printf("Wrong answer!\n");
    } else {
      printf("Pass test_4!\n");
    }
  }
}

int id = 0;
void test_6() {
  for(int i = 0; i < 100; i++) {
    int ret = fork();
    if(ret == 0) break;
  }
  id += 1;
  printf("Hello form #%d in test_6\n", id);
  while(1); // 不回收资源
}

#define N (1000)
void test_8(void) {
  int n, pid;

  printf("fork test\n");

  for(n=0; n<N; n++){
    pid = fork();
    if(pid < 0)
      break;
    if(pid == 0) // 如果是子进程那么直接退出
      exit(0);
  }

  if(n == N){
    panic("fork claimed to work N times!\n");
  }

  for(; n > 0; n--){
    int ret = wait(0);
    printf("wait的返回值是:%d\n", ret);
    if(ret < 0){
      panic("wait stopped early\n");
    }
  }

  if(wait(0) != -1){
    panic("wait got too many\n");
  }

  PASS("fork test OK");
}

int test_9() {
  printf("%s\n", "我是鸣人！");
  int pid = fork();
    
  if(pid != 0){
      int status;
      int result = wait(&status);
        //如果在调用wait()时子进程已经结束, 则wait()会立即返回子进程结束状态值status(exit()的参数)：成功则为0, 否则为对应的错误数字
        //如果执行成功则返回子进程识别码(PID), 如果有错误发生则返回-1.
        //用wait():1.希望子进程一定先完成2.父进程即将退出，为了避免僵尸进程
      if(result == -1 || status != 0){
            printf("%s\n", "可恶，又失败了，再来一次！");
            return -1;
        }else{
            printf("%s\n", "我负责性质变化！");
        }
        
    }else{
    int second_pid = fork();
        if(second_pid != 0){
            int new_status;
            int new_result = wait(&new_status);
            if(new_result == -1 || new_status != 0){
                exit(-1);
            }else{
                printf("%s\n", "我负责形态变化！");
              exit(0);                
            }
            
        }else{
            printf("%s\n", "我负责产生查克拉！");
            exit(0);
        }
    }
}

void test_10() {
  int ret = fork();
  if(ret == 0)exit(0);
  else printf("子进程!\n");
}

int main() {
  // test_1();
  // test_2();
  // test_3();
  // test_4();
  // test_6();
  // test_8();
  // test_9();
  // test_10();
  return 0;
}
