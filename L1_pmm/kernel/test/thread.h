#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void create(void *fn);
void join();
void create(void *fn);
__attribute__((destructor)) void cleanup();

typedef int lock_t;
#define LOCK_INIT() 0
void lock(lock_t *lk);
void unlock(lock_t *lk);

// Mutex
typedef pthread_mutex_t mutex_t;
#define MUTEX_INIT() PTHREAD_MUTEX_INITIALIZER

// Conditional Variable
typedef pthread_cond_t cond_t;
#define COND_INIT() PTHREAD_COND_INITIALIZER
#define cond_wait pthread_cond_wait
#define cond_broadcast pthread_cond_broadcast
#define cond_signal pthread_cond_signal

// Semaphore
#define P sem_wait
#define V sem_post
#define SEM_INIT(sem, val) sem_init(sem, 0, val)

void mutex_lock(mutex_t *lk);
void mutex_unlock(mutex_t *lk);
