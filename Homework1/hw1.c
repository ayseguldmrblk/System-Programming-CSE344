#include "hw1.h"

int main() 
{
    char *command = NULL;

    // Create or clear the log file
    int logFile = open(LOG_FILE, O_WRONLY | O_TRUNC | O_CREAT, 0666);
    if (logFile == -1) 
    {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }
    if (close(logFile) == -1) 
    {
        perror("Error closing log file");
        exit(EXIT_FAILURE);
    }

    while (1) 
    {
        command = readLine();

        // Exit condition
        if (strcmp(command, "exit") == 0) 
        {
            writeToLog(command);
            free(command);
            break;
        }

        // Fork a child process
        pid_t pid = fork();

        if (pid < 0) 
        {
            perror("Fork failed");
            free(command);
            exit(EXIT_FAILURE);
        } else if (pid == 0) 
        {
            // Child process
            executeCommand(command);
            free(command);
            exit(EXIT_SUCCESS);
        } else 
        {
            // Parent process
            int status;
            waitpid(pid, &status, 0); // Wait for child process to finish
            writeToLog("Parent process (PID: ");
            char parentPid[20];
            sprintf(parentPid, "%d", getpid());
            writeToLog(parentPid);
            writeToLog(") waited for child process (PID: ");
            char childPid[20];
            sprintf(childPid, "%d", pid);
            writeToLog(childPid);
            writeToLog(") with exit status: ");
            char exitStatus[20];
            sprintf(exitStatus, "%d\n", WEXITSTATUS(status));
            writeToLog(exitStatus);
            writeToLog("Command executed: ");
            writeToLog(command);
            writeToLog("\n");
        }
    }

    exit(EXIT_SUCCESS);
}
