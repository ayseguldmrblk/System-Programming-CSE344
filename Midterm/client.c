#include "concurrent_file_access_system.h"

void signal_handler(int signal)
{
    if(signal == SIGINT)
    {
        char message[100];
        snprintf(message, sizeof(message), "Client %d received SIGINT.\n", getpid());
        log_message(message);
        fprintf(stderr, "Client %d received SIGINT.\n", getpid());
        exit(EXIT_SUCCESS);
    }
}

int main(int argc, char *argv[])
{
    int server_pid;
    int server_fifo_fd;
    char message[100];
    char client_fifo[CLIENT_FIFO_NAME_LEN];

    if(signal(SIGINT, signal_handler) == SIG_ERR)
    {
        fprintf(stderr, "Error setting signal handler: %s (errno=%d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    if(argc != 3)
    {
        fprintf(stderr, "Usage: %s <Connect/tryConnect> ServerPID\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    else 
    {
        snprintf(client_fifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, getpid());
        if(create_client_fifo(client_fifo) == -1)
        {
            exit(EXIT_FAILURE);
        }
        print("Client FIFO created.\n");
        int connect_type = (strcmp(argv[1], "Connect") == 0) ? CONNECT : TRY_CONNECT;
        server_pid = atoi(argv[2]);

        char server_fifo[SERVER_FIFO_NAME_LEN];
        snprintf(server_fifo, SERVER_FIFO_NAME_LEN, SERVER_FIFO_TEMPLATE, server_pid);
        server_fifo_fd = open(server_fifo, O_WRONLY);
        if(server_fifo_fd == -1)
        {
            fprintf(stderr, "Error opening server FIFO: %s (errno=%d)\n", strerror(errno), errno);
            exit(EXIT_FAILURE);
        }
        else 
        {
            fprintf(stderr, "Server FIFO opened: %d\n", server_fifo_fd);
        }

        request_t connect_request;
        connect_request.client_pid = getpid();
        connect_request.connection_type = connect_type;
        connect_request.operation_type = NONE; // No operation at first

        if(write(server_fifo_fd, &connect_request, sizeof(request_t)) == -1)
        {
            fprintf(stderr, "Error writing to server FIFO: %s (errno=%d)\n", strerror(errno), errno);
            exit(EXIT_FAILURE);
        }
        else 
        {
            fprintf(stderr, "Connection request sent to server: %d\n", server_pid);
        }

        int client_fifo_fd = open(client_fifo, O_RDONLY); 
        if(client_fifo_fd == -1)
        {
            fprintf(stderr, "Error opening client FIFO: %s (errno=%d)\n", strerror(errno), errno);
            exit(EXIT_FAILURE);
        }
        else
        {
            fprintf(stderr, "Client %d FIFO opened.\n", getpid());
        }

        response_t response;
        if(read(client_fifo_fd, &response, sizeof(response_t)) == -1)
        {
            fprintf(stderr, "Error reading from client FIFO: %s (errno=%d)\n", strerror(errno), errno);
            exit(EXIT_FAILURE);
        }
       
        
        if(response.status == SUCCESS)
        {
            while(1)
            {
                print("Enter command: ");
                fgets(message, sizeof(message), stdin);
                message[strcspn(message, "\n")] = '\0'; 
                
                command_t command = parse_command(message);
                if(command.operation_type == INVALID_OPERATION)
                {
                    printf("Invalid command.\n");
                    continue;
                }
                else 
                {
                    if(command.operation_type == KILL_SERVER)
                    {
                        send_request(getpid(), connect_type, command.operation_type, command, server_fifo_fd);
                    }
                    send_request(getpid(), connect_type, command.operation_type, command, server_fifo_fd);
                }

                // Read response from server 
                if(read(client_fifo_fd, &response, sizeof(response_t)) == -1)
                {
                    fprintf(stderr, "Error reading from client FIFO: %s (errno=%d)\n", strerror(errno), errno);
                    exit(EXIT_FAILURE);
                }
                if(response.status == SUCCESS)
                {
                    fprintf(stdout, "%s\n", response.body);
                }
                else if(response.status == FAILURE)
                {
                    fprintf(stderr, "Operation failed: %s\n", response.body);
                }
                else if(response.status == WAIT)
                {
                    fprintf(stderr, "%s\n", response.body);
                }
            }
        }
        else if(response.status == FAILURE)
        {
            fprintf(stderr, "Connection to server failed: %s", response.body);
        }
        else if(response.status == WAIT)
        {
            fprintf(stderr, "%s", response.body);
        }

        close(server_fifo_fd);
        close(client_fifo_fd);

        unlink(client_fifo);

        exit(EXIT_SUCCESS);

    }
}