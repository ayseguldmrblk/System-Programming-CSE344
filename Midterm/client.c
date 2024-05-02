#include "concurrent_file_access_system.h"

int main(int argc, char *argv[])
{
    int server_pid;
    int server_fifo_fd, client_fifo_fd;
    char message[100];
    char client_fifo[CLIENT_FIFO_NAME_LEN];

    if(create_client_fifo(client_fifo) == -1)
    {
        exit(EXIT_FAILURE);
    }
    print("Client FIFO created\n");

    if(argc != 3)
    {
        fprintf(stderr, "Usage: %s <Connect/tryConnect> ServerPID\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    else 
    {
        int connect = connect_or_tryconnect(argv[1]);
    
        if(connect == 0) //Connect
        {
            server_pid = atoi(argv[2]);
            umask(0);
            snprintf(client_fifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)getpid());
            server_fifo_fd = open(SERVER_FIFO, O_WRONLY);
            if(server_fifo_fd == -1)
            {
                fprintf(stderr, "Error opening server FIFO: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            
            sprintf(message, "Client %d connected\n", getpid());
            log_message(message);

        }
        else if(connect == 1) //tryConnect
        {
            int server_pid = atoi(argv[2]);
            sprintf(message, "Client %d trying to connect\n", getpid());
            log_message(message);

        }
        else
        {
            fprintf(stderr, "Usage: %s <Connect/tryConnect> ServerPID\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}