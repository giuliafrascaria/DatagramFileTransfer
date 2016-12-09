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
extern int windowSize, timerSize, nanoSleep;
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

void createThread(pthread_t * thread, void * function, void * arguments)
{
    if(pthread_create(thread,NULL, function, arguments) != 0)
    {
        perror("error in pthread_create");
    }
}

void * timerFunction()
{

    struct timer currentTimer;
    int elimina = 0;

    for(;;)
    {
        //printf("dormo \n\n\n\n\n");
        /*while (pthread_cond_wait(condTIM, mtxTIM) != 0)
        {
            perror("1: error on cond_wait");
            sleep(1);
        }
        */

        initTimerWheel();
        currentTimeSlot = 0;


        //while (*TIMGB == 1)
        for(;;)
        {


            if (timerWheel[currentTimeSlot].nextTimer != NULL) {
                printf("ho trovato un assert\n");
                currentTimer = *timerWheel[currentTimeSlot].nextTimer;

                if (currentTimer.isValid == 1) {

                    printf("ho trovato il timer del pacchetto %d, è il primo della coda\n", currentTimer.seqNum);
                    currentTimer.isValid = 0;

                    /*if (write(pipeRT, (int *) &currentTimer.seqNum, sizeof(int)) == -1) {
                        perror("error in write pipe");
                    }*/

                    //*dihtr = *dihtr +1;
                    //printf("TIMER : valore di dihtr = %d\n", *dihtr);

                }
                elimina++;
                while (currentTimer.nextTimer != NULL) {

                    //rischio segfault
                    currentTimer = *(currentTimer.nextTimer);

                    if (currentTimer.isValid == 1) {
                        //printf("timer %u\n", currentTimeSlot);
                        //printf("ho trovato il timer del pacchetto\n");
                        printf("ho trovato il timer del pacchetto %d \n", currentTimer.seqNum);
                        currentTimer.isValid = 0;

                        //printf("TIMER : scrivo nella pipe\n");
                        /*if (write(pipeRT, (int *) &currentTimer.seqNum, sizeof(int)) == -1) {
                            perror("error in write pipe");
                        }*/


                        //*dihtr = *dihtr +1;
                        //printf("TIMER : valore di dihtr = %d\n", *dihtr);

                    }
                    elimina++;
                }

            }

            if (elimina != 0) {
                //printf("---------------\nmetto a NULL la posizione  %d\n----------------\n", *currentTimeSlot);
                timerWheel[currentTimeSlot].nextTimer = NULL;
                elimina = 0;

            }

            currentTimeSlot = (currentTimeSlot + 1) % timerSize;

            usleep((useconds_t) nanoSleep); //sleep di mezzo millisecondo
        exit(EXIT_SUCCESS);
        }

        //printf("mi fermo alla posizione currentTimeSlot = %d \n", *currentTimeSlot);
    }
}

void initTimerWheel(){
    for(int i = 0; i < timerSize; i++)
    {
        timerWheel[i].nextTimer = NULL;
    }
}