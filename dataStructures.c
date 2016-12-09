//
// Created by giogge on 09/12/16.
//

/*
sender, un pacchetto deve partire, sendbase è il seqnum che mi ha comunicato il client
    FATTO-controllo che il suo seqnum sia autorizzato a partire
    FATTO-parte e aggiorno la finestra
    FATTO-aspetto l'ack e lo segno nella finestra, eventualmente facendo avanzare sendbase

receiver, ricevo un pacchetto, recvbase è il seqnum che ho comunicato al server
    controllo che sia nella mia receiver window
    FATTO-se è un nuovo pacchetto lo segno come ackato e mando l'ack
    CIRCAFATTO-se è già ackato lo scarterò, ma l'ack va mandato lo stesso
    CIRCAFATTO-se non è nell'ordine lo acko ma salvo il pacchetto in un buffer, out of order
    FATTO-se ricevo e acko la recvbase la faccio avanzare
    FATTO-se ricevo pacchetti below recvbase li scarto, se ricevo pacchetti oltre la finestra pure
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "dataStructures.h"


extern struct selectCell selectiveWnd[];


//------------------------------------------------------------------------------------------------------START CONNECTION

//-------------------------------------------------------------------------------------------------------CREATE STRUCT
struct sockaddr_in createStruct(unsigned short portN)
{
    struct sockaddr_in address;
    socklen_t serverLen = sizeof(struct sockaddr_in);

    memset((void *) &address, 0, serverLen);//reset del contenuto
    address.sin_family = AF_INET;
    address.sin_port = htons(portN);
    if(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) <= 0)
    {
        exit(EXIT_FAILURE);
    }
    return address;
}

//---------------------------------FUNZIONI DI INIZIALIZZAZIONE, SIA S CHE R --------------------

void initWindow(int dimension, struct selectCell *window)
{
    memset(window, 0, dimension*sizeof(struct selectCell));
    int i;
    for(i = 0; i < dimension; i++)
    {
        window[i].seqNum = -1;
        window[i].value = 0;
        window[i].wheelTimer = NULL;
    }
    //setta tutte le celle a 0, cioè cella vuota
    //1 sta per pacchetto spedito non ackato
    //2 sta per pacchetto spedito e ackato, credo si possa migliorare
    //devo impostare sendBase e nextseqnum, non so bene quando
}

void bindSocket(int sockfd, struct sockaddr * address , socklen_t size)
{
    if(bind(sockfd, address, size) == -1)
    {
        perror("error in bind\n");
        exit(EXIT_FAILURE);
    }
}


int createSocket()
{
    int socketfd;
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(socketfd == -1)
    {
        perror("error in socket creation\n");
        exit(EXIT_FAILURE);
    }
    return socketfd;
}