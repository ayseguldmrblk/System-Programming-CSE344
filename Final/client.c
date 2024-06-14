#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include "common.h"

#define PORT 8080

int order_id = 0;

void signal_handler(int sig) {
    if (sig == SIGINT) {
        exit(0);
    }
}

void *place_order(void *arg) {
    int sock = *(int *)arg;
    order_t order;
    order.request_type = ORDER_PLACED;
    order.order_id = order_id++;
    strcpy(order.client_address, "Client Address");
    strcpy(order.status, "Placed");

    send(sock, &order, sizeof(order), 0);

    read(sock, &order, sizeof(order));
    printf("Order %d: %s\n", order.order_id, order.status);

    close(sock);
    pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <server_address> <num_clients>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_address = argv[1];
    int num_clients = atoi(argv[2]);

    // Signal handling
    signal(SIGINT, signal_handler);

    pthread_t tid[num_clients];

    for (int i = 0; i < num_clients; i++) {
        int sock = 0;
        struct sockaddr_in serv_addr;

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            printf("\n Socket creation error \n");
            return -1;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);

        if (inet_pton(AF_INET, server_address, &serv_addr.sin_addr) <= 0) {
            printf("\nInvalid address/ Address not supported \n");
            return -1;
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("\nConnection Failed \n");
            return -1;
        }

        int *client_socket = malloc(sizeof(int));
        *client_socket = sock;
        pthread_create(&tid[i], NULL, place_order, client_socket);
    }

    for (int i = 0; i < num_clients; i++) {
        pthread_join(tid[i], NULL);
    }

    return 0;
}
