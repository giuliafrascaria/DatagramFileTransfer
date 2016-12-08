//prova del branch

//--------------------------------------LIBRERIE-------------------------

#include <sys/socket.h> //socket(), bind()
#include <stdlib.h> //exit()
#include <netinet/in.h> // htons() ???
#include <stdio.h> //perror()
#include <arpa/inet.h> //inet_pton(), htons()
#include <unistd.h>//read()
#include <signal.h>
#include "dataStructures.h"
//#include "serverThreadFunctions.h"

//------------------------------------PROTOTIPI---------------------------------

void bindSocket(int sockfd, struct sockaddr * address , socklen_t size);
int createSocket();
void startServerConnection(struct details * cl, int socketfd, handshake * message);
void serveClient(int socketfd, struct clientDetails * address, handshake * message, ssize_t messageSize);
struct sockaddr_in createStruct(unsigned short portN);

//-------------------------------PROGRAMMA-------------------------------

struct details client;

int main()
{

    FILE * fdLog = fopen("logs/mainServerLog.txt", "w+");
    if(fdLog == NULL)
    {
        perror("error in log open");
    }

    int mainSocket;
    struct sockaddr_in address; //specializzazione ipv4 della struct generica sockaddr

    //creazione della socket udp
    mainSocket = createSocket();
    fprintf(fdLog,"creata la socket\n");

    //settaggio dei valori della struct
    address = createStruct(4242); //create main socket on well known port
    fprintf(fdLog, "ho creato la struct\n");

    //il server deve anche fare un bind della socket a una porta nota
    bindSocket(mainSocket, (struct sockaddr *) &address, sizeof(struct sockaddr_in));
    fprintf(fdLog,"bind della socket\n");

    //il server inizia a servire le richieste in un ciclo continuo
    for(;;)
    {

        handshake SYN;
        size_t SYNlen = sizeof(handshake);
        ssize_t msgLen = recvfrom(mainSocket, (char *) &SYN, SYNlen, 0, (struct sockaddr *) &(client.addr), &(client.Size));
        if(msgLen == -1)
        {
            perror("error in recvfrom");
        }

        fprintf(fdLog,"un nuovo client ha richiesto connessione, creo il processo server dedicato\n");
        //arriva un messaggio e salvo i dati del client nella struct

        //fork to allow child process to serve the client
        pid_t processPid;
        processPid = fork();
        if(processPid == -1)
        {
            perror("error in fork\n");
            //exit(EXIT_FAILURE);
        }

        if(processPid == 0)//child process
        { //
            signal(SIGALRM, AlarmWakeUp);
            printf("*----------------------------*\n un client vorrebbe connettersi\n*----------------------------*\n\n\n");
            //listenFunction(mainSocket, &client, &SYN, SYNlen);
            exit(EXIT_SUCCESS);

        }
        //if I am the parent process, I continue waiting for connections on this port
    }
    exit(EXIT_SUCCESS);
}











