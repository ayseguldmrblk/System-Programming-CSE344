#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <semaphore.h>
#include "common.h"

#define MAX_CLIENTS 50

typedef struct {
    pthread_t *threads;
    int thread_count;
    sem_t task_sem;
    pthread_mutex_t task_mutex;
    order_t *tasks;
    int task_index;
} thread_pool_t;

void init_thread_pool(thread_pool_t *pool, int thread_count, void *(*worker_func)(void *));

#endif