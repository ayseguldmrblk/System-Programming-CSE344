#include "hw4.h"

int main(int argc, char *argv[]) 
{
    struct timespec start, end;

    if (argc != 5) 
    {
        const char *msg = "Usage: <buffer_size> <num_workers> <src_dir> <dest_dir>\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        return 1;
    }

    int buffer_size = atoi(argv[1]);
    int num_workers = atoi(argv[2]);
    char *src_dir = argv[3];
    char *dest_dir = argv[4];

    if (buffer_size <= 0 || num_workers <= 0) 
    {
        const char *msg = "Buffer size and number of workers must be positive integers.\n";
        write(STDERR_FILENO, msg, strlen(msg));
        return 1;
    }

    check_fd_limit();

    buffer_t *buffer = create_buffer(buffer_size);
    pthread_t manager;
    pthread_t workers[num_workers];

    // Install signal handler
    signal(SIGINT, handle_signal);

    // Start timer
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Prepare manager arguments
    manager_args_t manager_args = {buffer, src_dir, dest_dir};

    // Create manager thread
    pthread_create(&manager, NULL, manager_thread, (void *)&manager_args);

    // Create worker threads
    for (int i = 0; i < num_workers; i++) 
    {
        pthread_create(&workers[i], NULL, worker_thread, (void *)buffer);
    }

    // Wait for manager and workers to complete
    pthread_join(manager, NULL);
    for (int i = 0; i < num_workers; i++) 
    {
        pthread_join(workers[i], NULL);
    }

    // Stop timer
    clock_gettime(CLOCK_MONOTONIC, &end);

    // Calculate elapsed time
    long seconds = end.tv_sec - start.tv_sec;
    long nanoseconds = end.tv_nsec - start.tv_nsec;
    double elapsed = seconds + nanoseconds * 1e-9;
    long milliseconds = (long)(elapsed * 1000) % 1000;
    long minutes = seconds / 60;
    seconds = seconds % 60;

    // Print statistics
    char stats[500];
    snprintf(stats, sizeof(stats),
             "\n---------------STATISTICS--------------------\n"
             "Consumers: %d - Buffer Size: %d\n"
             "Number of Regular Files: %d\n"
             "Number of FIFOs: %d\n"
             "Number of Directories: %d\n"
             "TOTAL BYTES COPIED: %zu\n"
             "TOTAL TIME: %02ld:%02ld.%03ld (min:sec.millisec)\n",
             num_workers, buffer_size, buffer->num_regular_files, buffer->num_fifo,
             buffer->num_directories, buffer->total_bytes_copied, minutes, seconds, milliseconds);
    safe_print(stats);

    // Clean up
    cleanup(buffer);

    return 0;
}
