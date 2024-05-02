#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#define ARRAY_SIZE 2

volatile sig_atomic_t child_exit_count = 0;

int safe_print(const char *message) 
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

void sigchld_handler(int sig) 
{
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) 
    {
        safe_print("Signal handler invoked\n");
        char message[100];
        if (WIFEXITED(status)) 
        {
            snprintf(message, sizeof(message), "Child process %d exited with status: %d\n", pid, WEXITSTATUS(status));
        } else 
        {
            if (errno == ECHILD) 
            {
                snprintf(message, sizeof(message), "No more child processes to wait for\n");
            } else 
            {
                snprintf(message, sizeof(message), "Error waiting for child process: %s\n", strerror(errno));
            }
        }
        safe_print(message);
        child_exit_count++;
    }
}

void create_fifos() 
{
    char *fifo1 = "fifo1";
    char *fifo2 = "fifo2";

    if (mkfifo(fifo1, 0666) < 0 || mkfifo(fifo2, 0666) < 0) 
    {
        perror("Failed to create FIFOs");
        exit(EXIT_FAILURE);
    }
    safe_print("FIFOs created successfully.\n");
}

void generate_random_numbers(int numbers[]) 
{
    srand(time(NULL));
    for (int i = 0; i < ARRAY_SIZE; i++) 
    {
        numbers[i] = rand() % 100; // Generate random numbers
    }
}

void write_to_fifo(int fd, int numbers[]) 
{
    for (int i = 0; i < ARRAY_SIZE; i++) 
    {
        write(fd, &numbers[i], sizeof(int));
    }
}

void parent_process(int fd1, int fd2, int numbers[], int total_children) 
{
    safe_print("Parent Process: Writing array to FIFO1 and command to FIFO2...\n");
    write_to_fifo(fd1, numbers);
    write_to_fifo(fd2, numbers);
    close(fd1);
    sleep(1); // Ensure children have enough time to read from the FIFOs before proceeding
    char command[] = "multiply\0";  // Ensure it's null-terminated
    write(fd2, command, strlen(command) + 1);  // Use strlen + 1 to include the null terminator
    close(fd2);
    
    while (1) 
    {
        if (child_exit_count >= total_children) 
        {  // Check if all child processes have exited
            safe_print("All child processes have exited.\n");
            break; // Exit the loop if all child processes have exited
        }
    }
}

void child_process1() 
{
    int fd1 = open("fifo1", O_RDONLY);
    int num, sum = 0;
    while (read(fd1, &num, sizeof(num)) > 0) 
    {
        sum += num;
    }
    char message[100];
    snprintf(message, sizeof(message), "Child Process 1: Calculated sum: %d\n", sum);
    safe_print(message);
    close(fd1);

    int fd2 = open("fifo2", O_WRONLY | O_APPEND );
    if (fd2 == -1) 
    {
        perror("Failed to open FIFO2 for writing in Child Process 1");
        exit(EXIT_FAILURE);
    }
    write(fd2, &sum, sizeof(sum));
    safe_print("Child Process 1: wrote to fifo2\n");
    close(fd2);
    exit(sum);
}

void child_process2() 
{
    sleep(10);  // Ensure Child Process 1 has time to write the sum
    int fd2 = open("fifo2", O_RDONLY);
    int numbers[ARRAY_SIZE];
    int sum_from_child1;
    char cmd_buffer[20] = {0};
    int product = 1;

    for (int i = 0; i < ARRAY_SIZE; i++) 
    {
        if (read(fd2, &numbers[i], sizeof(numbers[i])) > 0) 
        {
            product *= numbers[i];
            safe_print("Child Process 2: multiplying\n");
        }
    }

    if (read(fd2, &sum_from_child1, sizeof(sum_from_child1)) > 0) 
    {
        char message[100];
        snprintf(message, sizeof(message), "Child Process 2: Sum received: %d\n", sum_from_child1);
        safe_print(message);
    }

    if (read(fd2, cmd_buffer, sizeof(cmd_buffer) - 1) > 0) 
    {
        char message[100];
        snprintf(message, sizeof(message), "Child Process 2: Command received: %s\n", cmd_buffer);
        safe_print(message);
    }

    if (strcmp(cmd_buffer, "multiply") == 0) 
    {
        int final_sum = product + sum_from_child1;  // Add the product to the sum received
        char message[100];
        snprintf(message, sizeof(message), "Child Process 2: Final sum after addition: %d\n", final_sum);
        safe_print(message);
    } else 
    {
        safe_print("Child Process 2: No valid command received.\n");
    }

    close(fd2);
    exit(0);
}

int main(int argc, char *argv[]) 
{
    if (argc != 2) 
    {
        fprintf(stderr, "Usage: %s <integer>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int input = atoi(argv[1]);
    char message[100];
    snprintf(message, sizeof(message), "Received integer: %d\n", input);
    safe_print(message);

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    create_fifos();

    int numbers[ARRAY_SIZE];
    generate_random_numbers(numbers);
    safe_print("Array filled with random numbers:\n");
    for (int i = 0; i < ARRAY_SIZE; i++) {
        snprintf(message, sizeof(message), "%d ", numbers[i]);
        safe_print(message);
    }
    safe_print("\n");

    pid_t pid1 = fork();
    if (pid1 == 0) 
    {  // Child Process 1
        child_process1();
    }

    pid_t pid2 = fork();
    if (pid2 == 0) 
    {  // Child Process 2
        child_process2();
    }

    int fd1 = open("fifo1", O_WRONLY);
    int fd2 = open("fifo2", O_WRONLY | O_APPEND);
    if (fd1 == -1 || fd2 == -1) 
    {
        perror("Failed to open FIFOs");
        exit(EXIT_FAILURE);
    }

    parent_process(fd1, fd2, numbers, 2); // Total number of children processes is 2

    safe_print("Exit statuses of all processes:\n");
    int status;
    while (wait(&status) > 0) 
    {
        if (WIFEXITED(status)) 
        {
            snprintf(message, sizeof(message), "Child process %d exited with status: %d\n", pid1, WEXITSTATUS(status));
        } else 
        {
            if (errno == ECHILD) 
            {
                snprintf(message, sizeof(message), "No more child processes to wait for\n");
            } else 
            {
                snprintf(message, sizeof(message), "Error waiting for child process: %s\n", strerror(errno));
            }
        }
        safe_print(message);
    }

    safe_print("Parent Process: Exiting.\n");
    return EXIT_SUCCESS;
}