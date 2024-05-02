#include <stdlib.h>
#include "concurrent_file_access_system.h"

int main(int argc, char *argv[])
{
    int server_fd, dummy_fd, client_fd, max_clients;
    char *dirname = NULL;
    char message[100];

    if(argc != 3)
    {
        fprintf(stderr, "Usage: %s <dirname> <max. #ofClients>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    else 
    {
        char *dirname = argv[1];
        int max_clients = atoi(argv[2]);
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

    if(mkfifo(SERVER_FIFO, 0666) == -1)
    {
        fprintf(stderr, "Error creating server FIFO: %s (errno=%d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    if((server_fd = open(SERVER_FIFO, O_RDONLY)) == -1)
    {
        fprintf(stderr, "Error opening server FIFO: %s (errno=%d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    /* Open the server FIFO for writing so that it doesn't return EOF */
    if((dummy_fd = open(SERVER_FIFO, O_WRONLY)) == -1)
    {
        fprintf(stderr, "Error opening server FIFO for writing: %s (errno=%d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    // Server Start Up Message
    snprintf(message, sizeof(message), "Server started. PID: %d\n", getpid());
    log_message(message);
    fprintf(stdout, "Server started with PID=%d\n", getpid());

    
}