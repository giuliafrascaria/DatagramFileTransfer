CC=gcc
CFLAGS=-Wall -Wextra -pthread -DNANOSLEEP=10000 -DWINDOWSIZE=2048 -DTIMERSIZE=2048 -DLOSSPROB=0 -DBASETIMER=200 -DPULLDIR=\"/home/dandi/exp/\" -DLSDIR=\"/home/dandi/Downloads/\" -DPORT=4242 -I.

all: client server clean
	
client: client.o dataStructures.o
	$(CC) -Wall -Wextra -pthread -DNANOSLEEP=10000 -DWINDOWSIZE=2048 -DTIMERSIZE=2048 -DLOSSPROB=0 -DBASETIMER=200 -DPULLDIR=\"/home/dandi/exp/\" -DPORT=4242 -o client client.o dataStructures.o -I.

server: mainServer.o dataStructures.o server.o
	$(CC) -Wall -Wextra -pthread -DNANOSLEEP=10000 -DWINDOWSIZE=2048 -DTIMERSIZE=2048 -DLOSSPROB=0 -DBASETIMER=200 -DLSDIR=\"/home/dandi/Downloads/\" -DPORT=4242 -o server server.o mainServer.o dataStructures.o -I.

clean:
	rm *.o  
