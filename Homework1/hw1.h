#ifndef HW1_H
#define HW1_H

#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h> 

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

void sortAll(const char *filename, int byName, int ascending) 
{
    size_t num_grades = 0;
    StudentGrade grades[1000];
    ssize_t bytes_read;
    char buffer[1024];
    // Open the file for reading and writing
    int fd = open(filename, O_RDWR);
    if (fd == -1) 
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Read all student grades from the file
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) 
    {
        char *name = strtok(buffer, " ");
        char *grade = strtok(NULL, "\n");
        strcpy(grades[num_grades].name, name);
        strcpy(grades[num_grades].grade, grade);
        num_grades++;
    }

    // Sort the grades
    qsort(grades, num_grades, sizeof(StudentGrade), byName ? compareByName : compareByGrade);

    // Write the sorted grades back to the file
    lseek(fd, 0, SEEK_SET);
    for (size_t i = 0; i < num_grades; i++) {
        dprintf(fd, "%s %s\n", grades[i].name, grades[i].grade);
    }

    // Close the file
    if (close(fd) == -1) {
        perror("Error closing file");
        exit(EXIT_FAILURE);
    }
}

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
    int fd = open("grades.txt", O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        char err_msg[256];
        strerror_r(errno, err_msg, sizeof(err_msg));
        write(STDERR_FILENO, err_msg, strlen(err_msg));
    }
    
    char buffer[1024];
    int bytes_written = snprintf(buffer, sizeof(buffer), "%s %s\n", name, grade);
    ssize_t bytes_written = write(fd, buffer, strlen(buffer));
    if (bytes_written == -1) 
    {
        perror("Write Error");
    }
    
    if (close(fd) == -1) 
    {
        perror("Close Error");
    }
}

void searchStudent(const char *name)
{
    char buffer[1024];
    ssize_t bytes_read;
    int found = 0;

    int fd = open("grades.txt", O_RDONLY);
    if (fd == -1) {
        perror("Open Error");
    }
    
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) 
    {
        char *student_name = strtok(buffer, " ");
        char *student_grade = strtok(NULL, "\n");
        if (student_name != NULL && strcmp(student_name, name) == 0) 
        {
            write(STDOUT_FILENO, buffer, bytes_read);
            found = 1;
        }
    }
    
    if (!found) 
    {
        write(STDOUT_FILENO, "Student not found.\n", 19);
    }
    
    if (close(fd) == -1) 
    {
        perror("Close Error");
    }
}


#endif // HW1_H
