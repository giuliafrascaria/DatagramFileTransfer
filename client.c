
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include "dataStructures.h"

#define WINDOWSIZE 256
#define TIMERSIZE 2048
#define NANOSLEEP 500000


int timerSize = TIMERSIZE;
int nanoSleep = NANOSLEEP;
int windowSize = WINDOWSIZE;
int sendBase;
int pipeFd[2];
volatile int currentTimeSlot;
struct headTimer timerWheel[TIMERSIZE] = {NULL};


void clientSendFunction();
void * clientListenFunction();
void sendSYN(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);
void sendSYN2(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);
int waitForSYNACK(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);
void send_ACK(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, int synackSN);


void initProcess();
void startClientConnection(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);



// %%%%%%%%%%%%%%%%%%%%%%%    globali    %%%%%%%%%%%%%%%%%%%%%%%%%%

struct details details;
pthread_t listenThread, timerThread;
struct selectCell selectiveWnd[WINDOWSIZE];

pthread_mutex_t condMTX = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t secondConnectionCond = PTHREAD_COND_INITIALIZER;

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

    initPipe();

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

    //----------------------------------------------------


    senderServerAddress = createStruct(4242); //create struct with server port
    socketfd = createSocket();

    if (fcntl(socketfd, F_SETFL, O_NONBLOCK) == -1)
    {
        perror("error in fcntl");
    }
    //printf("ho settato la socket a non bloccante\n");

    //printf("starting handshake procedure\n\n");
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

        //salvo i dettagli di connessione in details
        details.addr = *servAddr;
        details.Size = servLen;
        details.sockfd = socketfd;

        //segnalo al listener
        sendSignalThread(&condMTX, &secondConnectionCond);

    }
    else //se ritorna -1 devo ritrasmettere
    {
        initWindow();
        startClientConnection(servAddr, servLen, socketfd);
    }

}

void * clientListenFunction()
{
    printf("listener thread attivato\n\n");


    details.Size2 = sizeof(struct sockaddr_in);
    details.sockfd2 = createSocket();

    if(pthread_cond_wait(&secondConnectionCond, &condMTX) != 0)
    {
        perror("error in cond wait");
    }

    printf("sono dopo la cond wait\n\n");

    sendSYN2(&(details.addr), details.Size, details.sockfd2);

    waitForSYNACK(&(details.addr2), details.Size2, details.sockfd2);

    send_ACK(&(details.addr2), details.Size2, details.sockfd2, details.remoteSeq);


    sleep(10);
    //return (EXIT_SUCCESS);
    for(;;)
    {

    }
}

void sendSYN(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd)
{
    handshake SYN;

    srandom((unsigned int)getpid());
    SYN.sequenceNum = (int) random() % 4096;

    sendBase = SYN.sequenceNum;

    // il prossimo seqnum utile
    details.remoteSeq = SYN.sequenceNum;

    sendACK(socketfd, &SYN, servAddr, servLen);
    sentPacket(SYN.sequenceNum, 0);


    printf("ho inviato il SYN. numero di sequenza : %d\n", SYN.sequenceNum);
}

void sendSYN2(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd)
{
    handshake SYN;

    SYN.sequenceNum = details.mySeq;
    SYN.ack = details.remoteSeq;

    sendACK(socketfd, &SYN, servAddr, servLen);
    sentPacket(SYN.sequenceNum, 0);


    printf("ho inviato il SYN2. numero di sequenza : %d e ack per il server %d\n", SYN.sequenceNum, SYN.ack);
}

int waitForSYNACK(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd)
{
    handshake SYNACK;
    int sockResult;
    struct pipeMessage rtxN;
    for(;;)
    {
        if(checkPipe(&rtxN))
        {
            printf("devo ritrasmettere\n");
            return 0;
        }
        sockResult = checkSocketAck(servAddr, servLen, socketfd, &SYNACK);
        if(sockResult == -1)
        {
            perror("error in socket read");
            return -1;
        }
        if(sockResult == 1)
        {
            printf("SYNACK ricevuto. numero di sequenza : %d\n", SYNACK.sequenceNum);
            ackSentPacket(SYNACK.ack);
            details.remoteSeq = SYNACK.sequenceNum;//---------------------------------------------serve al syn2
            //--------------------------------------------INIT GLOBAL DETAILS
            return SYNACK.sequenceNum;
        }
    }
}


void send_ACK(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, int synackSN)
{
    handshake ACK;
    ACK.ack = synackSN;
    ACK.sequenceNum = details.remoteSeq;
    ACK.windowsize = windowSize;

    sendACK(socketfd, &ACK, servAddr, servLen);
    //sentPacket(ACK.sequenceNum, 0);
    printf("ACK finale inviato. Numero di sequenza : %d\n", ACK.sequenceNum);
}