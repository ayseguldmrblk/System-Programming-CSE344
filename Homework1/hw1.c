#include "hw1.h"

int main(int argc, char *argv[])
{
    char *command = NULL;
    while(1)
    {
        command = readLine();
        if(strcompare(command, "exit") == 0)
        {
            break;
        }
        else if(strcompare(command, "gtuStudentGrades") == 0)
        {
            if(write(STDOUT_FILENO, info, strlen(info)) == -1)
            {
                perror("Write Error");
            }
        }
    }
    if(command != NULL)
    {
        free(command);
    }
    exit(EXIT_SUCCESS);
}
