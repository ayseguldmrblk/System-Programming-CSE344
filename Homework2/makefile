CC = gcc
CFLAGS = -Wall
LDFLAGS = 
OBJFILES = hw2.o 
TARGET = ipc

all: $(TARGET)

$(TARGET): $(OBJFILES) 
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES) $(LDFLAGS)

ipc.o: hw2.c
	$(CC) -c $(CFLAGS) hw2.c

clean:
	rm -f $(OBJFILES) $(TARGET) *~
	rm -f *.txt
	rm -f FIFO1
	rm -f FIFO2
