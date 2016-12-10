
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "dataStructures.h"

#define WINDOWSIZE 256
#define TIMERSIZE 2048
#define NANOSLEEP 100000


int timerSize = TIMERSIZE;
int nanoSleep = NANOSLEEP;
int windowSize = WINDOWSIZE;
int pipeFd[2];
volatile int currentTimeSlot;
struct headTimer timerWheel[TIMERSIZE];



void clientSendFunction();
void * clientListenFunction();

void initProcess();
void startClientConnection(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);


// %%%%%%%%%%%%%%%%%%%%%%%    globali    %%%%%%%%%%%%%%%%%%%%%%%%%%
pthread_t listenThread, timerThread;

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

    if(pipe(pipeFd) == -1)
    {
        perror("error in pipe open");
    }

    initProcess();

    struct pipeMessage rtxN;
    memset(&rtxN, 0, sizeof(struct pipeMessage));

    for(;;)
    {
        if(read(pipeFd[0], &rtxN, sizeof(struct pipeMessage)) == -1)
        {
            perror("error on pipe read");
        }
        else
        {
            printf("ho trovato un pipeMessage con numero di seq %d\n", rtxN.seqNum);
            memset(&rtxN, 0, sizeof(struct pipeMessage));
        }
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
    //mando il primo datagramma senza connettermi
    sendACK(socketfd, &SYN, servAddr, servLen);
    sentPacket(SYN.sequenceNum, 0);
    int i;
    for(i = 0; i < WINDOWSIZE; i++)
    {
        printf("[%d]", selectiveWnd[i].value);
    }
}


void * clientListenFunction()
{
    printf("sono il listener\n\n");
    sleep(10);
    //return (EXIT_SUCCESS);

}


