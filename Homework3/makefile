CC = gcc
CFLAGS = -Wall
LDFLAGS = 
OBJFILES = hw3.o 
TARGET = parking_lot_simulation

all: $(TARGET)

$(TARGET): $(OBJFILES) 
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES) $(LDFLAGS)

ipc.o: hw3.c
	$(CC) -c $(CFLAGS) hw3.c -lpthread

clean:
	rm -f $(OBJFILES) $(TARGET) *~
