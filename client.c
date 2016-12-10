
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
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
int checkPipe(struct pipeMessage *rtxN);


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

    if (fcntl(pipeFd[0], F_SETFL, O_NONBLOCK) == -1)
    {
        perror("error in fcntl");
    }

    initProcess();

    struct pipeMessage rtxN;
    memset(&rtxN, 0, sizeof(struct pipeMessage));


    for(;;)
    {
        if(checkPipe(&rtxN))
        {
            printf("ho trovato un messaggio in pipe\n\n");
        }
    }
}

int checkPipe(struct pipeMessage *rtxN)
{
    if(read(pipeFd[0], rtxN, sizeof(struct pipeMessage)) == -1)
    {
        if(errno != EAGAIN)
        {
            perror("error in pipe read");
            return -1;
        }
        else
        {
            return 0;
        }

    }
    else
    {
        printf("\n\nho trovato un rtxN\n\n");
        memset(rtxN, 0, sizeof(struct pipeMessage));
        return 1;
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


