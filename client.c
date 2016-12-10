
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dataStructures.h"

#define WINDOWSIZE 256
#define TIMERSIZE 2048
#define NANOSLEEP 100000


int timerSize = TIMERSIZE;
int nanoSleep = NANOSLEEP;
int windowSize = WINDOWSIZE;
volatile int currentTimeSlot;
struct headTimer timerWheel[TIMERSIZE];


pthread_t listenThread;
pthread_t timerThread;

void clientSendFunction();
void * clientListenFunction();

void initProcess();
void startClientConnection(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);


// %%%%%%%%%%%%%%%%%%%%%%%    globali    %%%%%%%%%%%%%%%%%%%%%%%%%%

struct selectCell selectiveWnd[WINDOWSIZE];

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

//------------------------------------------------------------------------------------------------------------------MAIN
int main()
{

    printf("connection is starting... \n\n\n");
    clientSendFunction();
    exit(EXIT_SUCCESS);
}


void clientSendFunction()
{
    initProcess();

    for(;;)
    {

    }
}

void initProcess()
{
    initWindow();

    // %%%%%%%%%%%%%%%%    thread       %%%%%%%%%%%%%%%%%

    createThread(&listenThread, clientListenFunction, NULL);

    createThread(&timerThread, timerFunction, NULL);

    // %%%%%%%%%%%%%%%%    variabili    %%%%%%%%%%%%%%%%%

    int socketfd;
    struct sockaddr_in senderServerAddress;
    socklen_t serverLen = sizeof(struct sockaddr_in);

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


    senderServerAddress = createStruct(4242); //create struct with server port
    socketfd = createSocket();
    printf("starting handshake procedure\n\n");
    startClientConnection( &senderServerAddress, serverLen, socketfd);
}

void startClientConnection(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd)
{

    handshake SYN;
    SYN.sequenceNum = rand() % 4096;

    size_t SYNsize = sizeof(handshake);

    //mando il primo datagramma senza connettermi
    ssize_t sentData;
    sentData = sendto(socketfd, (char *) &SYN, SYNsize, 0, (struct sockaddr* ) servAddr, servLen);

    //struct timer * packetTimer = malloc(sizeof(struct timer));

    sentPacket(NULL, SYN.sequenceNum, WINDOWSIZE, NULL, 0);

    int i;
    for(i = 0; i < WINDOWSIZE; i++)
    {
        printf("[%d]", selectiveWnd[i].value);
    }

    if(sentData == -1)
    {
        perror("error in sending data\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("mandato il primo messaggio\n");
    }

}


void * clientListenFunction()
{
    printf("sono il listener\n\n");
    sleep(10);
    //return (EXIT_SUCCESS);

}