#include <stdlib.h>
#include "concurrent_file_access_system.h"
#include "queue.h"
#include <stdio.h>
#include <unistd.h> 
#include <dirent.h>
#include <signal.h>
#include <errno.h> 
#include <string.h> 

volatile sig_atomic_t kill_signal_received = 0;

pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_var_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t waiting_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t connected_list_mutex = PTHREAD_MUTEX_INITIALIZER;

void sigint_handler(int sig) 
{
    pthread_mutex_lock(&global_var_mutex);
    kill_signal_received = 1;
    pthread_mutex_unlock(&global_var_mutex);
}

char* help_available_operations()
{
    return "Available comments are :\n"
           "help, list, readF, writeT, upload, download, archServer, quit, killServer \n";
}

char* help_for_operation(operation_type_t operation)
{
    switch(operation)
    {
        case HELP:
            return "Display the list of possible client requests.\n";

        case LIST:
            return "Sends a request to display the list of files in Server's directory. (Also displays the list received from the Server)\n";

        case READ_FILE:
            return "readF <file> <line #> \n"
                     "requests to display the # line of the <file>, if no line number is given the whole contents of the file is requested (and displayed on the client side) \n";

        case WRITE_FILE:
            return "writeT <file> <line #> <string>\n"
                    "Request to write the content of “string” to the #th line the <file>, if the line # is not given writes to the end of file. If the file does not exists in Servers directory creates and edits the file at the same time.\n";

        case UPLOAD:
            return "upload <file>\n"
                    "Uploads the file from the current working directory of client to the Server's directory. (Beware of the cases no file in clients current working directory  and  file with the same name on Server's side)\n";

        case DOWNLOAD:
            return "download <file>\n"
                    "Request to receive <file> from Server's directory to client side.\n";

        case ARCHIVE_SERVER:
            return "archServer <fileName>.tar\n"
                    "Using fork, exec and tar utilities create a child process that will collect all the files currently available on the the Server side and store them in the <filename>.tar archive.\n";

        case KILL_SERVER:
            return "killServer\n"
                    "Sends a kill request to the Server\n";

        case QUIT:
            return "quit\n"
                    "Send write request to Server side log file and quits.\n";
        default:
            return "Invalid operation\n";
    }
}

char* list()
{
    DIR *dir;
    struct dirent *entry;
    char *list = NULL;
    size_t total_length = 0;

    dir = opendir(".");
    if (dir == NULL) 
    {
        fprintf(stderr, "Error opening directory: %s (error=%d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    // Allocate initial memory for the list
    list = (char*)malloc(1);
    if (list == NULL) 
    {
        fprintf(stderr, "Memory allocation failed: %s (error=%d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }
    list[0] = '\0';

    while ((entry = readdir(dir)) != NULL) 
    {
        size_t entry_length = strlen(entry->d_name) + 2; // Add 2 for newline and null terminator
        char* temp = realloc(list, total_length + entry_length);
        if (temp == NULL) 
        {
            fprintf(stderr, "Memory reallocation failed: %s (errno=%d)\n", strerror(errno), errno);
            free(list);
            closedir(dir);
            exit(EXIT_FAILURE);
        }
        list = temp;
        snprintf(list + total_length, entry_length, "%s\n", entry->d_name);
        total_length += entry_length - 1; // Exclude null terminator
    }
    closedir(dir);
    return list;
}

char* read_file(const char* filename, int line_number) 
{
    FILE* file = fopen(filename, "r");
    if (file == NULL) 
    {
        fprintf(stderr, "Error opening file '%s': %s\n", filename, strerror(errno));
        return NULL;
    }

    char* result = NULL;
    char line[MAX_LINE_LENGTH];
    int current_line = 1;

    while (fgets(line, sizeof(line), file) != NULL) 
    {
        if (line_number != -1 && current_line == line_number) 
        {
            // Allocate memory for the result
            result = (char*)malloc(strlen(line) + 1);
            if (result == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                fclose(file);
                return NULL;
            }
            strcpy(result, line);
            break;
        } else if (line_number == -1) 
        {
            // Allocate or reallocate memory for the result
            if (result == NULL) 
            {
                result = (char*)malloc(strlen(line) + 1);
                if (result == NULL) 
                {
                    fprintf(stderr, "Memory allocation failed: %s (errno=%d)\n", strerror(errno), errno);
                    fclose(file);
                    return NULL;
                }
                strcpy(result, line);
            } 
            else 
            {
                char* temp = (char*)realloc(result, strlen(result) + strlen(line) + 1);
                if (temp == NULL) 
                {
                    fprintf(stderr, "Memory reallocation failed: %s (errno=%d)\n", strerror(errno), errno);
                    fclose(file);
                    free(result);
                    return NULL;
                }
                result = temp;
                strcat(result, line);
            }
        }
        current_line++;
    }

    fclose(file);
    return result;
}

int write_to_file(const char* filename, int line_number, const char* content) 
{
    FILE* file = fopen(filename, "a+"); // Open file in append mode or create if not exists
    if (file == NULL) 
    {
        fprintf(stderr, "Error opening file: %s (errno=%d)\n", strerror(errno), errno);
        return -1; // Return -1 to indicate failure
    }

    // If line number is specified, move to that line
    if (line_number > 0) 
    {
        // Move to the specified line
        for (int i = 1; i < line_number; i++) 
        {
            if (fgets(NULL, 0, file) == NULL) 
            {
                // Unable to reach the specified line, return -1 to indicate failure
                fclose(file);
                return -1;
            }
        }
    }

    // Write content to the file
    fprintf(file, "%s\n", content);
    fclose(file);
    return 0; 
}

int upload_file(const char* filename, const char* server_dir) 
{
    // Check if the file already exists in the server directory
    char server_filename[256];
    snprintf(server_filename, sizeof(server_filename), "%s/%s", server_dir, filename);
    if (access(server_filename, F_OK) != -1) 
    {
        return -1; // File with the same name already exists in the server directory
    }

    // Open the file in read mode
    FILE* file = fopen(filename, "r");
    if (file == NULL) 
    {
        return -1; // Error opening file
    }

    // Determine the size of the file
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate memory to store the file content
    char* file_content = (char*)malloc(file_size + 1);
    if (file_content == NULL) 
    {
        fclose(file);
        return -1; // Memory allocation failed
    }

    // Read the file content into memory
    if (fread(file_content, 1, file_size, file) != file_size) 
    {
        fclose(file);
        free(file_content);
        return -1; // Error reading file
    }
    file_content[file_size] = '\0'; // Null-terminate the string

    // Close the file
    fclose(file);

    // Open the file in the server's directory for writing
    FILE* server_file = fopen(server_filename, "w");
    if (server_file == NULL) 
    {
        free(file_content);
        return -1; // Error opening server file
    }

    // Write the file content to the server's file
    if (fwrite(file_content, 1, file_size, server_file) != file_size) 
    {
        fclose(server_file);
        free(file_content);
        return -1; // Error writing to server file
    }

    // Close the server file
    fclose(server_file);

    // Free the memory allocated for file content
    free(file_content);

    return 0; // Success
}

int download_file(const char* filename, const char* server_dir) 
{
    // Check if the file exists in the server directory
    char server_filename[256];
    snprintf(server_filename, sizeof(server_filename), "%s/%s", server_dir, filename);
    if (access(server_filename, F_OK) == -1) 
    {
        return -1; // File does not exist in the server directory
    }

    FILE* server_file = fopen(server_filename, "r");
    if (server_file == NULL) 
    {
        return -1; // Error opening server file
    }

    // Determine the size of the file
    fseek(server_file, 0, SEEK_END);
    long file_size = ftell(server_file);
    fseek(server_file, 0, SEEK_SET);

    // Allocate memory to store the file content
    char* file_content = (char*)malloc(file_size + 1);
    if (file_content == NULL) 
    {
        fclose(server_file);
        return -1; // Memory allocation failed
    }

    // Read the file content from the server file
    if (fread(file_content, 1, file_size, server_file) != file_size) 
    {
        fclose(server_file);
        free(file_content);
        return -1; // Error reading server file
    }
    file_content[file_size] = '\0'; // Null-terminate the string

    // Close the server file
    fclose(server_file);

    // Open the file in write mode
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        free(file_content);
        return -1; // Error opening local file
    }

    // Write the file content to the local file
    if (fwrite(file_content, 1, file_size, file) != file_size) {
        fclose(file);
        free(file_content);
        return -1; // Error writing to local file
    }

    // Close the local file
    fclose(file);
    free(file_content);
    return 0; // Success
}

int archive_server(const char* filename, const char* server_dir) 
{
    pid_t pid = fork(); // Fork a child process
    if (pid < 0) {
        perror("Fork failed");
        return -1;
    } else if (pid == 0) 
    {
        // Child process
        // Change working directory to the server directory
        if (chdir(server_dir) == -1) 
        {
            perror("Error changing directory");
            exit(EXIT_FAILURE);
        }
        // Execute tar command to create the archive
        execlp("tar", "tar", "-cf", filename, ".", NULL);
        // If exec returns, it failed
        perror("Exec failed");
        exit(EXIT_FAILURE);
    } else 
    {
        // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for the child to finish
        if (WIFEXITED(status)) 
        {
            int exit_status = WEXITSTATUS(status);
            if (exit_status == 0) 
            {
                return 0; // Success
            } 
            else 
            {
                return -1; // Failure
            }
        } 
        else 
        {
            //Child process terminated abnormally 
            return -2; // Failure
        }
    }
}

void handle_request(request_t request, int client_fd, queue_t *waiting_list, queue_t *connected_list, char *dirname, int max_clients)
{
    char log[100];
    char *file_list = NULL;
    char *file_content = NULL;
    int status = -2;

    if(request.connection_type == CONNECT && request.operation_type == NONE)
    {
        if(connected_list->size == max_clients)
        {
            pthread_mutex_lock(&waiting_list_mutex);
            enqueue(waiting_list, request.client_pid);
            pthread_mutex_unlock(&waiting_list_mutex);
            fprintf(stdout, "Client %d added to waiting list\n", request.client_pid);
            send_response(WAIT, "Server is full. Please wait.\n", client_fd, request.client_pid);
        }
        else
        {
            pthread_mutex_lock(&connected_list_mutex);
            enqueue(connected_list, request.client_pid);
            pthread_mutex_unlock(&connected_list_mutex);
            fprintf(stdout, "Client %d connected.\n", request.client_pid);
            send_response(SUCCESS, "Connected to server.\n", client_fd, request.client_pid);
        }
    }
    else if(request.connection_type == TRY_CONNECT && request.operation_type == NONE)
    {
        if(connected_list->size == max_clients)
        {
            send_response(FAILURE, "Server is full. Please try again later.\n", client_fd, request.client_pid);
        }
        else
        {
            pthread_mutex_lock(&connected_list_mutex);
            enqueue(connected_list, request.client_pid);
            pthread_mutex_unlock(&connected_list_mutex);
            fprintf(stdout, "Client %d connected\n", request.client_pid);
            send_response(SUCCESS, "Connected to server.\n", client_fd, request.client_pid);
        }
    }
    else if(is_client_in_queue(connected_list, request.client_pid == 1))
    {
        switch (request.operation_type)
        {
            case HELP:
                if(strcmp(request.command.filename, "") != 0)
                {
                    send_response(SUCCESS, help_for_operation(request.operation_type), client_fd, request.client_pid);
                }
                else
                {
                    send_response(SUCCESS, help_available_operations(), client_fd, request.client_pid);
                }
                break;
            case LIST:
                    file_list = list();
                    send_response(SUCCESS, file_list, client_fd, request.client_pid);
                    free(file_list);
                break;
            case READ_FILE:
                    file_content = read_file(request.command.filename, request.command.line);
                    if (file_content != NULL) 
                    {
                        send_response(SUCCESS, file_content, client_fd, request.client_pid);
                        free(file_content);
                    } 
                    else 
                    {
                        send_response(FAILURE, "Error reading file\n", client_fd, request.client_pid);
                    }
                break;
            case WRITE_FILE:
                    if (write_to_file(request.command.filename, request.command.line, request.command.data) == 0) 
                    {
                        send_response(SUCCESS, "File written successfully\n", client_fd, request.client_pid);
                    } 
                    else 
                    {
                        send_response(FAILURE, "Error writing to file\n", client_fd, request.client_pid);
                    }
                break;
            case UPLOAD:
                    if (upload_file(request.command.filename, dirname) == 0) 
                    {
                        send_response(SUCCESS, "File uploaded successfully\n", client_fd, request.client_pid);
                    } 
                    else 
                    {
                        send_response(FAILURE, "Error uploading file\n", client_fd, request.client_pid);
                    }
                break;
            case DOWNLOAD:
                    if (download_file(request.command.filename, dirname) == 0) 
                    {
                        send_response(SUCCESS, "File downloaded successfully\n", client_fd, request.client_pid);
                    } 
                    else 
                    {
                        send_response(FAILURE, "Error downloading file\n", client_fd, request.client_pid);
                    }
                break;
            case ARCHIVE_SERVER:
                    status = archive_server(request.command.filename, dirname);
                    if (status == 0) 
                    {
                        send_response(SUCCESS, "Server archived successfully.\n", client_fd, request.client_pid);
                    } 
                    else if(status == -1)
                    {
                        send_response(FAILURE, "Error archiving server.\n", client_fd, request.client_pid);
                    }
                    else if(status == -2)
                    {
                        send_response(FAILURE, "Error archiving server. Child process terminated abnormally.\n", client_fd, request.client_pid);
                    }
                break;
            case KILL_SERVER:
                while (!is_empty(connected_list)) 
                {
                    pthread_mutex_lock(&connected_list_mutex);
                    int child_pid = dequeue(connected_list);
                    pthread_mutex_unlock(&connected_list_mutex);
                    kill(child_pid, SIGKILL);
                }
                while (!is_empty(waiting_list)) 
                {
                    pthread_mutex_lock(&waiting_list_mutex);
                    int child_pid = dequeue(waiting_list);
                    pthread_mutex_unlock(&waiting_list_mutex);
                    kill(child_pid, SIGKILL);
                }
                kill(getpid(), SIGKILL);
                // Write to log file
                snprintf(log, sizeof(log), "Server killed. PID: %d\n", getpid());
                log_message(log);
                break;
            case QUIT:
                // Kill the client process
                kill(request.client_pid, SIGKILL);
                // Write to log file
                snprintf(log, sizeof(log), "Client %d killed.\n", request.client_pid);
                log_message(log);
                break;
            default:
                break;
        }
    }
}

int main(int argc, char *argv[])
{
    int server_fd, dummy_fd, max_clients;
    int client_fd = 0;
    char server_fifo[100];
    request_t request; 
    queue_t *waiting_list = create_queue();
    queue_t *connected_list = create_queue();
    char *dirname = NULL;
    char message[100];
    char client_fifo[CLIENT_FIFO_NAME_LEN];

    if(argc != 3)
    {
        fprintf(stderr, "Usage: %s <dirname> <max. #ofClients>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    else 
    {
        dirname = argv[1]; 
        max_clients = atoi(argv[2]);
    }

    // Register signal handler for SIGINT
    if (signal(SIGINT, sigint_handler) == SIG_ERR) 
    {
        perror("Error registering signal handler");
        exit(EXIT_FAILURE);
    }

    // Create directory if it doesn't exist
    if (mkdir(dirname, 0777) == -1 && errno != EEXIST) 
    {
        perror("Error creating directory");
        exit(EXIT_FAILURE);
    }

    // Change current working directory to the specified directory
    if (chdir(dirname) == -1) 
    {
        perror("Error changing directory");
        exit(EXIT_FAILURE);
    }

    // Create log file
    int log_file = open(LOG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (log_file == -1) 
    {
        fprintf(stderr, "Error creating log file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
   
    umask(0);
    snprintf(server_fifo, sizeof(server_fifo), SERVER_FIFO_TEMPLATE, getpid());
    if(mkfifo(SERVER_FIFO_TEMPLATE, 0666) == -1)
    {
        fprintf(stderr, "Error creating server FIFO: %s (errno=%d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    server_fd = open(SERVER_FIFO_TEMPLATE, O_RDONLY|O_NONBLOCK);
    if(server_fd == -1)
    {
        fprintf(stderr, "Error opening server FIFO: %s (errno=%d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    /* Open the server FIFO for writing so that it doesn't return EOF */
    dummy_fd = open(SERVER_FIFO_TEMPLATE, O_WRONLY );
    if(dummy_fd == -1)
    {
        fprintf(stderr, "Error opening server FIFO for writing: %s (errno=%d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    // Server Start Up Message
    snprintf(message, sizeof(message), "Server started. PID: %d\n", getpid());
    log_message(message);
    fprintf(stdout, "Server started with PID=%d\n", getpid());

    pthread_mutex_lock(&global_var_mutex);
    while(!kill_signal_received)
    {

        // Check if connected clients have plots available
        if (!is_empty(connected_list) && !is_empty(waiting_list))
        {
            // Dequeue a client from the waiting queue
            pthread_mutex_lock(&waiting_list_mutex);
            int waiting_client_pid = dequeue(waiting_list);
            pthread_mutex_unlock(&waiting_list_mutex);
            if(waiting_client_pid != -1)
            {
                // Dequeue a client from the connected queue
                pthread_mutex_lock(&connected_list_mutex);
                int connected_client_pid = dequeue(connected_list);
                pthread_mutex_unlock(&connected_list_mutex);
                if(connected_client_pid != -1)
                {
                    // Assign the plot to the waiting client
                    pthread_mutex_lock(&connected_list_mutex);
                    enqueue(connected_list, waiting_client_pid);
                    pthread_mutex_unlock(&connected_list_mutex);
                    // Notify the waiting client about the assignment
                    char waiting_client_fifo[CLIENT_FIFO_NAME_LEN];
                    snprintf(waiting_client_fifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, waiting_client_pid);
                    send_response(SUCCESS, "Connected to server.\n", client_fd, waiting_client_pid);
                }
            }
        }

        ssize_t bytes_read;
        while ((bytes_read = read(server_fd, &request, sizeof(request_t))) == -1) 
        {
            if (errno == EINTR) {
                // Retry the read operation if interrupted by a signal
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available yet, sleep for a short interval before retrying
                usleep(100000); // Sleep for 100 milliseconds (adjust as needed)
                continue;
            } else {
                // Other error occurred, handle it
                fprintf(stderr, "Error reading from server FIFO: %s (errno=%d)\n", strerror(errno), errno);
                exit(EXIT_FAILURE);
            }
        }

        pid_t pid = fork();
        if(pid < 0)
        {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }
        else if(pid == 0)
        {
            // Child process
            client_fd = open(client_fifo, O_WRONLY);
            if(client_fd == -1)
            {
                fprintf(stderr, "Error opening client FIFO: %s (errno=%d)\n", strerror(errno), errno);
                exit(EXIT_FAILURE);
            }

            handle_request(request, client_fd, waiting_list, connected_list, dirname, max_clients);

            exit(EXIT_SUCCESS);
        }
        else 
        {
            //Parent process
            int status;
            waitpid(pid, &status, 0);
            if(WIFEXITED(status))
            {
                int exit_status = WEXITSTATUS(status);
                if(exit_status == 0)
                {
                    fprintf(stdout, "Child process %d exited successfully\n", pid);
                }
                else
                {
                    fprintf(stderr, "Child process %d exited with status %d\n",pid, exit_status);
                }
            }
            else
            {
                fprintf(stderr, "Child process %d terminated abnormally\n", pid);
            }
        }
    }
    pthread_mutex_unlock(&global_var_mutex);
    exit(EXIT_SUCCESS);
}
