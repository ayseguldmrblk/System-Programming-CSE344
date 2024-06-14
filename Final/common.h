#ifndef COMMON_H
#define COMMON_H

#define ORDER_PLACED 1
#define ORDER_PREPARED 2
#define ORDER_COOKED 3
#define ORDER_DELIVERED 4
#define ORDER_CANCELLED 5

#define MAX_OVEN_TOOLS 3
#define MAX_DELIVERY_CAPACITY 3

typedef struct {
    int request_type; // Added field for request type
    int order_id;
    char client_address[256];
    char status[256]; // placed, prepared, cooked, delivered, canceled
    char data[1024]; // additional information if needed
} order_t;

typedef struct {
    int cook_id;
    int current_order_id; // -1 if no current order
    char status[256]; // available, busy
} cook_t;

typedef struct {
    int delivery_id;
    int current_orders[MAX_DELIVERY_CAPACITY]; // -1 if no current order
    char status[256]; // available, busy, on delivery
} delivery_person_t;

#endif
