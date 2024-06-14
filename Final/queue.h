#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"

typedef struct node {
    order_t order;
    struct node* next;
} node_t;

typedef struct {
    node_t* head;
    node_t* tail;
    int size;
} queue_t;

void init_queue(queue_t* queue);
void enqueue(queue_t* queue, order_t order);
order_t dequeue(queue_t* queue);
int is_empty(queue_t* queue);

#endif
