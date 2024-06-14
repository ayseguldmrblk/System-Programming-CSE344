// common.h
#ifndef COMMON_H
#define COMMON_H

#define ORDER_PLACED 1
#define ORDER_PREPARED 2
#define ORDER_COOKED 3
#define ORDER_DELIVERED 4
#define ORDER_CANCELLED 5

typedef struct {
    int request_type;
    int order_id;
    char client_address[256];
    char status[256];
    char data[1024];
} order_t;

#endif
