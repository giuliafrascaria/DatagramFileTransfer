#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include "server.h"

#define WINDOWSIZE 256
#define TIMERSIZE 2048
#define NANOSLEEP 100000


int timerSize = TIMERSIZE;
int nanoSleep = NANOSLEEP;
int windowSize = WINDOWSIZE;
int pipeFd[2];

volatile int currentTimeSlot;

struct selectCell selectiveWnd[WINDOWSIZE];
struct headTimer timerWheel[TIMERSIZE];

pthread_t timerThread;
pthread_t senderThread;

void listenCycle();


void listenFunction(int socketfd, struct details * details, handshake * message)
{
    initWindow();

    char buffer[100];
    printf("richiesta dal client %s\n\n\n", inet_ntop(AF_INET, &((details->addr).sin_addr), buffer, 100));

    createThread(&timerThread, timerFunction, NULL);
    createThread(&senderThread, sendFunction, NULL);

    //startServerConnection(details, socketfd, message);

    printf("finita la creazione dei thread\n");

    listenCycle();

}

void * sendFunction()
{
    printf("sono il sender\n");
}

void listenCycle()
{
    printf("inizio il ciclo di ascolto\n");
    for(;;)
    {

    }
}