CC=gcc
CFLAGS=-Wall -Wextra -pthread -I.

all: client server clean
	
client: client.o dataStructures.o
	$(CC) -Wall -Wextra -pthread -o client client.o dataStructures.o -I.

server: mainServer.o dataStructures.o server.o
	$(CC) -Wall -Wextra -pthread -o server server.o mainServer.o dataStructures.o -I.

clean:
	rm *.o  
