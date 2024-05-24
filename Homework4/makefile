CC = gcc
CFLAGS = -Wall
LDFLAGS = 
OBJFILES = hw4.o 
TARGET = MWCp

all: $(TARGET)

$(TARGET): $(OBJFILES) 
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES) $(LDFLAGS)

ipc.o: hw3.c
	$(CC) -c $(CFLAGS) hw4.c -lpthread

clean:
	rm -f $(OBJFILES) $(TARGET) *~
