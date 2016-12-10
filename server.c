#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <zconf.h>
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
    if(close(socketfd) == -1)
    {
        perror("error in public socket close\n");
        exit(EXIT_FAILURE);
    }

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

    handshake SYN_ACK;
    handshake * SYN;
    SYN = message;
    srandom((unsigned int)getpid());
    SYN_ACK.sequenceNum = rand() % 4096;
    SYN_ACK.ack = SYN->sequenceNum + 1;

    size_t SYNACKsize = sizeof(handshake);

    //mando il datagramma ancora senza connettermi, che poi ha senso la connessione per il server??
    sendACK(privateSocket, &SYN_ACK, &(cl->addr), socklen);

}