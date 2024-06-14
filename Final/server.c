#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include "common.h"
#include "thread_pool.h"
#include "delivery.h"
#include "complex_matrix.h"

#define SERVER_BACKLOG 200

// Delivery velocity (m/min)
int delivery_speed;

sem_t *oven_sem;
sem_t *oven_capacity_sem;
sem_t *delivery_sem;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t order_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t delivery_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t id_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t order_cond = PTHREAD_COND_INITIALIZER;
queue_t order_queue;
queue_t oven_queue;
queue_t delivery_queue;
FILE *log_file;

thread_pool_t cook_task_pool;
thread_pool_t delivery_task_pool;

location_t shop_location;

void signal_handler(int sig) {
    if (sig == SIGINT) {
        sem_unlink("/oven_sem");
        sem_unlink("/oven_capacity_sem");
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

int get_unique_cook_id() {
    static int cook_id_counter = 1;
    pthread_mutex_lock(&id_mutex);
    int id = cook_id_counter++;
    pthread_mutex_unlock(&id_mutex);
    return id;
}

int get_unique_delivery_id() {
    static int delivery_id_counter = 1;
    pthread_mutex_lock(&id_mutex);
    int id = delivery_id_counter++;
    pthread_mutex_unlock(&id_mutex);
    return id;
}

void remove_order_from_queue(queue_t* queue, int order_id) {
    pthread_mutex_lock(&order_mutex);
    queue_t temp_queue;
    init_queue(&temp_queue);

    while (!is_empty(queue)) {
        order_t order = dequeue(queue);
        if (order.order_id != order_id) {
            enqueue(&temp_queue, order);
        } else {
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "Order %d: Cancelled", order_id);
            log_activity(log_msg);
        }
    }

    while (!is_empty(&temp_queue)) {
        enqueue(queue, dequeue(&temp_queue));
    }

    pthread_mutex_unlock(&order_mutex);
}

void update_client(order_t order, const char *status) {
    strcpy(order.status, status);
    send(order.sock, &order, sizeof(order), 0);
}

void *cook_worker(void *arg) {
    int cook_id = *(int *)arg;
    thread_pool_t *pool = &cook_task_pool;
    
    while (1) {
        sem_wait(&pool->task_sem);
        pthread_mutex_lock(&pool->task_mutex);

        if (pool->task_index > 0) {
            // Get the next task
            order_t task = pool->tasks[--pool->task_index];
            pthread_mutex_unlock(&pool->task_mutex);

            // Update cook information
            task.cook_id = cook_id;

            // Simulate cooking by calculating pseudo-inverse of a complex matrix
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "Cook %d: Cooking order %d", cook_id, task.order_id);
            log_activity(log_msg);
            update_client(task, "Cooking");

            complex_matrix_t *matrix = create_matrix(30, 40);
            generate_random_matrix(matrix);
            time_t start = time(NULL);
            complex_matrix_t *inverse = pseudo_inverse(matrix);
            time_t end = time(NULL);
            double preparation_time = difftime(end, start);
            free_matrix(matrix);
            free_matrix(inverse);

            // Add to oven queue
            sem_wait(oven_capacity_sem); // Lock oven capacity
            sem_wait(oven_sem); // Lock oven tool
            snprintf(log_msg, sizeof(log_msg), "Cook %d: Placing order %d in the oven", cook_id, task.order_id);
            log_activity(log_msg);
            update_client(task, "In Oven");
            enqueue(&oven_queue, task);
            snprintf(log_msg, sizeof(log_msg), "Cook %d: Order %d placed in the oven", cook_id, task.order_id);
            log_activity(log_msg);
            sem_post(oven_sem); // Release oven tool

            // Simulate cooking time
            sleep(preparation_time / 2);

            // Remove from oven queue
            sem_wait(oven_sem); // Lock oven tool
            snprintf(log_msg, sizeof(log_msg), "Cook %d: Removing order %d from the oven", cook_id, task.order_id);
            log_activity(log_msg);
            dequeue(&oven_queue);
            snprintf(log_msg, sizeof(log_msg), "Cook %d: Order %d removed from the oven", cook_id, task.order_id);
            log_activity(log_msg);
            sem_post(oven_capacity_sem); // Release oven capacity
            sem_post(oven_sem); // Release oven tool

            snprintf(log_msg, sizeof(log_msg), "Cook %d: Order %d cooked", cook_id, task.order_id);
            log_activity(log_msg);
            update_client(task, "Cooked");

            // Add to delivery task pool
            pthread_mutex_lock(&delivery_task_pool.task_mutex);
            delivery_task_pool.tasks[delivery_task_pool.task_index++] = task;
            pthread_mutex_unlock(&delivery_task_pool.task_mutex);
            sem_post(&delivery_task_pool.task_sem);
        } else {
            pthread_mutex_unlock(&pool->task_mutex);
        }
    }
    return NULL;
}

void *delivery_worker(void *arg) {
    int delivery_id = *(int *)arg;
    thread_pool_t *pool = &delivery_task_pool;

    while (1) {
        sem_wait(&pool->task_sem);
        pthread_mutex_lock(&pool->task_mutex);

        if (pool->task_index > 0) {
            // Get the next task
            order_t task = pool->tasks[--pool->task_index];
            pthread_mutex_unlock(&pool->task_mutex);

            // Update delivery information
            task.delivery_id = delivery_id;

            // Simulate delivery
            sem_wait(delivery_sem); // Lock delivery personnel
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), "Moto %d: Delivering order %d", delivery_id, task.order_id);
            log_activity(log_msg);
            update_client(task, "Delivering");

            // Calculate delivery time based on customer location
            location_t customer_location;
            sscanf(task.client_address, "%lf %lf", &customer_location.x, &customer_location.y);
            double delivery_time = calculate_delivery_time(shop_location, customer_location, delivery_speed);

            sleep(delivery_time); // Simulate delivery time

            snprintf(log_msg, sizeof(log_msg), "Moto %d: Order %d delivered", delivery_id, task.order_id);
            log_activity(log_msg);
            sem_post(delivery_sem); // Release delivery personnel

            // Update client about the delivery
            update_client(task, "Delivered");
            close(task.sock);
        } else {
            pthread_mutex_unlock(&pool->task_mutex);
        }
    }
    return NULL;
}

void *handle_client(void *client_socket) {
    int sock = *(int *)client_socket;
    free(client_socket);

    order_t order;
    read(sock, &order, sizeof(order));

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Received order %d from client %s", order.order_id, order.client_address);
    log_activity(log_msg);

    if (order.request_type == ORDER_CANCELLED) {
        // Handle order cancellation
        remove_order_from_queue(&order_queue, order.order_id);
        remove_order_from_queue(&oven_queue, order.order_id);
        remove_order_from_queue(&delivery_queue, order.order_id);
        update_client(order, "Cancelled");
        close(sock);
        pthread_exit(NULL);
    }

    order.sock = sock;  // Save socket descriptor for later updates

    // Add to order queue
    pthread_mutex_lock(&order_mutex);
    enqueue(&order_queue, order);
    pthread_cond_signal(&order_cond);
    pthread_mutex_unlock(&order_mutex);

    pthread_exit(NULL);
}

void *manager_worker(void *arg) {
    while (1) {
        pthread_mutex_lock(&order_mutex);
        while (is_empty(&order_queue)) {
            pthread_cond_wait(&order_cond, &order_mutex);
        }
        order_t order = dequeue(&order_queue);
        pthread_mutex_unlock(&order_mutex);

        // Add to cook task pool
        pthread_mutex_lock(&cook_task_pool.task_mutex);
        cook_task_pool.tasks[cook_task_pool.task_index++] = order;
        pthread_mutex_unlock(&cook_task_pool.task_mutex);
        sem_post(&cook_task_pool.task_sem);
    }
    return NULL;
}

void print_server_info(const char *ip_address, int port) {
    printf("Server started on IP %s and port %d\n", ip_address, port);
}

int main(int argc, char const *argv[]) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if (argc != 5) {
        fprintf(stderr, "Usage: %s <port> <cook_thread_pool_size> <delivery_thread_pool_size> <speed_m_per_min>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int cook_thread_pool_size = atoi(argv[2]);
    int delivery_thread_pool_size = atoi(argv[3]);
    delivery_speed = atoi(argv[4]); // Speed in m/min

    // Get the IP address of the en0 interface
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    char *IPbuffer = NULL;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET && strcmp(ifa->ifa_name, "en0") == 0) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            IPbuffer = strdup(host); // Duplicate the string
            break;
        }
    }

    freeifaddrs(ifaddr);

    if (IPbuffer == NULL) {
        fprintf(stderr, "Failed to get IP address of en0 interface\n");
        exit(EXIT_FAILURE);
    }

    // Signal handling
    signal(SIGINT, signal_handler);

    // Initialize semaphores
    oven_sem = sem_open("/oven_sem", O_CREAT, 0644, MAX_OVEN_TOOLS);
    oven_capacity_sem = sem_open("/oven_capacity_sem", O_CREAT, 0644, MAX_OVEN_CAPACITY);
    delivery_sem = sem_open("/delivery_sem", O_CREAT, 0644, MAX_DELIVERY_CAPACITY);
    if (oven_sem == SEM_FAILED || delivery_sem == SEM_FAILED || oven_capacity_sem == SEM_FAILED) {
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
    address.sin_addr.s_addr = inet_addr(IPbuffer); // Bind to the host's IP address
    address.sin_port = htons(port);

    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, SERVER_BACKLOG) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    print_server_info(IPbuffer, port);
    log_activity("Server started");

    // Initialize cook and delivery thread pools
    init_thread_pool(&cook_task_pool, cook_thread_pool_size, cook_worker);
    init_thread_pool(&delivery_task_pool, delivery_thread_pool_size, delivery_worker);

    // Initialize queues
    init_queue(&order_queue);
    init_queue(&oven_queue);
    init_queue(&delivery_queue);

    // Start manager thread
    pthread_t manager_tid;
    pthread_create(&manager_tid, NULL, manager_worker, NULL);

    // Create cook and delivery worker threads
    int *cook_ids = malloc(cook_thread_pool_size * sizeof(int));
    int *delivery_ids = malloc(delivery_thread_pool_size * sizeof(int));
    for (int i = 0; i < cook_thread_pool_size; i++) {
        cook_ids[i] = get_unique_cook_id();  // Assign unique ID
        pthread_create(&cook_task_pool.threads[i], NULL, cook_worker, &cook_ids[i]);
    }

    for (int i = 0; i < delivery_thread_pool_size; i++) {
        delivery_ids[i] = get_unique_delivery_id();  // Assign unique ID
        pthread_create(&delivery_task_pool.threads[i], NULL, delivery_worker, &delivery_ids[i]);
    }

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

    // Clean up resources
    for (int i = 0; i < cook_thread_pool_size; i++) {
        pthread_join(cook_task_pool.threads[i], NULL);
    }
    for (int i = 0; i < delivery_thread_pool_size; i++) {
        pthread_join(delivery_task_pool.threads[i], NULL);
    }
    free(cook_ids);
    free(delivery_ids);
    sem_close(oven_sem);
    sem_unlink("/oven_sem");
    sem_close(oven_capacity_sem);
    sem_unlink("/oven_capacity_sem");
    sem_close(delivery_sem);
    sem_unlink("/delivery_sem");
    fclose(log_file);
    free(IPbuffer); // Free the duplicated string
    return 0;
}
