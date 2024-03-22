CC = gcc
CFLAGS = -Wall
LDFLAGS = 
OBJFILES = hw1.o 
TARGET = gtuStudentGrades

all: $(TARGET)

$(TARGET): $(OBJFILES) hw1.h
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES) $(LDFLAGS)

hw1.o: hw1.c
	$(CC) -c $(CFLAGS) hw1.c

clean:
	rm -f $(OBJFILES) $(TARGET) *~
	rm -f *.txt
