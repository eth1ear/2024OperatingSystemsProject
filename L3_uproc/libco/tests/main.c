#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "co-test.h"

int g_count = 0;

static void add_count() { g_count++; }

static int get_count() { return g_count; }

static void work_loop(void *arg) {
  const char *s = (const char *)arg;
  for (int i = 0; i < 100; ++i) {
    printf("%s%d  ", s, get_count());
    add_count();
    co_yield ();
  }
}

static void work(void *arg) { work_loop(arg); }

static void test_1() {
  struct co *thd1 = co_start("thread-1", work, "X");
  struct co *thd2 = co_start("thread-2", work, "Y");

  co_wait(thd1);
  co_wait(thd2);

  //    printf("\n");
}

// -----------------------------------------------

static int g_running = 1;

static void do_produce(Queue *queue) {
  assert(!q_is_full(queue));
  Item *item = (Item *)malloc(sizeof(Item));
  if (!item) {
    fprintf(stderr, "New item failure\n");
    return;
  }
  item->data = (char *)malloc(10);
  if (!item->data) {
    fprintf(stderr, "New data failure\n");
    free(item);
    return;
  }
  memset(item->data, 0, 10);
  sprintf(item->data, "libco-%d", g_count++);
  q_push(queue, item);
}

static void producer(void *arg) {
  Queue *queue = (Queue *)arg;
  for (int i = 0; i < 100;) {
    if (!q_is_full(queue)) {
      // co_yield();
      do_produce(queue);
      i += 1;
    }
    co_yield ();
  }
}

static void do_consume(Queue *queue) {
  assert(!q_is_empty(queue));

  Item *item = q_pop(queue);
  if (item) {
    printf("%s  ", (char *)item->data);
    free(item->data);
    free(item);
  }
}

static void consumer(void *arg) {
  Queue *queue = (Queue *)arg;
  while (g_running) {
    if (!q_is_empty(queue)) {
      do_consume(queue);
    }
    co_yield ();
  }
}

static void test_2() {
  Queue *queue = q_new();

  struct co *thd1 = co_start("producer-1", producer, queue);
  struct co *thd2 = co_start("producer-2", producer, queue);
  struct co *thd3 = co_start("consumer-1", consumer, queue);
  struct co *thd4 = co_start("consumer-2", consumer, queue);

  co_wait(thd1);
  co_wait(thd2);

  g_running = 0;

  co_wait(thd3);
  co_wait(thd4);

  while (!q_is_empty(queue)) {
    do_consume(queue);
  }

  q_free(queue);
}

// 创建10000个协程，将计数器累加到1e7，每次加一则co_yield
const int CNT = 1e7;
const int N = 100;
static void test_3_work(void *arg) {
  while (g_count < CNT) {
    add_count();
    co_yield ();
  }
}

pthread_spinlock_t count_spinlock;

void init_spinlock() { pthread_spin_init(&count_spinlock, 0); }

void *test_3_work_threaded(void *arg) {
  while (1) {
    pthread_spin_lock(&count_spinlock);
    if (g_count >= CNT) {
      pthread_spin_unlock(&count_spinlock);
      break;
    }

    g_count++;
    pthread_spin_unlock(&count_spinlock);
    // usleep(1);  // simulate co_yield with a short sleep
  }
  return NULL;
}

void test_3() {
  g_count = 0;
  struct co *thds[N];
  // 计算时间（精确）
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  for (int i = 0; i < N; ++i) {
    thds[i] = co_start("thread", test_3_work, NULL);
  }

  for (int i = 0; i < N; ++i) {
    co_wait(thds[i]);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  long long elapsed_ns =
      (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
  printf("average elapsed time: %lld ns\n", elapsed_ns / N);
  printf("g_count=%d\n", g_count);

  g_count = 0;
  pthread_t threads[N];

  // If using spinlock, initialize it
  init_spinlock();

  // 计算时间（精确）
  clock_gettime(CLOCK_MONOTONIC, &start);

  for (int i = 0; i < N; ++i) {
    pthread_create(&threads[i], NULL, test_3_work_threaded, NULL);
  }

  for (int i = 0; i < N; ++i) {
    pthread_join(threads[i], NULL);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  long long elapsed_ns_threaded =
      (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
  printf("average elapsed time for threads: %lld ns\n",
         elapsed_ns_threaded / N);
  printf("g_count=%d\n", g_count);
}

int main() {
  setbuf(stdout, NULL);

  printf("Test #1. Expect: (X|Y){0, 1, 2, ..., 199}\n");
  test_1();

  printf("\n\nTest #2. Expect: (libco-){200, 201, 202, ..., 399}\n");
  test_2();

  printf("\n\nefficiency test\n");
  test_3();

  return 0;
}
