CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS =
OBJFILES_SERVER = server.o thread_pool.o complex_matrix.o delivery.o queue.o
OBJFILES_CLIENT = client.o
TARGET_SERVER = PideShop
TARGET_CLIENT = HungryVeryMuch

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): $(OBJFILES_SERVER)
	$(CC) $(CFLAGS) -o $(TARGET_SERVER) $(OBJFILES_SERVER) $(LDFLAGS)

server.o: server.c thread_pool.h complex_matrix.h delivery.h common.h queue.h
	$(CC) -c $(CFLAGS) server.c

thread_pool.o: thread_pool.c thread_pool.h
	$(CC) -c $(CFLAGS) thread_pool.c

complex_matrix.o: complex_matrix.c complex_matrix.h
	$(CC) -c $(CFLAGS) complex_matrix.c

delivery.o: delivery.c delivery.h
	$(CC) -c $(CFLAGS) delivery.c

queue.o: queue.c queue.h
	$(CC) -c $(CFLAGS) queue.c

$(TARGET_CLIENT): $(OBJFILES_CLIENT)
	$(CC) $(CFLAGS) -o $(TARGET_CLIENT) $(OBJFILES_CLIENT) $(LDFLAGS)

client.o: client.c common.h
	$(CC) -c $(CFLAGS) client.c

clean:
	rm -f $(OBJFILES_SERVER) $(OBJFILES_CLIENT) $(TARGET_SERVER) $(TARGET_CLIENT)
	rm -f *.log *~
