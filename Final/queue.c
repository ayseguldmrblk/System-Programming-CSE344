#include "common.h"
#include <stdlib.h>

void init_queue(queue_t* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
}

void enqueue(queue_t* queue, order_t order) {
    node_t* new_node = (node_t*)malloc(sizeof(node_t));
    new_node->order = order;
    new_node->next = NULL;

    if (queue->tail == NULL) {
        queue->head = new_node;
    } else {
        queue->tail->next = new_node;
    }
    queue->tail = new_node;
    queue->size++;
}

order_t dequeue(queue_t* queue) {
    if (queue->head == NULL) {
        exit(EXIT_FAILURE); // Queue is empty
    }

    node_t* temp = queue->head;
    order_t order = temp->order;
    queue->head = queue->head->next;

    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    free(temp);
    queue->size--;
    return order;
}

int is_empty(queue_t* queue) {
    return queue->size == 0;
}
