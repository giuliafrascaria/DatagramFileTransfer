#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include "server.h"

#define WINDOWSIZE 256
#define TIMERSIZE 2048
#define NANOSLEEP 100000


int timerSize = TIMERSIZE;
int nanoSleep = NANOSLEEP;

volatile int currentTimeSlot;

struct selectCell selectiveWnd[WINDOWSIZE];
struct headTimer timerWheel[TIMERSIZE];

pthread_t timerThread;
pthread_t senderThread;



void listenFunction(int socketfd, struct details * details, handshake * message, ssize_t messageSize)
{
    initWindow(WINDOWSIZE, selectiveWnd);

    if(messageSize != sizeof(handshake))
    {
        perror("wrong connection start\n");
        exit(EXIT_FAILURE);

    }

    char buffer[100];
    printf("richiesta dal client %s\n\n\n", inet_ntop(AF_INET, &((details->addr).sin_addr), buffer, 100));

//    createThread(&timerThread, timerFunction, NULL);
//    createThread(&senderThread, sendFunction, NULL);
    if(pthread_create(&timerThread, NULL, (void *) timerFunction, NULL) != 0){
        perror("error on pthred_create");
    }

    if(pthread_create(&senderThread, NULL, (void *) sendFunction, NULL) != 0){
        perror("error on pthred_create");
    }
    //startServerConnection(details, socketfd, message);

}

void * sendFunction()
{
    printf("sono il sender\n");
}
