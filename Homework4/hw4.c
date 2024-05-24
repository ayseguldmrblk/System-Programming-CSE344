#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>

typedef struct {
    int *buffer;
    int head;
    int tail;
    int max_size;
    int count;
    int num_regular_files;
    int num_fifo_files;
    int num_directories;
    size_t total_bytes_copied;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
    int done;
} buffer_t;

typedef struct {
    buffer_t *buffer;
    char *src_dir;
    char *dest_dir;
} manager_args_t;

buffer_t *create_buffer(int size) {
    buffer_t *buf = malloc(sizeof(buffer_t));
    buf->buffer = malloc(size * 2 * sizeof(int));  // Buffer holds pairs of file descriptors
    buf->head = 0;
    buf->tail = 0;
    buf->max_size = size;
    buf->count = 0;
    buf->num_regular_files = 0;
    buf->num_fifo_files = 0;
    buf->num_directories = 0;
    buf->total_bytes_copied = 0;
    buf->done = 0;
    pthread_mutex_init(&buf->mutex, NULL);
    pthread_cond_init(&buf->not_full, NULL);
    pthread_cond_init(&buf->not_empty, NULL);
    return buf;
}

void cleanup(buffer_t *buffer) {
    free(buffer->buffer);
    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->not_full);
    pthread_cond_destroy(&buffer->not_empty);
    free(buffer);
}

void handle_signal(int sig) {
    // Handle signals like SIGINT to terminate gracefully
    fprintf(stderr, "Received signal %d. Terminating gracefully.\n", sig);
    exit(1);
}

// Helper function to process directories
void process_directory(buffer_t *buffer, const char *src_dir, const char *dest_dir) {
    DIR *src = opendir(src_dir);
    if (!src) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(src)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char src_path[PATH_MAX];
        char dest_path[PATH_MAX];
        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, entry->d_name);

        if (entry->d_type == DT_DIR) {
            // Create directory in destination
            if (mkdir(dest_path, 0755) == -1 && errno != EEXIST) {
                perror("mkdir");
                continue;
            }

            pthread_mutex_lock(&buffer->mutex);
            buffer->num_directories++;
            pthread_mutex_unlock(&buffer->mutex);

            // Recursively process the directory
            process_directory(buffer, src_path, dest_path);
        } else if (entry->d_type == DT_REG || entry->d_type == DT_FIFO) {
            int src_fd = open(src_path, O_RDONLY);
            if (src_fd == -1) {
                perror("open src");
                continue;
            }

            int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (dest_fd == -1) {
                perror("open dest");
                close(src_fd);
                continue;
            }

            pthread_mutex_lock(&buffer->mutex);
            while (buffer->count == buffer->max_size) {
                pthread_cond_wait(&buffer->not_full, &buffer->mutex);
            }

            // Add file descriptors to buffer
            buffer->buffer[buffer->tail] = src_fd;
            buffer->buffer[buffer->tail + 1] = dest_fd;
            buffer->tail = (buffer->tail + 2) % (buffer->max_size * 2);
            buffer->count++;

            pthread_cond_signal(&buffer->not_empty);
            pthread_mutex_unlock(&buffer->mutex);

            // Update statistics
            pthread_mutex_lock(&buffer->mutex);
            if (entry->d_type == DT_REG) {
                buffer->num_regular_files++;
            } else if (entry->d_type == DT_FIFO) {
                buffer->num_fifo_files++;
            }
            pthread_mutex_unlock(&buffer->mutex);
        }
    }
    closedir(src);
}

void *manager_thread(void *arg) {
    manager_args_t *args = (manager_args_t *)arg;
    buffer_t *buffer = args->buffer;
    char *src_dir = args->src_dir;
    char *dest_dir = args->dest_dir;

    // Start processing the source directory
    process_directory(buffer, src_dir, dest_dir);

    pthread_mutex_lock(&buffer->mutex);
    buffer->done = 1;
    pthread_cond_broadcast(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);

    pthread_exit(NULL);
}

void *worker_thread(void *arg) {
    buffer_t *buffer = (buffer_t *)arg;

    while (1) {
        pthread_mutex_lock(&buffer->mutex);
        while (buffer->count == 0 && !buffer->done) {
            pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
        }

        if (buffer->count == 0 && buffer->done) {
            pthread_mutex_unlock(&buffer->mutex);
            break;
        }

        int src_fd = buffer->buffer[buffer->head];
        int dest_fd = buffer->buffer[buffer->head + 1];
        buffer->head = (buffer->head + 2) % (buffer->max_size * 2);
        buffer->count--;

        pthread_cond_signal(&buffer->not_full);
        pthread_mutex_unlock(&buffer->mutex);

        // Copy file logic using file descriptors
        char buf[BUFSIZ];
        ssize_t n;
        while ((n = read(src_fd, buf, sizeof(buf))) > 0) {
            if (write(dest_fd, buf, n) != n) {
                perror("write");
                break;
            }
            pthread_mutex_lock(&buffer->mutex);
            buffer->total_bytes_copied += n;
            pthread_mutex_unlock(&buffer->mutex);
        }
        close(src_fd);
        close(dest_fd);

        // Critical section: write completion status to standard output
        pthread_mutex_lock(&buffer->mutex);
        printf("Copied file descriptor %d to %d\n", src_fd, dest_fd);
        pthread_mutex_unlock(&buffer->mutex);
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <buffer_size> <num_workers> <src_dir> <dest_dir>\n", argv[0]);
        return 1;
    }

    int buffer_size = atoi(argv[1]);
    int num_workers = atoi(argv[2]);
    char *src_dir = argv[3];
    char *dest_dir = argv[4];

    if (buffer_size <= 0 || num_workers <= 0) {
        fprintf(stderr, "Buffer size and number of workers must be positive integers.\n");
        return 1;
    }

    buffer_t *buffer = create_buffer(buffer_size);

    // Install signal handler
    signal(SIGINT, handle_signal);

    pthread_t manager;
    pthread_t workers[num_workers];

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Prepare manager arguments
    manager_args_t manager_args = {buffer, src_dir, dest_dir};

    // Create manager thread
    pthread_create(&manager, NULL, manager_thread, (void *)&manager_args);

    // Create worker threads
    for (int i = 0; i < num_workers; i++) {
        pthread_create(&workers[i], NULL, worker_thread, (void *)buffer);
    }

    // Wait for manager and workers to complete
    pthread_join(manager, NULL);
    for (int i = 0; i < num_workers; i++) {
        pthread_join(workers[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    // Calculate elapsed time
    long seconds = end.tv_sec - start.tv_sec;
    long nanoseconds = end.tv_nsec - start.tv_nsec;
    double elapsed = seconds + nanoseconds * 1e-9;
    long milliseconds = (long)(elapsed * 1000) % 1000;
    long minutes = seconds / 60;
    seconds = seconds % 60;

    // Print statistics
    printf("\n---------------STATISTICS--------------------\n");
    printf("Consumers: %d - Buffer Size: %d\n", num_workers, buffer_size);
    printf("Number of Regular File: %d\n", buffer->num_regular_files);
    printf("Number of FIFO File: %d\n", buffer->num_fifo_files);
    printf("Number of Directory: %d\n", buffer->num_directories);
    printf("TOTAL BYTES COPIED: %zu\n", buffer->total_bytes_copied);
    printf("TOTAL TIME: %02ld:%02ld.%03ld (min:sec.mili)\n", minutes, seconds, milliseconds);

    // Clean up
    cleanup(buffer);

    return 0;
}