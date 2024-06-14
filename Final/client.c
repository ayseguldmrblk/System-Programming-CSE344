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
int num_clients;
char server_address[256];
int port;
int *client_sockets;

void cancel_orders(int sig) {
    if (sig == SIGINT) {
        for (int i = 0; i < num_clients; i++) {
            if (client_sockets[i] != -1) {
                order_t order;
                order.request_type = ORDER_CANCELLED;
                order.order_id = i; // Cancel the corresponding order
                send(client_sockets[i], &order, sizeof(order), 0);
                close(client_sockets[i]);
                client_sockets[i] = -1; // Mark this socket as closed
            }
        }
        exit(0);
    }
}

void *place_order(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    order_t order;
    order.request_type = ORDER_PLACED;
    order.order_id = __sync_fetch_and_add(&order_id, 1); // Thread-safe increment

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
        if (strcmp(order.status, "Delivered") == 0 || strcmp(order.status, "Cancelled") == 0) break;
    }

    close(sock);
    pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <server_address> <port> <num_clients> <p> <q>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    strcpy(server_address, argv[1]);
    port = atoi(argv[2]);
    num_clients = atoi(argv[3]);
    p = atof(argv[4]);
    q = atof(argv[5]);

    // Seed the random number generator
    srand(time(NULL));

    // Allocate memory for client sockets
    client_sockets = malloc(num_clients * sizeof(int));
    if (!client_sockets) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Signal handling
    signal(SIGINT, cancel_orders);

    pthread_t tid[num_clients];

    for (int i = 0; i < num_clients; i++) {
        int *sock = malloc(sizeof(int));
        if (sock == NULL) {
            perror("malloc");
            return -1;
        }

        if ((*sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("Socket creation error");
            free(sock);
            return -1;
        }

        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);

        if (inet_pton(AF_INET, server_address, &serv_addr.sin_addr) <= 0) {
            perror("Invalid address/ Address not supported");
            free(sock);
            return -1;
        }

        if (connect(*sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Connection failed");
            free(sock);
            return -1;
        }

        client_sockets[i] = *sock; // Store the socket descriptor
        pthread_create(&tid[i], NULL, place_order, sock);
    }

    for (int i = 0; i < num_clients; i++) {
        pthread_join(tid[i], NULL);
    }

    free(client_sockets);
    return 0;
}
