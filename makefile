CC=gcc
CFLAGS=-Wall -Wextra -pthread -DNANOSLEEP=10000 -DWINDOWSIZE=2048 -DTIMERSIZE=2048 -DLOSSPROB=100 -DBASETIMER=200 -I.

all: client server clean
	
client: client.o dataStructures.o
	$(CC) -Wall -Wextra -pthread -DNANOSLEEP=10000 -DWINDOWSIZE=2048 -DTIMERSIZE=2048 -DLOSSPROB=100 -DBASETIMER=200 -o client client.o dataStructures.o -I.

server: mainServer.o dataStructures.o server.o
	$(CC) -Wall -Wextra -pthread -DNANOSLEEP=10000 -DWINDOWSIZE=2048 -DTIMERSIZE=2048 -DLOSSPROB=100 -DBASETIMER=200 -o server server.o mainServer.o dataStructures.o -I.

clean:
	rm *.o  
