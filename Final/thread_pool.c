#include "thread_pool.h"
#include <stdlib.h>

// Suppress deprecated warnings for sem_init
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

void init_thread_pool(thread_pool_t *pool, int thread_count, void *(*worker_func)(void *)) {
    pool->threads = malloc(sizeof(pthread_t) * thread_count);
    pool->thread_count = thread_count;
    sem_init(&pool->task_sem, 0, 0); // This line triggers the deprecation warning
    pthread_mutex_init(&pool->task_mutex, NULL);
    pool->tasks = malloc(sizeof(order_t) * MAX_CLIENTS);
    pool->task_index = 0;

    for (int i = 0; i < thread_count; i++) {
        pthread_create(&pool->threads[i], NULL, worker_func, pool);
    }
}

// Re-enable deprecated warnings
#pragma clang diagnostic pop
