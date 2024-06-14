#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include "common.h"

#define PORT 8080
#define MAX_CLIENTS 50
#define MAX_OVEN_CAPACITY 6
#define MAX_OVEN_TOOLS 3
#define MAX_DELIVERY_CAPACITY 3

sem_t *oven_sem;
sem_t *delivery_sem;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t order_mutex = PTHREAD_MUTEX_INITIALIZER;
order_t orders[MAX_CLIENTS];
int order_count = 0;
FILE *log_file;

void signal_handler(int sig) {
    if (sig == SIGINT) {
        sem_unlink("/oven_sem");
        sem_unlink("/delivery_sem");
        fclose(log_file);
        exit(0);
    }
}

void log_activity(const char *activity) {
    pthread_mutex_lock(&log_mutex);
    fprintf(log_file, "%s\n", activity);
    fflush(log_file);
    printf("%s\n", activity); // Also print to the console
    pthread_mutex_unlock(&log_mutex);
}

void *handle_client(void *client_socket) {
    int sock = *(int *)client_socket;
    free(client_socket);
    
    order_t order;
    read(sock, &order, sizeof(order));

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Received order %d from client %s", order.order_id, order.client_address);
    log_activity(log_msg);

    pthread_mutex_lock(&order_mutex);
    orders[order_count++] = order;
    pthread_mutex_unlock(&order_mutex);

    // Simulate cooking
    sem_wait(oven_sem); // Lock oven tool
    log_activity("Cooking order");

    sleep(2); // Simulate cooking time

    log_activity("Order cooked");
    sem_post(oven_sem); // Release oven tool

    // Simulate delivery
    sem_wait(delivery_sem); // Lock delivery personnel
    log_activity("Delivering order");

    sleep(1); // Simulate delivery time

    log_activity("Order delivered");
    sem_post(delivery_sem); // Release delivery personnel

    strcpy(order.status, "Delivered");
    send(sock, &order, sizeof(order), 0);

    close(sock);
    pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <cook_thread_pool_size> <delivery_thread_pool_size>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int cook_thread_pool_size = atoi(argv[2]);
    int delivery_thread_pool_size = atoi(argv[3]);

    // Signal handling
    signal(SIGINT, signal_handler);

    // Initialize semaphores
    oven_sem = sem_open("/oven_sem", O_CREAT, 0644, MAX_OVEN_TOOLS);
    delivery_sem = sem_open("/delivery_sem", O_CREAT, 0644, MAX_DELIVERY_CAPACITY);
    if (oven_sem == SEM_FAILED || delivery_sem == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    // Initialize log file
    log_file = fopen("pideshop.log", "a");
    if (log_file == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d\n", port);
    log_activity("Server started");

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        log_activity("Accepted new client connection");

        pthread_t tid;
        int *client_socket = malloc(sizeof(int));
        *client_socket = new_socket;
        pthread_create(&tid, NULL, handle_client, client_socket);
    }

    sem_close(oven_sem);
    sem_unlink("/oven_sem");
    sem_close(delivery_sem);
    sem_unlink("/delivery_sem");
    fclose(log_file);
    return 0;
}
