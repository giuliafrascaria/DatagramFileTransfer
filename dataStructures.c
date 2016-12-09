//
// Created by giogge on 09/12/16.
//


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include "dataStructures.h"


extern struct selectCell selectiveWnd[];
extern struct headTimer timerWheel[];
extern int  timerSize, nanoSleep;
extern volatile int currentTimeSlot;

//------------------------------------------------------------------------------------------------------START CONNECTION

//---------------------------------------------------------------------------------------------------------CREATE SOCKET

struct sockaddr_in createStruct(unsigned short portN)
{
    struct sockaddr_in address;
    socklen_t serverLen = sizeof(struct sockaddr_in);

    memset((void *) &address, 0, serverLen);//reset del contenuto

    address.sin_family = AF_INET;
    address.sin_port = htons(portN);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

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
    socketfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(socketfd == -1)
    {
        perror("error in socket creation\n");
        exit(EXIT_FAILURE);
    }
    return socketfd;
}

//------------------------------------------------------------------------------------------------------SELECTIVE REPEAT

void initWindow(int dimension, struct selectCell * window)
{
    memset(window, 0, dimension * sizeof(struct selectCell));
    int i;
    for(i = 0; i < dimension; i++)
    {
        window[i].seqNum = -1;
        window[i].value = 0;
        window[i].wheelTimer = NULL;
    }

    printf("inizializzazione terminata\n");
    //setta tutte le celle a 0, cioè cella vuota
    //1 sta per pacchetto spedito non ackato
    //2 sta per pacchetto spedito e ackato, credo si possa migliorare
    //devo impostare sendBase e nextseqnum, non so bene quando
}

void sentPacket(pthread_mutex_t *mtxARCVD , int packetN, int windowDim,
                struct timer * packetTimer,
                int slot, int offset, int retransmission)
{
    if(retransmission == 0)
    {

        (selectiveWnd[packetN % windowDim]).value = 1;
        //((details)->selectiveWnd)[packetN % (details)->windowDimension].packetTimer.seqNum = packetN;
        printf("updated selective repeat\n");
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

//----------------------------------------------------------------------------------------------------------------THREAD

void createThread(pthread_t * thread, void * function, void * arguments)
{
    if(pthread_create(thread, NULL, function, arguments) != 0)
    {
        perror("error in pthread_create");
    }

    printf("thread creato\n");
}

//-----------------------------------------------------------------------------------------------------------------TIMER
void * timerFunction()
{

    printf("sono il timer\n\n");
    struct timer currentTimer;


    for(;;)
    {

        initTimerWheel();
        currentTimeSlot = 0;


        //while (*TIMGB == 1)
        for(;;)
        {


            if (timerWheel[currentTimeSlot].nextTimer != NULL)
            {
                printf("ho trovato un assert\n");
            }
            else
            {
                printf("nulla\n");
            }

            currentTimeSlot = (currentTimeSlot + 1) % timerSize;

            sleep(10);
            //usleep((useconds_t) nanoSleep); //sleep di mezzo millisecondo

        }

        exit(EXIT_SUCCESS);
        //printf("mi fermo alla posizione currentTimeSlot = %d \n", *currentTimeSlot);
    }
}

void initTimerWheel(){
    for(int i = 0; i < timerSize; i++)
    {
        timerWheel[i].nextTimer = NULL;
    }
}