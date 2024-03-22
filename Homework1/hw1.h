#ifndef HW1_H
#define HW1_H

#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h> 

#define LOG_FILE "log.txt"

const char info[] = "gtuStudentGrades grades.txt -> File Creation\n"
                    "addStudentGrade \"Name Surname\" \"AA\" -> Add Student Grade\n"
                    "searchStudent \"Name Surname\" -> Search Student Grades\n"
                    "sortAll \"grades.txt\" -> Sort All Grades\n"
                    "showAll \"grades.txt\" -> Show All Grades\n"
                    "listGrades \"grades.txt\" -> List First 5 Entries\n"
                    "listSome \"numOfEntries\" \"pageNumber\" \"grades.txt\" -> List Specific Entries\n";

typedef struct {
    char name[100];
    char grade[3];
} StudentGrade;

typedef enum {
    BY_NAME,
    BY_GRADE
} SortBy;

typedef enum {
    ASCENDING,
    DESCENDING
} SortOrder;

int compareByName(const void *a, const void *b) 
{
    const StudentGrade *gradeA = (const StudentGrade *)a;
    const StudentGrade *gradeB = (const StudentGrade *)b;
    return strcmp(gradeA->name, gradeB->name);
}

int compareByGrade(const void *a, const void *b) 
{
    const StudentGrade *gradeA = (const StudentGrade *)a;
    const StudentGrade *gradeB = (const StudentGrade *)b;
    return strcmp(gradeA->grade, gradeB->grade);
}

// Comparison functions for descending order
int compareByNameDesc(const void *a, const void *b) 
{
    return -compareByName(a, b);
}

int compareByGradeDesc(const void *a, const void *b) 
{
    return -compareByGrade(a, b);
}

void sortAll(const char *filename, SortBy sortBy, SortOrder sortOrder) 
{
    size_t num_grades = 0;
    StudentGrade grades[1000];
    ssize_t bytes_read;
    char buffer[1024];
    char *name;
    char *surname;
    char *grade;

    // Open the file for reading
    int fd = open(filename, O_RDONLY);
    if (fd == -1) 
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) 
    {
        char *ptr = buffer;
        while ((name = strtok_r(ptr, " ", &ptr)) != NULL &&
               (surname = strtok_r(NULL, " ", &ptr)) != NULL &&
               (grade = strtok_r(NULL, "\n", &ptr)) != NULL) 
        {
            // Store name and grade
            strcpy(grades[num_grades].name, name);
            strcat(grades[num_grades].name, " ");
            strcat(grades[num_grades].name, surname);
            strcpy(grades[num_grades].grade, grade);

            // Increment the number of grades
            num_grades++;
        }
    }

    if (bytes_read == -1) 
    {
        perror("Error reading from file");
        exit(EXIT_FAILURE);
    }

    // Determine comparison function based on sort criteria
    int (*compareFunction)(const void *, const void *);
    if (sortBy == BY_NAME) 
    {
        compareFunction = (sortOrder == ASCENDING) ? compareByName : compareByNameDesc;
    } else 
    { // BY_GRADE
        compareFunction = (sortOrder == ASCENDING) ? compareByGrade : compareByGradeDesc;
    }

    // Sort the grades
    qsort(grades, num_grades, sizeof(StudentGrade), compareFunction);

    // Print sorted grades for debugging
    write(STDOUT_FILENO, "Sorted grades:\n", strlen("Sorted grades:\n"));
    for (size_t i = 0; i < num_grades; i++) 
    {
        write(STDOUT_FILENO, grades[i].name, strlen(grades[i].name));
        write(STDOUT_FILENO, " ", 1);
        write(STDOUT_FILENO, grades[i].grade, strlen(grades[i].grade));
        write(STDOUT_FILENO, "\n", 1);
    }

    // Close the file
    if (close(fd) == -1) 
    {
        perror("Error closing file");
        exit(EXIT_FAILURE);
    }
}

char *readLine()
{
    char* line = (char*)malloc(1024);
    ssize_t bytes_read = 0;
    char c;

    while(bytes_read < 1023)
    {
        ssize_t check = read(STDIN_FILENO, &c, 1);

        if(check < 0)
        {
            char err_msg[256];
            strerror_r(errno, err_msg, sizeof(err_msg));
            write(STDERR_FILENO, err_msg, strlen(err_msg));
            /* No need to check if the error is caused by write or read, just exit */
            exit(errno);
        }
        /* The allocated buffer for line is full or end of file character occurs */
        else if(check == 0 || c == '\n') 
        {
            break; 
        }
        else 
        {
            line[bytes_read] = c;
            bytes_read = bytes_read + 1; 
        }
    }
    line[bytes_read] = '\0';
    return line;
}

void createFile(char *filename)
{
    int fd = open(filename, O_CREAT | O_RDWR, 0666);
    if(fd == -1)
    {
        char err_msg[256];
        strerror_r(errno, err_msg, sizeof(err_msg));
        write(STDERR_FILENO, err_msg, strlen(err_msg));
    }
    if(close(fd) != 0)
    {
        perror("Close Error"); // Print the error message
    }
}

void addStudentGrade(const char *name, const char *grade) 
{
    // Open the file for appending
    int fd = open("grades.txt", O_WRONLY | O_APPEND | O_CREAT, 0666);
    if (fd == -1) 
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    
    // Write the name and grade to the file
    char buffer[1024];
    int bytes_written = snprintf(buffer, sizeof(buffer), "%s %s\n", name, grade);
    if (bytes_written < 0 || (size_t)bytes_written >= sizeof(buffer)) 
    {
        perror("Error formatting data");
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    ssize_t bytes_written_to_file = write(fd, buffer, strlen(buffer));
    if (bytes_written_to_file == -1) 
    {
        perror("Error writing to file");
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    // Close the file
    if (close(fd) == -1) 
    {
        perror("Error closing file");
        exit(EXIT_FAILURE);
    }
}

void searchStudent(const char *name)
{
    char buffer[1024];
    ssize_t bytes_read;
    int found = 0;

    int fd = open("grades.txt", O_RDONLY);
    if (fd == -1) 
    {
        perror("Open Error");
        return;  // Exit the function early if file open fails
    }
    
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) 
    {
        // Null-terminate the buffer to prevent strtok from going beyond it
        buffer[bytes_read] = '\0';

        // Tokenize the buffer using newline as the delimiter
        char *saveptr;
        char *line = strtok_r(buffer, "\n", &saveptr);
        while (line != NULL) {
            // Extract student name and surname from the line
            char *student_name = strtok(line, " ");
            char *student_surname = strtok(NULL, " ");
            
            // Concatenate name and surname to form the full name
            char student_fullname[100]; // Assuming maximum length for name and surname is 50 characters each
            strcpy(student_fullname, student_name);
            strcat(student_fullname, " ");
            strcat(student_fullname, student_surname);
            // Skip to the next token, which should be the grade
            char *student_grade = strtok(NULL, " ");
            
            // Check if the student name matches the provided name
            if (strcmp(student_fullname, name) == 0) 
            {
                found = 1;
                write(STDOUT_FILENO, student_fullname, strlen(student_fullname));
                write(STDOUT_FILENO, " ", 1);
                write(STDOUT_FILENO, student_grade, strlen(student_grade));
                write(STDOUT_FILENO, "\n", 1);
                break;  // Exit the loop once a match is found
            }
            // Move to the next line
            line = strtok_r(NULL, "\n", &saveptr);
        }
    }
    
    if (!found) 
    {
        printf("Student not found.\n");
    }
    
    if (close(fd) == -1) 
    {
        perror("Close Error");
    }
}


void displayAll(const char *filename) 
{
    ssize_t bytes_read;
    char buffer[1024];
    // Open the file for reading
    int fd = open(filename, O_RDONLY);
    if (fd == -1) 
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Read and display all student grades from the file
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) 
    {
        write(STDOUT_FILENO, buffer, bytes_read);
    }

    // Check for read error
    if (bytes_read == -1) 
    {
        perror("Error reading from file");
        exit(EXIT_FAILURE);
    }

    // Close the file
    if (close(fd) == -1) 
    {
        perror("Error closing file");
        exit(EXIT_FAILURE);
    }
}

void displayFirst5(const char *filename) 
{
    ssize_t bytes_read;
    char buffer[1024];
    int lines_read = 0; // Counter for lines read
    // Open the file for reading
    int fd = open(filename, O_RDONLY);
    if (fd == -1) 
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Read and display the first 5 student grades from the file
    while (lines_read < 5) // Loop until 5 lines are read
    {
        bytes_read = read(fd, buffer, sizeof(buffer));
        if (bytes_read <= 0) 
        {
            break; // End of file reached or error occurred
        }
        // Count lines in the buffer
        for (int i = 0; i < bytes_read; i++) 
        {
            if (buffer[i] == '\n') 
            {
                lines_read++;
                if (lines_read >= 5) 
                {
                    // If 5 lines are read, exit the loop
                    bytes_read = i + 1; // Adjust bytes_read to stop reading
                    break;
                }
            }
        }
        // Write buffer content to stdout
        write(STDOUT_FILENO, buffer, bytes_read);
    }

    // Close the file
    if (close(fd) == -1) 
    {
        perror("Error closing file");
        exit(EXIT_FAILURE);
    }
}

void displayPage(const char *filename, int numOfEntries, int pageNumber) 
{
    int fd, lines_read = 0, offset = 0;
    ssize_t bytes_read;
    char buffer[1024];

    // Open the file in read-only mode
    fd = open(filename, O_RDONLY);
    if (fd == -1) 
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Calculate the offset based on the number of newline characters
    while (lines_read < (pageNumber - 1) * numOfEntries) 
    {
        bytes_read = read(fd, buffer, sizeof(buffer));
        if (bytes_read == -1) 
        {
            perror("Error reading from file");
            exit(EXIT_FAILURE);
        }
        if (bytes_read == 0) 
        {
            // Reached end of file before reaching the desired page
            write(STDOUT_FILENO, "End of file reached.\n", strlen("End of file reached.\n"));
            if(close(fd) == -1)
            {
                perror("Error closing file");
                exit(EXIT_FAILURE);
            }
            return;
        }
        for (int i = 0; i < bytes_read; i++) 
        {
            if (buffer[i] == '\n') 
            {
                lines_read++;
            }
            if (lines_read >= (pageNumber - 1) * numOfEntries) 
            {
                offset += i + 1;
                break;
            }
        }
    }

    // Seek to the starting line
    lseek(fd, offset, SEEK_SET);

    // Read and display lines until the desired number of lines are displayed or the end of file is reached
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) 
    {
        // Output the read buffer
        for (int i = 0; i < bytes_read; i++) 
        {
            if (buffer[i] == '\n') 
            {
                lines_read++;
            }
            if (lines_read >= pageNumber * numOfEntries) 
            {
                break;
            }
            write(STDOUT_FILENO, &buffer[i], 1);
        }
        if (lines_read >= pageNumber * numOfEntries) 
        {
            break;
        }
    }

    // Check for errors or end of file
    if (bytes_read == -1) 
    {
        perror("Error reading from file");
        exit(EXIT_FAILURE);
    }

    // Close the file
    if (close(fd) == -1) 
    {
        perror("Error closing file");
        exit(EXIT_FAILURE);
    }
}

void writeToLog(const char *message) 
{
    // Open the log file in append mode
    int logFile = open(LOG_FILE, O_WRONLY | O_APPEND | O_CREAT, 0666);
    if (logFile == -1) 
    {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }

    // Write the message to the log file
    ssize_t bytes_written = write(logFile, message, strlen(message));
    if (bytes_written == -1) 
    {
        perror("Error writing to log file");
        close(logFile);
        exit(EXIT_FAILURE);
    }

    // Close the log file
    if (close(logFile) == -1) 
    {
        perror("Error closing log file");
        exit(EXIT_FAILURE);
    }
}

void executeCommand(const char *command) 
{
    char *token = strtok((char *)command, " ");
    if (strcmp(token, "gtuStudentGrades") == 0) 
    {
        // Handle gtuStudentGrades command
        // If a filename is provided, perform file creation
        char *filename = strtok(NULL, " ");
        if (filename != NULL) 
        {
            createFile(filename);
        } else 
        {
            // If no filename is provided, print usage information
            write(STDOUT_FILENO, info, strlen(info));
        }
    } else if (strcmp(token, "addStudentGrade") == 0) 
    {
        // Get the name
        char *name = strtok(NULL, " ");
        char *surname = strtok(NULL, " ");
        
        // Concatenate the name and surname
        char fullName[100];
        snprintf(fullName, sizeof(fullName), "%s %s", name, surname);

        // Get the grade
        char *grade = strtok(NULL, " ");

        // Call the addStudentGrade function with the parsed name and grade
        addStudentGrade(fullName, grade);
    } else if (strcmp(token, "searchStudent") == 0) 
    {
        // Handle searchStudent command
        char *name = strtok(NULL, " "); // Get the name
        char *surname = strtok(NULL, " "); // Get the surname
        
        // Concatenate name and surname
        char full_name[256]; // Assuming maximum length of name and surname is 255 characters
        snprintf(full_name, sizeof(full_name), "%s %s", name, surname);
        searchStudent(full_name); // Send the full name to the searchStudent function
    } else if (strcmp(token, "sortAll") == 0) 
    {
        char *filename = strtok(NULL, " ");
        int choice;
        write(STDOUT_FILENO, "Enter 1 for sorting by name or 2 for sorting by grade: ", strlen("Enter 1 for sorting by name or 2 for sorting by grade: "));
        char choiceStr[2];
        read(STDIN_FILENO, choiceStr, sizeof(choiceStr));
        choice = atoi(choiceStr);

        SortBy sortBy;
        switch (choice) 
        {
            case 1:
                sortBy = BY_NAME;
                break;
            case 2:
                sortBy = BY_GRADE;
                break;
            default:
                write(STDOUT_FILENO, "Invalid choice. Defaulting to sorting by name.\n", strlen("Invalid choice. Defaulting to sorting by name.\n"));
                sortBy = BY_NAME;
                break;
        }

        int sortOrderChoice;
        write(STDOUT_FILENO, "Enter 1 for ascending order or 2 for descending order: ", strlen("Enter 1 for ascending order or 2 for descending order: "));
        char sortOrderChoiceStr[2];
        read(STDIN_FILENO, sortOrderChoiceStr, sizeof(sortOrderChoiceStr));
        sortOrderChoice = atoi(sortOrderChoiceStr);

        SortOrder sortOrder;
        switch (sortOrderChoice) 
        {
            case 1:
                sortOrder = ASCENDING;
                break;
            case 2:
                sortOrder = DESCENDING;
                break;
            default:
                write(STDOUT_FILENO, "Invalid choice. Defaulting to ascending order.\n", strlen("Invalid choice. Defaulting to ascending order.\n"));
                sortOrder = ASCENDING;
                break;
        }
        printf("Sorting by %s in %s order...\n", sortBy == BY_NAME ? "name" : "grade", sortOrder == ASCENDING ? "ascending" : "descending");
        sortAll(filename, sortBy, sortOrder);
    } else if (strcmp(token, "showAll") == 0) 
    {
        // Handle showAll command
        char *filename = strtok(NULL, " ");
        displayAll(filename);
    } else if (strcmp(token, "listGrades") == 0) 
    {
        // Handle listGrades command
        char *filename = strtok(NULL, " ");
        displayFirst5(filename);
    } else if (strcmp(token, "listSome") == 0) 
    {
        // Handle listSome command
        int numOfEntries = atoi(strtok(NULL, " "));
        int pageNumber = atoi(strtok(NULL, " "));
        char *filename = strtok(NULL, " ");
        displayPage(filename, numOfEntries, pageNumber);
    } else 
    {
        write(STDOUT_FILENO, "Invalid command: ", strlen("Invalid command: "));
        write(STDOUT_FILENO, command, strlen(command));
        write(STDOUT_FILENO, "\n", 1);
    }
}

#endif // HW1_H
