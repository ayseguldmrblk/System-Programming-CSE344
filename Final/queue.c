#include <stdlib.h>
#include "common.h"

void init_queue(queue_t* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
}

void enqueue(queue_t* queue, order_t order) {
    node_t* new_node = (node_t*)malloc(sizeof(node_t));
    if (!new_node) return;
    new_node->order = order;
    new_node->next = NULL;
    if (is_empty(queue)) {
        queue->head = new_node;
        queue->tail = new_node;
    } else {
        queue->tail->next = new_node;
        queue->tail = new_node;
    }
    queue->size++;
}

order_t dequeue(queue_t* queue) {
    order_t order;
    if (is_empty(queue)) {
        order.order_id = -1; // Indicate the queue is empty
        return order;
    }
    node_t* temp = queue->head;
    order = temp->order;
    queue->head = queue->head->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    free(temp);
    queue->size--;
    return order;
}

int is_empty(queue_t* queue) {
    return queue->head == NULL;
}
