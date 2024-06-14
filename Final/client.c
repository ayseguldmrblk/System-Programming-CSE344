#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include "common.h"

int order_id = 0;
double p, q;

void signal_handler(int sig) {
    if (sig == SIGINT) {
        exit(0);
    }
}

void *place_order(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    order_t order;
    order.request_type = ORDER_PLACED;
    order.order_id = order_id++;

    // Generate a random location for the client
    double x = ((double)rand() / RAND_MAX) * p;
    double y = ((double)rand() / RAND_MAX) * q;
    snprintf(order.client_address, sizeof(order.client_address), "%.2lf %.2lf", x, y);
    strcpy(order.status, "Placed");

    send(sock, &order, sizeof(order), 0);

    while (1) {
        int n = read(sock, &order, sizeof(order));
        if (n <= 0) break;
        printf("Order %d: %s (Cook %d, Moto %d)\n", order.order_id, order.status, order.cook_id, order.delivery_id);
        if (strcmp(order.status, "Delivered") == 0) break;
    }

    close(sock);
    pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <server_address> <port> <num_clients> <p> <q>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_address = argv[1];
    int port = atoi(argv[2]);
    int num_clients = atoi(argv[3]);
    p = atof(argv[4]);
    q = atof(argv[5]);

    // Signal handling
    signal(SIGINT, signal_handler);

    pthread_t tid[num_clients];

    for (int i = 0; i < num_clients; i++) {
        int *sock = malloc(sizeof(int));
        if ((*sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Socket creation error");
            return -1;
        }

        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);

        if (inet_pton(AF_INET, server_address, &serv_addr.sin_addr) <= 0) {
            perror("Invalid address/ Address not supported");
            return -1;
        }

        if (connect(*sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Connection failed");
            return -1;
        }

        pthread_create(&tid[i], NULL, place_order, sock);
    }

    for (int i = 0; i < num_clients; i++) {
        pthread_join(tid[i], NULL);
    }

    return 0;
}
