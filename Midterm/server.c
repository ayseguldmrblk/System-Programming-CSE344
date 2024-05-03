#include <stdlib.h>
#include "concurrent_file_access_system.h"
#include "queue.h"
#include <stdio.h>
#include <unistd.h> 

int main(int argc, char *argv[])
{
    int server_fd, dummy_fd, client_fd, max_clients;
    char server_fifo[100];
    request_t request; 
    response_t response;
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

    while(1)
    {
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
        
        if(request.connection_type == CONNECT && request.operation_type == NONE)
        {
            if(connected_list->size == max_clients)
            {
                enqueue(waiting_list, request.client_pid);
                fprintf(stdout, "Client %d added to waiting list\n", request.client_pid);
                send_response(WAIT, "Server is full. Please wait.\n", client_fd, request.client_pid);
            }
            else
            {
                enqueue(connected_list, request.client_pid);
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
                enqueue(connected_list, request.client_pid);
                fprintf(stdout, "Client %d connected\n", request.client_pid);
                send_response(SUCCESS, "Connected to server.\n", client_fd, request.client_pid);
            }
        }
        else if(is_client_in_queue(connected_list, request.client_pid == 1))
        {
            switch (request.operation_type)
            {
                case HELP:
                    if(request.command.filename != NULL)
                    {
                        send_response(SUCCESS, help_for_operation(request.operation_type), client_fd, request.client_pid);
                    }
                    else
                    {
                        send_response(SUCCESS, help_available_operations(), client_fd, request.client_pid);
                    }
                    break;
                case LIST:
                    break;
                case READ_FILE:
                    break;
                case WRITE_FILE:
                    break;
                case UPLOAD:
                    break;
                case DOWNLOAD:
                    break;
                case ARCHIVE_SERVER:
                    break;
                case KILL_SERVER:
                    break;
                case QUIT:
                    break;
                default:
                    break;
            }
        }
    }

    exit(EXIT_SUCCESS);
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
