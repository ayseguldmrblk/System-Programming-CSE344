#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>

// Function declarations
void* carOwner(void* arg);
void* carAttendant(void* arg);
void simulateParkingLot();
void handle_sigint(int sig);

// Semaphores
sem_t newPickup;
sem_t inChargeforPickup;
sem_t newAutomobile;
sem_t inChargeforAutomobile;

// Shared counters for free spots
int mFree_automobile = 8;
int mFree_pickup = 4;

// Mutexes for accessing shared variables
pthread_mutex_t automobile_lock;
pthread_mutex_t pickup_lock;

// Flag to stop the parking lot simulation loop
volatile sig_atomic_t stop = 0;

// Signal handler for SIGINT
void handle_sigint(int sig) {
    stop = 1;
    printf("\nGoodbye\n");
}

void* carOwner(void* arg) {
    char vehicle_type = *(char*)arg;
    free(arg);  // Free dynamically allocated memory

    if (vehicle_type == 'a') {  // 'a' for automobile
        pthread_mutex_lock(&automobile_lock);
        printf("Car owner arrives. Available car spots before: %d\n", mFree_automobile);
        if (mFree_automobile > 0) {
            mFree_automobile--;
            printf("Car owner parks. Available car spots after: %d\n", mFree_automobile);
            sem_post(&newAutomobile);
            pthread_mutex_unlock(&automobile_lock);
            sem_wait(&inChargeforAutomobile);
            printf("Car parked by attendant.\n");
        } else {
            pthread_mutex_unlock(&automobile_lock);
            printf("No available car spots. Car owner leaves.\n");
        }
    } else if (vehicle_type == 'p') {  // 'p' for pickup
        pthread_mutex_lock(&pickup_lock);
        printf("Pickup owner arrives. Available pickup spots before: %d\n", mFree_pickup);
        if (mFree_pickup > 0) {
            mFree_pickup--;
            printf("Pickup owner parks. Available pickup spots after: %d\n", mFree_pickup);
            sem_post(&newPickup);
            pthread_mutex_unlock(&pickup_lock);
            sem_wait(&inChargeforPickup);
            printf("Pickup parked by attendant.\n");
        } else {
            pthread_mutex_unlock(&pickup_lock);
            printf("No available pickup spots. Pickup owner leaves.\n");
        }
    }
    return NULL;
}

void* carAttendant(void* arg) {
    char vehicle_type = *(char*)arg;
    free(arg);  // Free dynamically allocated memory

    if (vehicle_type == 'a') {  // 'a' for automobile
        sem_wait(&newAutomobile);
        printf("Attendant parks the car. Available car spots before: %d\n", mFree_automobile);
        pthread_mutex_lock(&automobile_lock);
        mFree_automobile++;
        printf("Attendant finishes parking the car. Available car spots after: %d\n", mFree_automobile);
        pthread_mutex_unlock(&automobile_lock);
        sem_post(&inChargeforAutomobile);
    } else if (vehicle_type == 'p') {  // 'p' for pickup
        sem_wait(&newPickup);
        printf("Attendant parks the pickup. Available pickup spots before: %d\n", mFree_pickup);
        pthread_mutex_lock(&pickup_lock);
        mFree_pickup++;
        printf("Attendant finishes parking the pickup. Available pickup spots after: %d\n", mFree_pickup);
        pthread_mutex_unlock(&pickup_lock);
        sem_post(&inChargeforPickup);
    }
    return NULL;
}

void simulateParkingLot() {
    while (!stop) {
        char* vehicle_type_owner = malloc(sizeof(char));
        char* vehicle_type_attendant = malloc(sizeof(char));
        *vehicle_type_owner = (rand() % 2 == 0) ? 'a' : 'p';
        *vehicle_type_attendant = *vehicle_type_owner;
        
        pthread_t ownerThread, attendantThread;
        
        printf("Creating threads for a %s\n", (*vehicle_type_owner == 'a') ? "car" : "pickup");
        pthread_create(&ownerThread, NULL, carOwner, vehicle_type_owner);
        pthread_create(&attendantThread, NULL, carAttendant, vehicle_type_attendant);
        
        pthread_join(ownerThread, NULL);
        pthread_join(attendantThread, NULL);
        
        // Simulate time delay between vehicle arrivals
        usleep((rand() % 1500 + 500) * 1000);
    }
}

int main() {
    srand(time(NULL));

    // Set up the SIGINT signal handler
    signal(SIGINT, handle_sigint);

    // Initialize semaphores
    sem_init(&newPickup, 0, 0);
    sem_init(&inChargeforPickup, 0, 0);
    sem_init(&newAutomobile, 0, 0);
    sem_init(&inChargeforAutomobile, 0, 0);

    // Initialize mutexes
    pthread_mutex_init(&automobile_lock, NULL);
    pthread_mutex_init(&pickup_lock, NULL);

    simulateParkingLot();

    // Destroy semaphores
    sem_destroy(&newPickup);
    sem_destroy(&inChargeforPickup);
    sem_destroy(&newAutomobile);
    sem_destroy(&inChargeforAutomobile);

    // Destroy mutexes
    pthread_mutex_destroy(&automobile_lock);
    pthread_mutex_destroy(&pickup_lock);

    return 0;
}
