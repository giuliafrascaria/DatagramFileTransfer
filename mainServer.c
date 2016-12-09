//prova del branch

//--------------------------------------LIBRERIE-------------------------

#include <sys/socket.h> //socket(), bind()
#include <stdlib.h> //exit()
#include <netinet/in.h> // htons() ???
#include <stdio.h> //perror()
#include <arpa/inet.h> //inet_pton(), htons()
#include <unistd.h>//read()
#include "dataStructures.h"
#include "server.h"



//------------------------------------PROTOTIPI---------------------------------





//-------------------------------PROGRAMMA-------------------------------

struct details client;



int main() {

    //-----------------------------------------------------------------------------------------------init main server

    int mainSocket;
    struct sockaddr_in address; //specializzazione ipv4 della struct generica sockaddr
    socklen_t slen = sizeof(address);

    //creazione della socket udp
    mainSocket = createSocket();


    //settaggio dei valori della struct
    address = createStruct(4242); //create main socket on well known port


    //il server deve anche fare un bind della socket a una porta nota
    bindSocket(mainSocket, (struct sockaddr *) &address, slen);

    //-----------------------------------------------------------------------------------------------main server loop

    //il server inizia a servire le richieste in un ciclo continuo
    for(;;)
    {

        handshake SYN;
        size_t SYNlen = sizeof(handshake);

        receiveMsg(mainSocket, &SYN, SYNlen, (struct sockaddr *) &(client.addr), &slen);
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

            printf("*----------------------------*\n un client vorrebbe connettersi\n*----------------------------*\n\n\n");

            listenFunction(mainSocket, &client, &SYN, SYNlen);

            printf("il figlio è pronto a servire il client\n");
            //exit(EXIT_SUCCESS);

        }
        //if I am the parent process, I continue waiting for connections on this port
    }
    exit(EXIT_SUCCESS);
}












