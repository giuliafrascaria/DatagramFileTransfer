//
// Created by giogge on 09/12/16.
//


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "dataStructures.h"


extern struct selectCell selectiveWnd[];


//------------------------------------------------------------------------------------------------------START CONNECTION

//---------------------------------------------------------------------------------------------------------CREATE SOCKET

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

//------------------------------------------------------------------------------------------------------SELECTIVE REPEAT

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

void sentPacket(pthread_mutex_t *mtxARCVD , int packetN, struct details * details,
                struct timer * packetTimer, volatile struct headTimer *wheel,
                int slot, int offset, int retransmission)
{
    if(retransmission == 0)
    {
        if (pthread_mutex_lock(mtxARCVD) != 0)
        {
            perror("error in pthread_mutex_lock");
        }
        (selectiveWnd[packetN % (details)->windowDimension]).value = 1;
        //((details)->selectiveWnd)[packetN % (details)->windowDimension].packetTimer.seqNum = packetN;

        if (pthread_mutex_unlock(mtxARCVD) != 0) {
            perror("error in pthread_mutex_unlock");
        }
    }


}


void ackSentPacket(pthread_mutex_t * mtxARCVD, int ackN, int currentSlot, struct details *details)
{

    //printf("current  SB = %i, value SB = %i\n", (*details)->sendBase % (*details)->windowDimension, ((*details)->selectiveWnd)[((*details)->sendBase % (*details)->windowDimension)].value );
    //printf("ackN value = %i\n\n\n", ackN);
    //int i;

    if (selectiveWnd[ackN % (details)->windowDimension].value != 0 && (selectiveWnd)[ackN % (details)->windowDimension].value != 2) {

        /*NON COMMENTARE*/
        int i;
        printf("\n |");
        for (i = 0; i < details->windowDimension; i++)
        {
            if (i == ackN % (details)->windowDimension)
            {
                printf(" (%d) |", (selectiveWnd)[i].value);
            }
            else {
                printf(" %d |", (selectiveWnd)[i].value);
            }
        }
        printf("\n");




        if(pthread_mutex_lock(mtxARCVD)!= 0)
        {
            perror("error in mutex lock");
        }
        (selectiveWnd)[ackN % (details)->windowDimension].value = 2;


        while ((selectiveWnd)[((details)->sendBase % (details)->windowDimension)].value == 2) {

            (selectiveWnd)[((details)->sendBase % (details)->windowDimension)].value = 0;

            (details)->sendBase = (details)->sendBase + 1;

        }
        if(pthread_mutex_unlock(mtxARCVD)!=0)
        {
            perror("error in mutex unlock");
        }

    }

        //ack di paccheto mai ricevuto
    else if (selectiveWnd[ackN % (details)->windowDimension].value == 0) {
        if(((selectiveWnd)[ackN % (details)->windowDimension].wheelTimer) != NULL)
        {
            printf("stoppo il pacchetto ricevuto come duplicato\n");

        }
        else
            printf("mi hai ackato qualcosa che non ho mai mandato : %d\n", ackN );

    }

}



