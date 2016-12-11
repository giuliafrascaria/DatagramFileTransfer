
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
void sendSYN(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);
int waitForSYNACK(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);
void send_ACK(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, int synackSN);


void initProcess();
void startClientConnection(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);
int checkPipe(struct pipeMessage *rtxN);


// %%%%%%%%%%%%%%%%%%%%%%%    globali    %%%%%%%%%%%%%%%%%%%%%%%%%%

struct details details;
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

    if (fcntl(socketfd, F_SETFL, O_NONBLOCK) == -1)
    {
        perror("error in fcntl");
    }
    printf("ho settato la socket a non bloccante\n");

    printf("starting handshake procedure\n\n");
    startClientConnection( &senderServerAddress, serverLen, socketfd);
}

void startClientConnection(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd)
{
    sendSYN(servAddr, servLen, socketfd);
    int rcvSequence = waitForSYNACK(servAddr, servLen, socketfd);
    if(rcvSequence == -1)
    {
        perror("error in connection");
    }
    else if(rcvSequence > 0)//ho ricevuto il SYNACK
    {
        send_ACK(servAddr, servLen, socketfd, rcvSequence);
    }
    else //se ritorna -1 devo ritrasmettere
    {
        startClientConnection(servAddr, servLen, socketfd);
    }

}


void * clientListenFunction()
{
    printf("sono il listener\n\n");
    sleep(10);
    //return (EXIT_SUCCESS);

}

void sendSYN(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd)
{
    handshake SYN;
    SYN.sequenceNum = rand() % 4096;
    details.servSeq = SYN.sequenceNum;
    sendACK(socketfd, &SYN, servAddr, servLen);
    sentPacket(SYN.sequenceNum, 0);
}

int waitForSYNACK(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd)
{
    handshake SYNACK;
    struct pipeMessage rtxN;
    for(;;){
        if(checkPipe(&rtxN))
        {
            printf("devo ritrasmettere\n");
            return 0;
        }
        if((recvfrom(socketfd, (char *) &SYNACK, sizeof(handshake), 0, (struct sockaddr *) servAddr, &servLen) == -1) && (errno != EAGAIN))
        {
            perror("error in socket read");
            return -1;
        }
        if((recvfrom(socketfd, (char *) &SYNACK, sizeof(handshake), 0, (struct sockaddr *) servAddr, &servLen) > 0))
        {
            //receiveACK(socketfd, &SYNACK, (struct sockaddr *) servAddr, &servLen)
            printf("ho ricevuto un syn ack %d\n\n", SYNACK.sequenceNum);
            ackSentPacket(SYNACK.ack, 0);

            //--------------------------------------------INIT GLOBAL DETAILS
            return SYNACK.sequenceNum;
        }
    }
}

void send_ACK(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, int synackSN)
{
    handshake ACK;
    ACK.ack = synackSN + 1;
    ACK.sequenceNum = details.servSeq + 1;
    ACK.windowsize = windowSize;

    sendACK(socketfd, &ACK, servAddr, servLen);
    sentPacket(ACK.sequenceNum, 0);
}



























