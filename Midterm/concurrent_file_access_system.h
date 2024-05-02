#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#ifndef CONCURRENT_FILE_ACCESS_SYSTEM_H

#define SERVER_FIFO "/tmp/server_fifo"
#define LOG_FILE "log.txt"
#define CLIENT_FIFO_TEMPLATE "/tmp/client_fifo.%ld"
#define CLIENT_FIFO_NAME_LEN (sizeof(CLIENT_FIFO_TEMPLATE) + 20)

typedef struct request_t
{

} request_t;

typedef struct response_t
{

} response_t;


int print(const char *message) 
{
    size_t len = strlen(message);
    ssize_t bytes_written;

    do {
        bytes_written = write(STDOUT_FILENO, message, len);
    } while (bytes_written == -1 && errno == EINTR);  // Retry if interrupted by signal

    if (bytes_written == -1) 
    {
        perror("Error writing to STDOUT");
        return -1;
    }

    return 0;  // Success
}


// Function to log messages to the log file
void log_message(const char *message) 
{
    int log_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (log_fd == -1) 
    {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }

    if (write(log_fd, message, strlen(message)) == -1) 
    {
        perror("Error writing to log file");
        close(log_fd);
        exit(EXIT_FAILURE);
    }

    close(log_fd);
}

int connect_or_tryconnect(const char *str)
{
    if(str != NULL)
    {
        if(strcmp(str, "Connect") == 0)
        {
            return 0;
        }
        else if(strcmp(str, "tryConnect") == 0)
        {
            return 1;
        }
        else
        {
            return -1;
        }
    }
    else 
    {
        return -1;
    }
}

// Function to create client FIFO
int create_client_fifo(const char *client_fifo) 
{
    if (mkfifo(client_fifo, 0666) == -1) 
    {
        fprintf(stderr, "Error creating client FIFO: %s (errno=%d)\n", strerror(errno), errno);
        return -1;
    }

    return 0;
}


#endif // CONCURRENT_FILE_ACCESS_SYSTEM_H