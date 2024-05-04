#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef struct client_node_t
{
    int client_pid;
    struct client_node_t *next;
} client_node_t;

client_node_t *create_client_node(int client_pid)
{
    client_node_t *new_node = (client_node_t *)malloc(sizeof(client_node_t));
    if(new_node == NULL)
    {
        perror("Error allocating memory for new node");
        return NULL;
    }

    new_node->client_pid = client_pid;
    new_node->next = NULL;

    return new_node;
}

typedef struct queue_t
{
    client_node_t *front;
    client_node_t *rear;
    int size;
} queue_t;

queue_t *create_queue()
{
    queue_t *queue = (queue_t *)malloc(sizeof(queue_t));
    if(queue == NULL)
    {
        perror("Error allocating memory for queue");
        return NULL;
    }

    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;

    return queue;
}

int is_empty(queue_t *queue)
{
    return queue->size == 0;
}

void enqueue(queue_t *queue, int client_pid)
{
    client_node_t *new_node = create_client_node(client_pid);
    if(new_node == NULL)
    {
        return;
    }

    if(queue->rear == NULL)
    {
        queue->front = new_node;
        queue->rear = new_node;
    }
    else
    {
        queue->rear->next = new_node;
        queue->rear = new_node;
    }

    queue->size++;
}

int dequeue(queue_t *queue)
{
    if(is_empty(queue))
    {
        fprintf(stderr, "Queue is empty\n");
        return INT_MIN;
    }

    client_node_t *temp = queue->front;
    int client_pid = temp->client_pid;

    queue->front = queue->front->next;
    free(temp);

    if(queue->front == NULL)
    {
        queue->rear = NULL;
    }

    queue->size--;

    return client_pid;
}

void display_queue(queue_t *queue)
{
    if(is_empty(queue))
    {
        fprintf(stderr, "Queue is empty\n");
        return;
    }

    client_node_t *current = queue->front;
    while(current != NULL)
    {
        printf("%d ", current->client_pid);
        current = current->next;
    }
    printf("\n");
}

int peek(queue_t *queue)
{
    if(is_empty(queue))
    {
        fprintf(stderr, "Queue is empty\n");
        return INT_MIN;
    }

    return queue->front->client_pid;
}

int queue_size(queue_t *queue)
{
    return queue->size;
}

struct client_node_t* remove_client(queue_t *queue, int client_pid)
{
    if(is_empty(queue))
    {
        fprintf(stderr, "Queue is empty\n");
        return NULL;
    }

    client_node_t *current = queue->front;
    client_node_t *prev = NULL;

    while(current != NULL)
    {
        if(current->client_pid == client_pid)
        {
            if(prev == NULL)
            {
                queue->front = current->next;
            }
            else
            {
                prev->next = current->next;
            }

            if(current == queue->rear)
            {
                queue->rear = prev;
            }

            queue->size--;
            return current;
        }

        prev = current;
        current = current->next;
    }

    fprintf(stderr, "Client %d not found in queue\n", client_pid);
    return NULL;
}

int is_client_in_queue(queue_t *queue, int client_pid)
{
    if(is_empty(queue))
    {
        fprintf(stderr, "Queue is emptyyyyy\n");
        return 0;
    }

    client_node_t *current = queue->front;
    while(current != NULL)
    {
        if(current->client_pid == client_pid)
        {
            return 1;
        }

        current = current->next;
    }

    return 0;
}

#endif // QUEUE_H