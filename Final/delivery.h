#ifndef DELIVERY_H
#define DELIVERY_H

// Delivery velocity (km/h)
#define DELIVERY_VELOCITY 2.0

typedef struct {
    double x;
    double y;
} location_t;

double calculate_distance(location_t a, location_t b);
double calculate_delivery_time(location_t shop, location_t customer, double velocity);

#endif
