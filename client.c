
#include <stdio.h>
#include <stdlib.h>
#include "dataStructures.h"


#define WINDOWSIZE 256

void startClientConnection(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);
void clientSendFunction();

//struct details sendServer;
// %%%%%%%%%%%%%%%%%%%%%%%    globali    %%%%%%%%%%%%%%%%%%%%%%%%%%

struct selectCell selectiveWnd[WINDOWSIZE];

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

//MAIN
//------------------------------------------------------------------------------------------------------------------MAIN
int main()
{

    printf("connection is starting... \n\n\n");
    clientSendFunction();
    exit(EXIT_SUCCESS);
}


void clientSendFunction()
{

    initWindow(WINDOWSIZE, selectiveWnd);

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

