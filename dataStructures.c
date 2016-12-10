//
// Created by giogge on 09/12/16.
//


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include "dataStructures.h"


extern struct selectCell selectiveWnd[];
extern struct headTimer timerWheel[];
extern int  timerSize, nanoSleep, windowSize;
extern volatile int currentTimeSlot;

int offset = 3;

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

void initWindow()
{
    memset(selectiveWnd, 0, windowSize * sizeof(struct selectCell));
    int i;
    for(i = 0; i < windowSize; i++)
    {
        selectiveWnd[i].seqNum = -1;
        selectiveWnd[i].value = 0;
        selectiveWnd[i].wheelTimer = NULL;
    }

    printf("inizializzazione terminata\n");
    //setta tutte le celle a 0, cioè cella vuota
    //1 sta per pacchetto spedito non ackato
    //2 sta per pacchetto spedito e ackato, credo si possa migliorare
    //devo impostare sendBase e nextseqnum, non so bene quando
}

void sentPacket(int packetN, int retransmission)
{
    if(retransmission == 0)
    {
        (selectiveWnd[packetN % windowSize]).value = 1;
        ((selectiveWnd[packetN % windowSize]).packetTimer).seqNum = packetN;
        printf("updated selective repeat\n");

        int pos = getWheelPosition();
        startTimer(packetN, pos);
    }
}


int getWheelPosition()
{
    int pos = (currentTimeSlot + offset)%timerSize;
    printf("timer will be set in position %d\n\n", pos);
    return(pos);
}

void ackSentPacket(int ackN, int currentSlot, struct details *details)
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

        (selectiveWnd)[ackN % (details)->windowDimension].value = 2;


        while ((selectiveWnd)[((details)->sendBase % (details)->windowDimension)].value == 2) {

            (selectiveWnd)[((details)->sendBase % (details)->windowDimension)].value = 0;

            (details)->sendBase = (details)->sendBase + 1;

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

            usleep((useconds_t) nanoSleep); //sleep di mezzo millisecondo

        }

        exit(EXIT_SUCCESS);
        //printf("mi fermo alla posizione currentTimeSlot = %d \n", *currentTimeSlot);
    }
}

void startTimer( int packetN, int posInWheel)
{

    (selectiveWnd[(packetN)%(windowSize)].packetTimer).seqNum = packetN;
    (selectiveWnd[(packetN)%(windowSize)].packetTimer).isValid = 1;
    (selectiveWnd[(packetN)%(windowSize)].packetTimer).posInWheel = posInWheel;

    if(timerWheel[posInWheel].nextTimer != NULL)
    {
        (selectiveWnd[(packetN)%(windowSize)].packetTimer).nextTimer = timerWheel[posInWheel].nextTimer;

    }
    else
        ((selectiveWnd[(packetN)%(windowSize)].packetTimer).nextTimer = NULL);

    printf("setting timer in wheel position %d\n", posInWheel);
    (timerWheel[posInWheel]).nextTimer = &(selectiveWnd[(packetN)%(windowSize)].packetTimer);
    //selectiveWnd[(packetN)%(windowSize)].wheelTimer = packetTimer;
}



void initTimerWheel()
{
    printf("inizializzo ruota\n\n");
    for(int i = 0; i < timerSize; i++)
    {
        timerWheel[i].nextTimer = NULL;
    }
    printf("inizializzazione terminata\n\n");
}

//----------------------------------------------------------------------------------------------------------------

void receiveMsg(int mainSocket, handshake * SYN, size_t SYNlen, struct sockaddr * address, socklen_t *slen)
{
    ssize_t msgLen = recvfrom(mainSocket, (char *) SYN, SYNlen, 0, address, slen);
    if(msgLen == -1)
    {
        perror("error in recvfrom");
    }
}


void retransmissionServer( int pipeRT, struct details * details, datagram * packet,
                           int firstPacket, char ** FN)
{
    int sequenceNumber,fd;
    ssize_t readByte;
    datagram sndPacket;


    while (read(pipeRT, &sequenceNumber, sizeof(int)) == -1) {
        perror("1: error in read pipe");
        sleep(1);
    }
    sndPacket.seqNum = sequenceNumber;
    sndPacket.opID = packet->opID;
    sndPacket.isFinal = 0;
    sndPacket.command = packet->command;

    fd = open(*FN, O_RDONLY);
    while (fd == -1) {
        perror("1: error on open file, retransmission");
        sleep(1);
        fd = open(*FN, O_RDONLY);
    }
    while (lseek(fd, (sequenceNumber - firstPacket) * 512, SEEK_SET) == -1) {
        perror("1: lseek error");
        sleep(1);
    }
    readByte = read(fd, sndPacket.content, 512);
    while (readByte == -1) {
        perror("1: error in read");
        readByte = read(fd, sndPacket.content, 512);
    }
    if (readByte < 512 && readByte >= 0) {
        sndPacket.isFinal = 1;
        printf("sto ritrasmettendo il pacchetto finale\n");
    }

    int pos = getWheelPosition();
    startTimer(sequenceNumber, pos);
    if (write(details->sockfd, (char *) &sndPacket, sizeof(datagram)) == -1) {
        perror("datagram send error");
    }
    if (close(fd) == -1) {
        perror("0: error on close, retransmission");
    }

}

void retransmissionClient( int pipeRT, struct details * details, datagram * packet,
                           int firstPacket, char ** FN)
{


    int sequenceNumber, fd;
    ssize_t readByte;
    struct timer *packetTimer = malloc(sizeof(struct timer));
    datagram sndPacket;

    while (read(pipeRT, &sequenceNumber, sizeof(int)) == -1) {
        perror("1: error in read pipe");
        sleep(1);
    }
    packetTimer->seqNum = sequenceNumber;
    if (packet->command == 0 || packet->command == 2 || (packet->command == 1 && sequenceNumber == firstPacket))
    {
        printf("faccio lo start del timer\n");
        if((packetTimer = malloc(sizeof(struct timer)))==NULL)
        {
            perror("error in malloc");
        }
        sentPacket(sndPacket.seqNum, 1);


        printf("mando il paccetto ritrasmesso\n");
        if (send(details->sockfd, (char *) &packet, sizeof(datagram), 0) == -1) {
            perror("datagram send error");
        }

    } else    //packet.command == 1 && firstpacket != 0
    {

        sndPacket.seqNum = sequenceNumber;
        sndPacket.opID = packet->opID;
        printf("RETRANSMISSION : ritrasmetto pacchetto con numero di sequenza %d\n", sndPacket.seqNum);

        fd = open(*FN, O_RDONLY);
        while (fd == -1) {
            perror("1: error on open file, retransmission");
            sleep(1);
            fd = open(*FN, O_RDONLY);
        }
        while (lseek(fd, (sequenceNumber - firstPacket) * 512, SEEK_SET) == -1) {
            perror("1: lseek error");
            sleep(1);
        }
        readByte = read(fd, sndPacket.content, 512);
        while (readByte == -1) {
            perror("1: error in read");
            readByte = read(fd, sndPacket.content, 512);
        }
        if (readByte < 512 && readByte >= 0) {
            sndPacket.isFinal = 1;
        }
        sentPacket(sndPacket.seqNum, 1);

        if (send(details->sockfd, (char *) &sndPacket, sizeof(datagram), 0) == -1) {
            perror("datagram send error");
        }

        if (close(fd) == -1) {
            perror("0: error on close, retransmission");
        }

    }
}
