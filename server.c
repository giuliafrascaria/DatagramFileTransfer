#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include "server.h"

#define WINDOWSIZE 256
#define TIMERSIZE 2048
#define NANOSLEEP 500000


int timerSize = TIMERSIZE;
int nanoSleep = NANOSLEEP;
int windowSize = WINDOWSIZE;
int sendBase;
int pipeFd[2];

volatile int currentTimeSlot;

struct selectCell selectiveWnd[WINDOWSIZE];
struct headTimer timerWheel[TIMERSIZE] = {NULL};
struct details details;

pthread_t timerThread;
pthread_t senderThread;

pthread_mutex_t condMTX = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t secondConnectionCond = PTHREAD_COND_INITIALIZER;

void listenCycle();
int waitForAck(int socketFD, struct sockaddr_in * clientAddr);
void terminateConnection(int socketFD, struct sockaddr_in * clientAddr, socklen_t socklen, struct details *cl );
void sendSYNACK(int privateSocket, socklen_t socklen , struct details * cl);
int waitForAck2(int socketFD, struct sockaddr_in * clientAddr);
void finishHandshake();

void listenFunction(int socketfd, struct details * details, handshake * message)
{
    initWindow();

    char buffer[100];
    printf("richiesta dal client %s\n\n\n", inet_ntop(AF_INET, &((details->addr).sin_addr), buffer, 100));

    initPipe();

    createThread(&timerThread, timerFunction, NULL);
    createThread(&senderThread, sendFunction, NULL);

    printf("finita la creazione dei thread\n");

    startServerConnection(details, socketfd, message);

    listenCycle();
}

void * sendFunction()
{

    printf("sender thread attivato\n\n");

    if(pthread_cond_wait(&secondConnectionCond, &condMTX) != 0)
    {
        perror("error in cond wait");
    }

    printf("sono dopo la cond wait\n\n");

    startSecondConnection(&details, details.sockfd2);

    finishHandshake();


    //---------------------------------------------------------------------scheletro di sincronizzazione listener sender
    printf("mi metto in cond wait\n");

    if(pthread_cond_wait(&secondConnectionCond, &condMTX) != 0)
    {
        perror("error in cond wait");
    }

    printf("sono dopo la seconda cond wait\n\n");

    //-----------------------------------------------------------------------------------------------------------------
    for(;;)
    {

    }

}

void finishHandshake()
{
    //aspetto ultimo ack
    int res = waitForAck2(details.sockfd2, &(details.addr2));
    if(res == 0)
    {
        //ritrasmetto
        sendSYNACK2(details.sockfd2, details.Size2, &details);
        finishHandshake();
    }
    else
    {
        //ho finito la connessione, aspetto che mi svegli il listener
        printf("fine, sono pronto\n\n");
    }
}

void listenCycle()
{
    printf("inizio il ciclo di ascolto\n");

    //---------------------------------------------------------------------------------scheletro sincronizzazione thread
    sleep(10);
    printf("provo a svegliare il sender\n");
    sendSignalThread(&condMTX, &secondConnectionCond);
    datagram packet;
    //------------------------------------------------------------------------------------------------------------------
    for(;;)
    {
        memset(&packet, 0, sizeof(datagram));
        int res = 0;
        while(!res)
        {
            res = checkSocketDatagram(&(details.addr), details.Size, details.sockfd, &packet);
            if(res == -1)
            {
                perror("error in socket read");
            }
            if(res == 0)
            {
                usleep(1000);
            }
        }


        //arrivo qui quando ho ricevuto cose, chiamo una funzione tipo controllaComando() e sveglia il sender e il timer
    }
}

void startServerConnection(struct details * cl, int socketfd, handshake * message)
{
    //chiudo la socket pubblica nel processo figlio
    closeFile(socketfd);

    //apro la socket dedicata al client su una porta casuale
    int privateSocket;
    privateSocket = createSocket();
    socklen_t socklen = sizeof(struct sockaddr_in);

    //collego una struct legata a una porta effimera, dedicata al client
    struct sockaddr_in serverAddress;
    //mi metto su una porta effimera, indicandola con sin_port = 0
    serverAddress = createStruct(0); //create struct with ephemeral port
    printf("ho creato la struct dedicata\n");

    //per il client rimango in ascolto su questa socket
    bindSocket(privateSocket, (struct sockaddr *) &serverAddress, socklen);

    details.remoteSeq = (message->sequenceNum);

    //mando il datagramma ancora senza connettermi
    sendSYNACK(privateSocket, socklen, cl);
    terminateConnection(privateSocket, &(cl->addr), socklen, cl);

}

void startSecondConnection(struct details * cl, int socketfd)
{
    //chiudo la socket pubblica nel processo figlio
    closeFile(socketfd);

    //apro la socket dedicata al client su una porta casuale
    details.sockfd2 = createSocket();
    details.Size2 = sizeof(struct sockaddr_in);

    //collego una struct legata a una porta effimera, dedicata al client
    struct sockaddr_in serverAddress;
    //mi metto su una porta effimera, indicandola con sin_port = 0
    serverAddress = createStruct(0); //create struct with ephemeral port
    printf("ho creato la seconda struct dedicata\n");

    details.addr2 = serverAddress;

    //per il client rimango in ascolto su questa socket
    bindSocket(details.sockfd2, (struct sockaddr *) &(details.addr2), details.Size2);

    //mando il datagramma ancora senza connettermi
    sendSYNACK2(details.sockfd2, details.Size2, cl);
    //terminateConnection(privateSocket, &(cl->addr), socklen, cl);

}

void terminateConnection(int socketFD, struct sockaddr_in * clientAddr, socklen_t socklen, struct details *cl )
{
    int rcvSequence = waitForAck(socketFD, clientAddr);
    if(rcvSequence == -1)
    {
        perror("error in connection");
    }
    else if(rcvSequence > 0)
    {
        printf("ACK ricevuto con numero di sequenza : %d. fine connessione parte 1\n", rcvSequence);
        //avvio il thread listener per connettersi su una nuova socket
        //cond signal e il listener mi manda un secondo SYNACK, chiudendo socket eccetera

        sendSignalThread(&condMTX, &secondConnectionCond);

        //in teoria ora posso connettere la socket
    }
    else //se ritorna 0 devo ritrasmettere
    {
        sendSYNACK(socketFD, socklen , cl);
        terminateConnection(socketFD, clientAddr, socklen, cl);
    }
}

int waitForAck(int socketFD, struct sockaddr_in * clientAddr)
{
    if (fcntl(socketFD, F_SETFL, O_NONBLOCK) == -1)
    {
        perror("error in fcntl");
    }
    socklen_t slen = sizeof(struct sockaddr_in);
    handshake ACK;
    struct pipeMessage rtxN;
    int sockResult;
    for(;;)
    {
        if(checkPipe(&rtxN))
        {
            printf("devo ritrasmettere\n");
            return 0;
        }
        sockResult = checkSocketAck(clientAddr, slen, socketFD, &ACK);
        if (sockResult == -1)
        {
            perror("error in socket read");
            return -1;
        }
        if (sockResult == 1)
        {
            details.addr = *clientAddr;
            details.Size = slen;
            details.sockfd = socketFD;
            details.remoteSeq = ACK.sequenceNum;

            ackSentPacket(ACK.ack);
            //--------------------------------------------INIT GLOBAL DETAILS
            return ACK.sequenceNum;
        }
    }
}

int waitForAck2(int socketFD, struct sockaddr_in * clientAddr)
{
    if (fcntl(socketFD, F_SETFL, O_NONBLOCK) == -1)
    {
        perror("error in fcntl");
    }
    socklen_t slen = sizeof(struct sockaddr_in);
    handshake ACK;
    struct pipeMessage rtxN;
    int sockResult;
    for(;;)
    {
        if(checkPipe(&rtxN))
        {
            printf("devo ritrasmettere\n");
            return 0;
        }
        sockResult = checkSocketAck(clientAddr, slen, socketFD, &ACK);
        if (sockResult == -1)
        {
            perror("error in socket read");

        }
        if (sockResult == 1)
        {
            ackSentPacket(ACK.ack);
            //--------------------------------------------INIT GLOBAL DETAILS
            return ACK.sequenceNum;
        }
    }
}

void sendSYNACK(int privateSocket, socklen_t socklen , struct details * cl)
{
    handshake SYN_ACK;
    srandom((unsigned int)getpid());
    SYN_ACK.sequenceNum = rand() % 4096;

    sendBase = SYN_ACK.sequenceNum;

    SYN_ACK.ack = details.remoteSeq;
    sendACK(privateSocket, &SYN_ACK, &(cl->addr), socklen);

    sentPacket(SYN_ACK.sequenceNum, 0);
    printf("SYNACK inviato, numero di sequenza : %d\n", SYN_ACK.sequenceNum);

}

void sendSYNACK2(int privateSocket, socklen_t socklen , struct details * cl)
{
    handshake SYN_ACK;

    SYN_ACK.sequenceNum = details.mySeq;

    SYN_ACK.ack = details.remoteSeq;
    sendACK(privateSocket, &SYN_ACK, &(cl->addr), socklen);

    sentPacket(SYN_ACK.sequenceNum, 0);
    printf("SYNACK inviato, numero di sequenza : %d\n", SYN_ACK.sequenceNum);

}