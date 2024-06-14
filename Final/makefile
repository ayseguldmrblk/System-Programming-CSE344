CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS =
OBJFILES_SERVER = server.o thread_pool.o
OBJFILES_CLIENT = client.o
TARGET_SERVER = PideShop
TARGET_CLIENT = HungryVeryMuch

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): $(OBJFILES_SERVER)
	$(CC) $(CFLAGS) -o $(TARGET_SERVER) $(OBJFILES_SERVER) $(LDFLAGS)

server.o: server.c thread_pool.h
	$(CC) -c $(CFLAGS) server.c

thread_pool.o: thread_pool.c thread_pool.h
	$(CC) -c $(CFLAGS) thread_pool.c

$(TARGET_CLIENT): $(OBJFILES_CLIENT)
	$(CC) $(CFLAGS) -o $(TARGET_CLIENT) $(OBJFILES_CLIENT) $(LDFLAGS)

client.o: client.c
	$(CC) -c $(CFLAGS) client.c

clean:
	rm -f $(OBJFILES_SERVER) $(OBJFILES_CLIENT) $(TARGET_SERVER) $(TARGET_CLIENT)
