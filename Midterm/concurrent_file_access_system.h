#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#ifndef CONCURRENT_FILE_ACCESS_SYSTEM_H

#define SERVER_FIFO_TEMPLATE "/tmp/server_fifo.%d"
#define SERVER_FIFO_NAME_LEN (sizeof(SERVER_FIFO_TEMPLATE) + 20)
#define LOG_FILE "log.txt"
#define CLIENT_FIFO_TEMPLATE "/tmp/client_fifo.%d"
#define CLIENT_FIFO_NAME_LEN (sizeof(CLIENT_FIFO_TEMPLATE) + 20)
#define MAX_LINE_LENGTH 1000

typedef enum 
{
    CONNECT = 0,
    TRY_CONNECT
} connection_type_t;

typedef enum 
{
    NONE = 0, //For connection requests
    HELP,
    LIST,
    READ_FILE,
    WRITE_FILE,
    UPLOAD,
    DOWNLOAD,
    ARCHIVE_SERVER,
    KILL_SERVER,
    QUIT,
    INVALID_OPERATION
} operation_type_t;

typedef struct command_t
{
    operation_type_t operation_type;
    char filename[100];
    int line;
    char data[100];
} command_t;

typedef struct request_t
{
    pid_t client_pid;
    connection_type_t connection_type;
    operation_type_t operation_type;
    command_t command;
} request_t;

typedef enum 
{
    SUCCESS = 0,
    FAILURE,
    WAIT,
} response_status_t;

typedef struct response_t
{
    response_status_t status;
    char body[400];
} response_t;

char* help_available_operations();
char* help_for_operation(char* commandAsked);

int send_response(response_status_t status, const char *body, int client_fd, pid_t client_pid)
{
    response_t response;
    response.status = status;
    snprintf(response.body, sizeof(response.body), "%s", body);

    if(write(client_fd, &response, sizeof(response_t)) == -1)
    {
        fprintf(stderr, "Error writing to client FIFO: %s (errno=%d)\n", strerror(errno), errno);
        return -1;
    }
    else
    {
        fprintf(stdout, "Response sent to client %d\n", client_pid);
    }

    return 0;
}

int send_request(pid_t client_pid, connection_type_t connection_type, operation_type_t operation_type, command_t command, int server_fifo_fd)
{
    request_t request;
    request.client_pid = client_pid;
    request.connection_type = connection_type;
    request.operation_type = operation_type;
    request.command = command;

    if(write(server_fifo_fd, &request, sizeof(request_t)) == -1)
    {
        fprintf(stderr, "Error writing to server FIFO: %s (errno=%d)\n", strerror(errno), errno);
        return -1;
    }
    else
    {
        fprintf(stdout, "Request sent to server\n");
    }

    return 0;
}

// Function to convert integer to string
char* int_to_str(int num) 
{
    // Maximum length of an int is 10 digits, plus 1 for the null terminator
    char *str = (char *)malloc(11 * sizeof(char));
    if (str == NULL) 
    {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    sprintf(str, "%d", num); // Convert integer to string
    return str;
}

command_t parse_command(char *input) 
{
    command_t command;
    memset(&command, 0, sizeof(command)); // Initialize command struct to zero
    snprintf(command.filename, sizeof(command.filename), "%s", ""); // Initialize filename to empty string
    command.line = -1;
    snprintf(command.data, sizeof(command.data), "%s", ""); // Initialize data to empty string

    // Extract the operation type
    char *token = strtok(input, " ");

    // help 
    if (strcmp(token, "help") == 0) 
    {
        command.operation_type = HELP;
        token = strtok(NULL, " ");
        // help <operation>
        if (token != NULL) 
        {
            strcpy(command.filename, token);
        }
    // list
    } else if (strcmp(token, "list") == 0) 
    {
        command.operation_type = LIST;
    // readF
    } else if (strcmp(token, "readF") == 0) 
    {
        command.operation_type = READ_FILE;
        // <file>
        token = strtok(NULL, " ");
        if (token != NULL) 
        {
            strcpy(command.filename, token);
        }
        // <line #>
        token = strtok(NULL, "");
        if (token != NULL) 
        {
            command.line = atoi(token);
        }
    //write
    } else if (strcmp(token, "writeT") == 0) 
    {
        command.operation_type = WRITE_FILE;
        // <file>
        token = strtok(NULL, " ");
        if (token != NULL) 
        {
            strcpy(command.filename, token);
        }
        // <line #>
        token = strtok(NULL, "");
        if (token != NULL) 
        {
            command.line = atoi(token);
        }
        // <string>
        token = strtok(NULL, "");
        if (token != NULL) 
        {
            strcpy(command.data, token);
        }
    // upload 
    } else if (strcmp(token, "upload") == 0) 
    {
        command.operation_type = UPLOAD;
        // <file>
        token = strtok(NULL, " ");
        if (token != NULL) 
        {
            strcpy(command.filename, token);
        }
    // download
    } else if (strcmp(token, "download") == 0) 
    {
        command.operation_type = DOWNLOAD;
        // <file>
        token = strtok(NULL, " ");
        if (token != NULL) 
        {
            strcpy(command.filename, token);
        }
    // archServer
    } else if (strcmp(token, "archServer") == 0) 
    {
        command.operation_type = ARCHIVE_SERVER;
        // <file>.tar
        token = strtok(NULL, " ");
        if (token != NULL) 
        {
            strcpy(command.filename, token);
        }
    // killServer
    } else if (strcmp(token, "killServer") == 0) 
    {
        command.operation_type = KILL_SERVER;
    // quit
    } else if (strcmp(token, "quit") == 0) 
    {
        command.operation_type = QUIT;
    } else 
    {
        command.operation_type = INVALID_OPERATION;
        return command; // Invalid operation, return immediately
    }
    return command;
}

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

// Function to log a message to the log file
void log_message(const char* message) {
    // Open the log file
    int log_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (log_fd == -1) {
        perror("Error opening log file");
        return;
    }

    // Acquire an exclusive lock on the log file
    struct flock fl;
    fl.l_type = F_WRLCK;  // Exclusive write lock
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    if (fcntl(log_fd, F_SETLKW, &fl) == -1) {
        perror("Error acquiring lock on log file");
        close(log_fd);
        return;
    }

    // Write the message to the log file
    if (write(log_fd, message, strlen(message)) == -1) {
        perror("Error writing to log file");
    }

    // Release the lock on the log file
    fl.l_type = F_UNLCK;
    if (fcntl(log_fd, F_SETLK, &fl) == -1) {
        perror("Error releasing lock on log file");
    }

    // Close the log file
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
    if (mkfifo(client_fifo, 0644) == -1 && errno != EEXIST) 
    {
        fprintf(stderr, "Error creating client FIFO: %s (errno=%d)\n", strerror(errno), errno);
        return -1;
    }
    return 0;
}

#endif // CONCURRENT_FILE_ACCESS_SYSTEM_H