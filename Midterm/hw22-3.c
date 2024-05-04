#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#define ARRAY_SIZE 2

volatile sig_atomic_t child_exit_count = 0;

void sigchld_handler(int sig) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Signal handler invoked\nChild process %d exited with status: %d\n", pid, WEXITSTATUS(status));
        child_exit_count++;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <integer>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int input = atoi(argv[1]);
    printf("Received integer: %d\n", input);

    char *fifo1 = "fifo1";
    char *fifo2 = "fifo2";

    if (mkfifo(fifo1, 0666) < 0 || mkfifo(fifo2, 0666) < 0) {
        perror("Failed to create FIFOs");
        exit(EXIT_FAILURE);
    }
    printf("FIFOs created successfully.\n");

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    srand(time(NULL));
    int numbers[ARRAY_SIZE];
    for (int i = 0; i < ARRAY_SIZE; i++) {
        numbers[i] = rand() % 100; // Generate random numbers
    }
    printf("Array filled with random numbers:\n");
    for (int i = 0; i < ARRAY_SIZE; i++) {
        printf("%d ", numbers[i]);
    }
    printf("\n");

    // Fork children before opening FIFOs for writing
    pid_t pid1 = fork();
    if (pid1 == 0) {  // Child Process 1
        sleep(10);
        int fd1 = open(fifo1, O_RDONLY);
        int num, sum = 0;
        while (read(fd1, &num, sizeof(num)) > 0) {
            sum += num;
        }
        printf("Child Process 1: Calculated sum: %d\n", sum);
        close(fd1);

        // Write the result to the second FIFO (fifo2)
        int fd2 = open(fifo2, O_WRONLY | O_APPEND );
        if (fd2 == -1) {
            perror("Failed to open FIFO2 for writing in Child Process 1");
            exit(EXIT_FAILURE);
        }
        write(fd2, &sum, sizeof(sum));
        printf("Child Process 1: wrote to fifo2\n");
        close(fd2);

        exit(sum);
    }


    pid_t pid2 = fork();
    if (pid2 == 0) {  // Child Process 2
    sleep(10);  // Ensure Child Process 1 has time to write the sum
    int fd2 = open(fifo2, O_RDONLY);
    int numbers[ARRAY_SIZE];
    int sum_from_child1;
    char cmd_buffer[20] = {0};
    int product = 1;

    // Read array numbers and compute product
    for (int i = 0; i < ARRAY_SIZE; i++) {
        if (read(fd2, &numbers[i], sizeof(numbers[i])) > 0) {
            product *= numbers[i];
            printf("Child Process 2:multiplying\n");
        }
    }

    // Read sum from Child Process 1
    if (read(fd2, &sum_from_child1, sizeof(sum_from_child1)) > 0) {
        printf("Child Process 2: Sum received: %d\n", sum_from_child1);
    }

    // Read command
    if (read(fd2, cmd_buffer, sizeof(cmd_buffer) - 1) > 0) {
        printf("Child Process 2: Command received: %s\n", cmd_buffer);
    }

    // Perform operation if the command is 'multiply'
    if (strcmp(cmd_buffer, "multiply") == 0) {
        int final_sum = product + sum_from_child1;  // Add the product to the sum received
        printf("Child Process 2: Final sum after addition: %d\n", final_sum);
    } else {
        printf("Child Process 2: No valid command received.\n");
    }
    
    close(fd2);
    exit(0);  // Exit with a success status
}






    // Parent writes to FIFO1
    int fd1 = open(fifo1, O_WRONLY);
    int fd2 = open(fifo2, O_WRONLY| O_APPEND);

    if (fd1 == -1 || fd2 == -1) {
        perror("Failed to open FIFOs");
        exit(EXIT_FAILURE);
    }

    printf("Parent Process: Writing array to FIFO1...\n");
    for (int i = 0; i < ARRAY_SIZE; i++) {
        write(fd1, &numbers[i], sizeof(int));
        write(fd2, &numbers[i], sizeof(int));
    }
    close(fd1);
     // Close after writing to ensure children know writing is done.
    sleep(1); 
    // Parent Process: Write command to FIFO2
    //int fd2 = open(fifo2, O_WRONLY );
    char command[] = "multiply\0";  // Ensure it's null-terminated
    write(fd2, command, strlen(command) + 1);  // Use strlen + 1 to include the null terminator
    close(fd2);


    // Wait for both children to exit
    // Wait for both children to exit
    // Parent Process: Main loop to handle process exits
while (1) {
    printf("Parent Process: Proceeding...\n");
    sleep(2);  // Sleep to prevent tight looping

    // Break the loop if all children have exited
    if (child_exit_count >= 2) {  // You have 2 child processes
        printf("All child processes have exited.\n");
        break;
    }
}



    printf("Parent Process: Exit statuses of all child processes:\n");
    return EXIT_SUCCESS;
}