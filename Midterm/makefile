CC = gcc
CFLAGS = -Wall
LDFLAGS =
OBJFILES_SERVER = server.o
OBJFILES_CLIENT = client.o
TARGET_SERVER = neHosServer
TARGET_CLIENT = neHosClient
FIFO_FILES = /tmp/server_fifo_* /tmp/client_fifo.*

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): $(OBJFILES_SERVER)
	$(CC) $(CFLAGS) -o $(TARGET_SERVER) $(OBJFILES_SERVER) $(LDFLAGS)

server.o: server.c concurrent_file_access_system.h queue.h
	$(CC) -c $(CFLAGS) server.c

$(TARGET_CLIENT): $(OBJFILES_CLIENT)
	$(CC) $(CFLAGS) -o $(TARGET_CLIENT) $(OBJFILES_CLIENT) $(LDFLAGS)

client.o: client.c concurrent_file_access_system.h
	$(CC) -c $(CFLAGS) client.c

clean:
	rm -f $(OBJFILES_SERVER) $(OBJFILES_CLIENT) $(TARGET_SERVER) $(TARGET_CLIENT)
	rm -f $(FIFO_FILES)
