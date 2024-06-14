#include "delivery.h"
#include <math.h>

double calculate_distance(location_t a, location_t b) {
    return sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2));
}

double calculate_delivery_time(location_t shop, location_t customer, double velocity) {
    double distance = calculate_distance(shop, customer);
    return distance / velocity;
}
