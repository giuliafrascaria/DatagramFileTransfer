#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
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
struct headTimer timerWheel[TIMERSIZE];
struct details details;

pthread_t timerThread;
pthread_t senderThread;

void listenCycle();
int waitForAck(int socketFD, struct sockaddr_in * clientAddr);
void terminateConnection(int socketFD, struct sockaddr_in * clientAddr, socklen_t socklen, struct details *cl );
void sendSYNACK(int privateSocket, socklen_t socklen , struct details * cl);

void listenFunction(int socketfd, struct details * details, handshake * message)
{
    initWindow();

    char buffer[100];
    printf("richiesta dal client %s\n\n\n", inet_ntop(AF_INET, &((details->addr).sin_addr), buffer, 100));

    createThread(&timerThread, timerFunction, NULL);
    createThread(&senderThread, sendFunction, NULL);

    printf("finita la creazione dei thread\n");

    startServerConnection(details, socketfd, message);

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

    details.servSeq = (message->sequenceNum);

    //mando il datagramma ancora senza connettermi
    sendSYNACK(privateSocket, socklen, cl);
    terminateConnection(privateSocket, &(cl->addr), socklen, cl);

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
        //FINE DELLA CONNESSIONE PARTE
    }
    else //se ritorna 0 devo ritrasmettere
    {
        sendSYNACK(socketFD, socklen , cl);
        terminateConnection(socketFD, clientAddr, socklen, cl);
    }
}

int waitForAck(int socketFD, struct sockaddr_in * clientAddr)
{
    if (fcntl(socketFD, F_SETFL, O_NONBLOCK) == -1) {
        perror("error in fcntl");
    }
    socklen_t slen = sizeof(struct sockaddr_in);
    handshake ACK;
    int i = 0;
    while (i == 0)
    {
        i = checkSocketAck(clientAddr, slen, socketFD, &ACK);
        if (i == -1) {
            perror("error in socket read");
            return -1;
        }
        if (i == 1) {
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
    SYN_ACK.ack = details.servSeq;
    sendACK(privateSocket, &SYN_ACK, &(cl->addr), socklen);
    printf("SYNACK inviato, numero di sequenza : %d\n", SYN_ACK.sequenceNum);

}