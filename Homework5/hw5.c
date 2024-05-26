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
#include <sys/resource.h>

typedef struct {
    int *buffer;                   // Circular buffer to hold file descriptor pairs (source and destination)
    int head;                      // Head index for the circular buffer
    int tail;                      // Tail index for the circular buffer
    int max_size;                  // Maximum number of file descriptor pairs the buffer can hold
    int count;                     // Current number of file descriptor pairs in the buffer
    int num_regular_files;         // Number of regular files processed
    int num_directories;           // Number of directories processed
    int num_fifo;                  // Number of FIFOs processed
    size_t total_bytes_copied;     // Total number of bytes copied
    pthread_mutex_t mutex;         // Mutex for synchronizing access to the buffer
    pthread_cond_t not_full;       // Condition variable to signal when the buffer is not full
    pthread_cond_t not_empty;      // Condition variable to signal when the buffer is not empty
    int done;                      // Flag to indicate when the manager is done processing directories
} buffer_t;

typedef struct {
    buffer_t *buffer;
    char *src_dir;
    char *dest_dir;
} manager_args_t;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int num_threads;
} barrier_t;

void barrier_init(barrier_t *barrier, int num_threads) {
    pthread_mutex_init(&barrier->mutex, NULL);
    pthread_cond_init(&barrier->cond, NULL);
    barrier->count = 0;
    barrier->num_threads = num_threads;
}

void barrier_wait(barrier_t *barrier) {
    pthread_mutex_lock(&barrier->mutex);
    barrier->count++;
    if (barrier->count == barrier->num_threads) {
        barrier->count = 0;  // Reset the barrier for next use
        pthread_cond_broadcast(&barrier->cond);
    } else {
        pthread_cond_wait(&barrier->cond, &barrier->mutex);
    }
    pthread_mutex_unlock(&barrier->mutex);
}

void safe_print(const char *msg) {
    write(STDOUT_FILENO, msg, strlen(msg));
}

buffer_t *create_buffer(int size) {
    buffer_t *buf = malloc(sizeof(buffer_t));
    if (!buf) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    buf->buffer = malloc(size * 2 * sizeof(int));  // Buffer holds pairs of file descriptors
    if (!buf->buffer) {
        perror("malloc");
        free(buf);
        exit(EXIT_FAILURE);
    }
    buf->head = 0;
    buf->tail = 0;
    buf->max_size = size;
    buf->count = 0;
    buf->num_regular_files = 0;
    buf->num_directories = 0;
    buf->num_fifo = 0;  // Initialize FIFO counter
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
    const char *msg = "Received signal. Terminating gracefully.\n";
    write(STDERR_FILENO, msg, strlen(msg));
    exit(1);
}

void check_fd_limit() {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("getrlimit");
        exit(EXIT_FAILURE);
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "Current file descriptor limit: %llu. If it exceeds, program will exit.\n", (unsigned long long)rl.rlim_cur);
    safe_print(msg);
}

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
            if (mkdir(dest_path, 0755) == -1 && errno != EEXIST) {
                perror("mkdir");
                continue;
            }

            pthread_mutex_lock(&buffer->mutex);
            buffer->num_directories++;
            pthread_mutex_unlock(&buffer->mutex);

            process_directory(buffer, src_path, dest_path);
        } else if (entry->d_type == DT_REG) {
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

            buffer->buffer[buffer->tail] = src_fd;
            buffer->buffer[buffer->tail + 1] = dest_fd;
            buffer->tail = (buffer->tail + 2) % (buffer->max_size * 2);
            buffer->count++;

            pthread_cond_signal(&buffer->not_empty);
            pthread_mutex_unlock(&buffer->mutex);

            pthread_mutex_lock(&buffer->mutex);
            buffer->num_regular_files++;
            pthread_mutex_unlock(&buffer->mutex);
        } else if (entry->d_type == DT_FIFO) {
            if (mkfifo(dest_path, 0644) == -1 && errno != EEXIST) {
                perror("mkfifo");
                continue;
            }
            pthread_mutex_lock(&buffer->mutex);
            buffer->num_fifo++;
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

    process_directory(buffer, src_dir, dest_dir);

    pthread_mutex_lock(&buffer->mutex);
    buffer->done = 1;
    pthread_cond_broadcast(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);

    pthread_exit(NULL);
}

void *worker_thread(void *arg) {
    buffer_t *buffer = (buffer_t *)arg;
    barrier_t *barrier = (barrier_t *)arg;

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
    }

    barrier_wait(barrier);

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    struct timespec start, end;

    if (argc != 5) {
        const char *msg = "Usage: %s <buffer_size> <num_workers> <src_dir> <dest_dir>\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        return 1;
    }

    int buffer_size = atoi(argv[1]);
    int num_workers = atoi(argv[2]);
    char *src_dir = argv[3];
    char *dest_dir = argv[4];

    if (buffer_size <= 0 || num_workers <= 0) {
        const char *msg = "Buffer size and number of workers must be positive integers.\n";
        write(STDERR_FILENO, msg, strlen(msg));
        return 1;
    }

    check_fd_limit();

    buffer_t *buffer = create_buffer(buffer_size);
    barrier_t barrier;
    barrier_init(&barrier, num_workers + 1);  // +1 for the manager thread

    pthread_t manager;
    pthread_t workers[num_workers];

    signal(SIGINT, handle_signal);

    clock_gettime(CLOCK_MONOTONIC, &start);

    manager_args_t manager_args = {buffer, src_dir, dest_dir};

    pthread_create(&manager, NULL, manager_thread, (void *)&manager_args);

    for (int i = 0; i < num_workers; i++) {
        pthread_create(&workers[i], NULL, worker_thread, (void *)buffer);
    }

    pthread_join(manager, NULL);
    for (int i = 0; i < num_workers; i++) {
        pthread_join(workers[i], NULL);
    }

    barrier_wait(&barrier);

    clock_gettime(CLOCK_MONOTONIC, &end);

    long seconds = end.tv_sec - start.tv_sec;
    long nanoseconds = end.tv_nsec - start.tv_nsec;
    double elapsed = seconds + nanoseconds * 1e-9;
    long milliseconds = (long)(elapsed * 1000) % 1000;
    long minutes = seconds / 60;
    seconds = seconds % 60;

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

    cleanup(buffer);

    return 0;
}
