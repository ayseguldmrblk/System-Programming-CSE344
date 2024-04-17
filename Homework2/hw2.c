#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define FIFO1 "fifo1"
#define FIFO2 "fifo2"
#define COMMAND "multiply"

int child_count = 2; // Counter for the number of child processes spawned

void sigchld_handler(int sig) {
    write(STDOUT_FILENO, "Signal handler invoked\n", strlen("Signal handler invoked\n"));
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            char buf[50];
            snprintf(buf, sizeof(buf), "Child process %d exited with status: %d\n", pid, WEXITSTATUS(status));
            write(STDOUT_FILENO, buf, strlen(buf));
            if (child_count > 0) {
                child_count--; // Decrement the child count only if it's greater than 0
            }
        }
    }
}

void print_exit_status(pid_t pid, const char *process_name) {
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }
    if (WIFEXITED(status)) {
        char buf[50];
        snprintf(buf, sizeof(buf), "%s exit status: %d\n", process_name, WEXITSTATUS(status));
        write(STDOUT_FILENO, buf, strlen(buf));
    } else {
        char buf[50];
        snprintf(buf, sizeof(buf), "%s did not exit normally\n", process_name);
        write(STDOUT_FILENO, buf, strlen(buf));
    }
}

void print_proceeding_message(int duration) {
    while (duration > 0) {
        write(STDOUT_FILENO, "Parent Process: Proceeding...\n", strlen("Parent Process: Proceeding...\n"));
        sleep(2);
        duration -= 2;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        write(STDERR_FILENO, "Usage: ./program <integer>\n", strlen("Usage: ./program <integer>\n"));
        exit(EXIT_FAILURE);
    }

    int n = atoi(argv[1]);
    srand(time(NULL));

    int numbers[n];
    // Fill the array with random numbers
    write(STDOUT_FILENO, "Array filled with random numbers:\n", strlen("Array filled with random numbers:\n"));
    for (int i = 0; i < n; ++i) {
        numbers[i] = rand() % 100 + 1; // Generate random numbers between 1 and 100
        char buf[20];
        snprintf(buf, sizeof(buf), "%d ", numbers[i]);
        write(STDOUT_FILENO, buf, strlen(buf));
    }
    write(STDOUT_FILENO, "\n", strlen("\n"));

    // Create FIFOs
    if (mkfifo(FIFO1, 0666) == -1 || mkfifo(FIFO2, 0666) == -1) {
        perror("Error creating FIFOs");
        exit(EXIT_FAILURE);
    } else {
        write(STDOUT_FILENO, "FIFOs created successfully.\n", strlen("FIFOs created successfully.\n"));
    }

    // Set signal handler for SIGCHLD
    signal(SIGCHLD, sigchld_handler);

    // Fork first child process
    pid_t pid1 = fork();

    if (pid1 == -1) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    } else if (pid1 == 0) {
        // Child process 1
        sleep(10);
        int fd1 = open(FIFO1, O_WRONLY);
        if (fd1 == -1) {
            perror("Error opening FIFO1");
            exit(EXIT_FAILURE);
        }

        // Write the array to FIFO1
        write(STDOUT_FILENO, "Parent Process: Writing array to FIFO1...\n", strlen("Parent Process: Writing array to FIFO1...\n"));
        if (write(fd1, numbers, sizeof(numbers)) == -1) {
            perror("Error writing to FIFO1");
            exit(EXIT_FAILURE);
        }

        close(fd1);
        exit(EXIT_SUCCESS);
    }

    // Fork second child process
    pid_t pid2 = fork();

    if (pid2 == -1) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    } else if (pid2 == 0) {
        // Child process 2
        sleep(10);
        int fd2 = open(FIFO2, O_WRONLY);
        if (fd2 == -1) {
            perror("Error opening FIFO2");
            exit(EXIT_FAILURE);
        }

        if (write(fd2, COMMAND, strlen(COMMAND) + 1) == -1) {  // Write command
            perror("Error writing to FIFO2");
            exit(EXIT_FAILURE);
        }

        close(fd2);
        exit(EXIT_SUCCESS);
    }

    // Parent process
    int sum = 0;

    // Enter a loop, printing a message containing "proceeding" every two seconds
    print_proceeding_message(10);

    // Open FIFO1 for reading sum from Child Process 1
    int fd1_read = open(FIFO1, O_RDONLY);
    if (fd1_read == -1) {
        perror("Error opening FIFO1 for reading");
        exit(EXIT_FAILURE);
    }

    // Read sum from FIFO1
    int numbers_read[n];
    write(STDOUT_FILENO, "Parent Process: Reading array from FIFO1...\n", strlen("Parent Process: Reading array from FIFO1...\n"));
    if (read(fd1_read, numbers_read, sizeof(numbers_read)) == -1) {
        perror("Error reading from FIFO1");
        exit(EXIT_FAILURE);
    }

    write(STDOUT_FILENO, "Parent Process: Read array from FIFO1:\n", strlen("Parent Process: Read array from FIFO1:\n"));
    for (int i = 0; i < n; ++i) {
        char buf[20];
        snprintf(buf, sizeof(buf), "%d ", numbers_read[i]);
        write(STDOUT_FILENO, buf, strlen(buf));
    }
    write(STDOUT_FILENO, "\n", strlen("\n"));

    for (int i = 0; i < n; ++i) {
        sum += numbers_read[i];
    }

    char buf[100];
    snprintf(buf, sizeof(buf), "Parent Process: Received sum from Child Process 1: %d\n", sum);
    write(STDOUT_FILENO, buf, strlen(buf));

    // Close FIFO1 for reading
    close(fd1_read);

    // Open FIFO2 for reading command from Child Process 2
    int fd2_read = open(FIFO2, O_RDONLY);
    if (fd2_read == -1) {
        perror("Error opening FIFO2 for reading");
        exit(EXIT_FAILURE);
    }

    // Read command from FIFO2
    char command[20];
    if (read(fd2_read, command, sizeof(command)) == -1) {
        perror("Error reading from FIFO2");
        exit(EXIT_FAILURE);
    }

    close(fd2_read);

    if (strcmp(command, COMMAND) != 0) {
        write(STDERR_FILENO, "Error: Invalid command received from Child Process 2\n", strlen("Error: Invalid command received from Child Process 2\n"));
        exit(EXIT_FAILURE);
    }

    snprintf(buf, sizeof(buf), "Parent Process: Received command from Child Process 2: %s\n", command);
    write(STDOUT_FILENO, buf, strlen(buf));

    // If the command is "multiply", calculate the multiplication of all array elements
    if (strcmp(command, COMMAND) == 0) {
        int product = 1;
        for (int i = 0; i < n; ++i) {
            product *= numbers_read[i];
        }
        snprintf(buf, sizeof(buf), "Product of all array elements: %d\n", product);
        write(STDOUT_FILENO, buf, strlen(buf));
        sum += product; // Add the product to the sum
    }

    snprintf(buf, sizeof(buf), "Final sum after multiplication: %d\n", sum);
    write(STDOUT_FILENO, buf, strlen(buf));

    // Print exit statuses of all processes
    write(STDOUT_FILENO, "Parent Process: Exit statuses of all child processes:\n", strlen("Parent Process: Exit statuses of all child processes:\n"));
    while (child_count > 0) {
        sleep(1); // Wait for child processes to terminate
    }
    print_exit_status(pid1, "Child Process 1");
    print_exit_status(pid2, "Child Process 2");

    // Destroy FIFOs
    unlink(FIFO1);
    unlink(FIFO2);

    exit(EXIT_SUCCESS);
}
