#include <stdlib.h>
#include "concurrent_file_access_system.h"
#include "queue.h"
#include <stdio.h>
#include <unistd.h> 
#include <dirent.h>
#include <signal.h>
#include <errno.h> 
#include <string.h> 

int kill_signal_received = 0;

pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t waiting_list_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t connected_list_mutex = PTHREAD_MUTEX_INITIALIZER;

void sigint_handler(int sig) 
{
    kill_signal_received = 1;
    exit(EXIT_SUCCESS);
}

void kill_parent_and_children(pid_t parent_pid) 
{
    // Kill the parent process
    kill(parent_pid, SIGKILL);

    // Wait for the parent process to terminate
    waitpid(parent_pid, NULL, 0);

    // Kill all child processes recursively
    pid_t child_pid;
    while ((child_pid = fork()) != 0) {
        if (child_pid == -1) {
            perror("Error forking child process");
            exit(EXIT_FAILURE);
        } else if (child_pid == 0) {
            // Child process
            kill(getpid(), SIGKILL); // Kill itself
            exit(EXIT_SUCCESS);
        } else {
            // Parent process
            waitpid(child_pid, NULL, 0); // Wait for child to terminate
        }
    }
}

char* help_available_operations()
{
    return "Available comments are :\n"
           "help, list, readF, writeT, upload, download, archServer, quit, killServer \n";
}

char* help_for_operation(char* commandAsked)
{
    if(strcmp(commandAsked, "help") == 0)
    {
        return "Display the list of possible client requests.\n";
    }
    else if(strcmp(commandAsked, "list") == 0)
    {
        return "Sends a request to display the list of files in Server's directory. (Also displays the list received from the Server)\n";
    }
    else if(strcmp(commandAsked, "readF") == 0)
    {
        return "readF <file> <line #> \n"
               "requests to display the # line of the <file>, if no line number is given the whole contents of the file is requested (and displayed on the client side) \n";
    }
    else if(strcmp(commandAsked, "writeT") == 0)
    {
        return "writeT <file> <line #> <string>\n"
               "Request to write the content of “string” to the #th line the <file>, if the line # is not given writes to the end of file. If the file does not exists in Servers directory creates and edits the file at the same time.\n";
    }
    else if(strcmp(commandAsked, "upload") == 0)
    {
        return "upload <file>\n"
               "Uploads the file from the current working directory of client to the Server's directory. (Beware of the cases no file in clients current working directory  and  file with the same name on Server's side)\n";
    }
    else if(strcmp(commandAsked, "download") == 0)
    {
        return "download <file>\n"
               "Request to receive <file> from Server's directory to client side.\n";
    }
    else if(strcmp(commandAsked, "archServer") == 0)
    {
        return "archServer <fileName>.tar\n"
               "Using fork, exec and tar utilities create a child process that will collect all the files currently available on the the Server side and store them in the <filename>.tar archive.\n";
    }
    else if(strcmp(commandAsked, "killServer") == 0)
    {
        return "killServer\n"
               "Sends a kill request to the Server\n";
    }
    else if(strcmp(commandAsked, "quit") == 0)
    {
        return "quit\n"
               "Send write request to Server side log file";
    }
    else
    {
        return "Invalid command\n";
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
    printf("In write_to_file\n");
    printf("Filename: %s\n", filename);
    printf("Line number: %d\n", line_number);
    printf("Content: %s\n", content);
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

int upload_file(const char* filename, const char* client_dir, const char* server_dir) 
{
    // Construct the full path of the file in the client directory
    char client_filename[MAX_FILENAME_LEN];
    snprintf(client_filename, sizeof(client_filename), "%s/%s", client_dir, filename);

    // Construct the full path of the file in the server directory
    char server_filename[MAX_FILENAME_LEN];
    snprintf(server_filename, sizeof(server_filename), "%s", filename);

    // Check if the file already exists in the server directory
    if (access(server_filename, F_OK) != -1) 
    {
        printf("File with the same name already exists in the server directory.\n");
        return -1; // File with the same name already exists in the server directory
    }

    // Open the file in read mode from the client directory
    int file = open(client_filename, O_RDONLY);
    if (file == -1) 
    {
        fprintf(stderr, "Error opening file: %s (errno=%d)\n", strerror(errno), errno);
        return -1; // Error opening file
    }

    // Determine the size of the file
    off_t file_size = lseek(file, 0, SEEK_END);
    lseek(file, 0, SEEK_SET);
    printf("File size: %ld\n", (long)file_size);

    // Allocate memory to store the file content
    char* file_content = (char*)malloc(file_size);
    if (file_content == NULL) 
    {
        printf("Memory allocation failed.\n");
        close(file);
        return -1; // Memory allocation failed
    }

    // Read the file content into memory
    ssize_t bytes_read = read(file, file_content, file_size);
    if (bytes_read != file_size) 
    {
        printf("Error reading file.\n");
        close(file);
        free(file_content);
        return -1; // Error reading file
    }
    printf("File content: %s\n", file_content);

    // Close the file
    close(file);

    // Open the file in write mode in the server directory
    int server_file = open(server_filename, O_WRONLY | O_CREAT, 0666);
    if (server_file == -1) 
    {
        fprintf(stderr, "Error opening server file: %s (errno=%d)\n", strerror(errno), errno);
        free(file_content);
        return -1; // Error opening server file
    }

    // Write the file content to the server's file
    ssize_t bytes_written = write(server_file, file_content, file_size);
    if (bytes_written != file_size) 
    {
        printf("Error writing to server file.\n");
        close(server_file);
        free(file_content);
        return -1; // Error writing to server file
    }

    // Close the server file
    close(server_file);

    // Free the memory allocated for file content
    free(file_content);

    return 0; // Success
}

int download_file(const char* filename, const char* server_dir) 
{
    printf("In download_file\n");
    // Check if the file exists in the server directory
    char server_filename[256];
    snprintf(server_filename, sizeof(server_filename), "%s", filename);
    if (access(server_filename, F_OK) == -1) 
    {
        fprintf(stderr, "File does not exist in the server directory: %s\n", server_filename);
        return -1; // File does not exist in the server directory
    }

    FILE* server_file = fopen(server_filename, "r");
    if (server_file == NULL) 
    {
        fprintf(stderr, "Error opening server file: %s\n", strerror(errno));
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
        fprintf(stderr, "Memory allocation failed.\n");
        fclose(server_file);
        return -1; // Memory allocation failed
    }

    // Read the file content from the server file
    if (fread(file_content, 1, file_size, server_file) != file_size) 
    {
        fprintf(stderr, "Error reading server file.\n");
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
        fprintf(stderr, "Error writing to local file.\n");
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

void handle_request(request_t request, queue_t *waiting_list, queue_t *connected_list, char *dirname, int max_clients)
{
    printf("In handle_request\n");
    char log[100];
    int client_fd;
    char client_fifo[CLIENT_FIFO_NAME_LEN];
    printf("Client %d requested operation %d\n", request.client_pid, request.operation_type);

    snprintf(client_fifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, request.client_pid);
    if((client_fd = open(client_fifo, O_WRONLY)) == -1)
    {
        fprintf(stderr, "Error opening client FIFO: %s (errno=%d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }
    else
    {
        fprintf(stdout, "Client FIFO opened: %d\n", client_fd);
    }

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

        switch (request.operation_type)
        {
            case HELP:
                if(strcmp(request.command.filename, "") != 0)
                {
                    send_response(SUCCESS, help_for_operation(request.command.filename), client_fd, request.client_pid);
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
                    printf("client dir: %s\n", request.command.data);
                    printf("dirname: %s\n", dirname);
                    if (upload_file(request.command.filename, request.command.data, dirname) == 0) 
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
            case QUIT:
                snprintf(log, sizeof(log), "Client %d killed.\n", request.client_pid);
                log_message(log);
                kill(request.client_pid, SIGKILL);
                break;
            default:
                break;
        }
    
}

int main(int argc, char *argv[])
{
    int server_fd, dummy_fd, max_clients;
    int client_fd = 0;
    char server_fifo[100];
    request_t request; 
    response_t connction_response;
    queue_t *waiting_list = create_queue();
    queue_t *connected_list = create_queue();
    char *dirname = NULL;
    char message[100];
    char client_fifo[CLIENT_FIFO_NAME_LEN];

    signal(SIGINT, sigint_handler);

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
    snprintf(server_fifo, SERVER_FIFO_NAME_LEN, SERVER_FIFO_TEMPLATE, getpid());
    print(server_fifo);
    if(mkfifo(server_fifo, 0666) == -1)
    {
        fprintf(stderr, "Error creating server FIFO: %s (errno=%d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    server_fd = open(server_fifo, O_RDONLY|O_NONBLOCK);
    if(server_fd == -1)
    {
        fprintf(stderr, "Error opening server FIFO: %s (errno=%d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    /* Open the server FIFO for writing so that it doesn't return EOF */
    dummy_fd = open(server_fifo, O_WRONLY );
    if(dummy_fd == -1)
    {
        fprintf(stderr, "Error opening server FIFO for writing: %s (errno=%d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    // Server Start Up Message
    snprintf(message, sizeof(message), "Server started. PID: %d\n", getpid());
    log_message(message);
    fprintf(stdout, "Server started with PID=%d\n", getpid());

    while(1)
    {
        if(request.operation_type == KILL_SERVER)
        {
            printf("Kill server request received\n");
            // Directly initiate shutdown sequence
            unlink(server_fifo); // Remove the server FIFO to clean up
            exit(EXIT_SUCCESS);  // Exit directly
        }
        /*
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
        */
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
        printf("Operation type: %d\n", request.operation_type);
        if(request.operation_type == KILL_SERVER)
        {
            printf("Kill server request received\n");
            // Directly initiate shutdown sequence
            unlink(server_fifo); // Remove the server FIFO to clean up
            exit(EXIT_SUCCESS);  // Exit directly
        }

            pid_t pid = fork();
            if(pid < 0)
            {
                perror("Fork failed");
                exit(EXIT_FAILURE);
            }
            else if(pid == 0)
            {
                if(request.operation_type == NONE)
                {
                    enqueue(waiting_list, request.client_pid);
                    int client_fd2;
                    char client_fifo2[CLIENT_FIFO_NAME_LEN];
                    snprintf(client_fifo2, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, request.client_pid);
                    if((client_fd2 = open(client_fifo2, O_WRONLY)) == -1)
                    {
                        fprintf(stderr, "Error opening client FIFO: %s (errno=%d)\n", strerror(errno), errno);
                        exit(EXIT_FAILURE);
                    }
                    else
                    {
                        fprintf(stdout, "Client FIFO2 opened: %d\n", client_fd2);
                    }
                    if(kill_signal_received == 1)
                    {
                        exit(EXIT_SUCCESS);
                    }
                    send_response(SUCCESS, "Connecteeeeeed.\n", client_fd2, request.client_pid);
                }
                else if(request.operation_type == KILL_SERVER)
                {
                    printf("Kill server request received\n");
                    kill_signal_received = 1;
                    printf("Kill signal value: %d\n", kill_signal_received);
                    kill_parent_and_children(getppid());
                    exit(EXIT_SUCCESS);
                }
                else
                {
                    handle_request(request, waiting_list, connected_list, dirname, max_clients);
                    exit(EXIT_SUCCESS);
                }
    
            }
            else 
            {
                if(kill_signal_received==1)
                {
                    kill_parent_and_children(getpid());
                }
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
    exit(EXIT_SUCCESS);
}
