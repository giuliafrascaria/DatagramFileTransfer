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
extern struct details details;
extern int  timerSize, nanoSleep, windowSize, sendBase;
extern int pipeFd[2];
extern volatile int currentTimeSlot;

pthread_mutex_t posinwheelMTX = PTHREAD_MUTEX_INITIALIZER;

int offset = 10;

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

int checkSocketAck(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, handshake * ACK)
{
    ssize_t res;
    res = recvfrom(socketfd, (char *) ACK, sizeof(handshake), 0, (struct sockaddr *) servAddr, &servLen);

    if((res == -1) && (errno != EAGAIN))
    {
        return -1;
    }
    else if(res > 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

//------------------------------------------------------------------------------------------------------SELECTIVE REPEAT

void initWindow()
{
    memset(selectiveWnd, 0, windowSize * sizeof(struct selectCell));
    int i;
    for(i = 0; i < windowSize; i++)
    {
        selectiveWnd[i].value = 0;
    }

    printf("inizializzo ruota della selective\n");

}

void sentPacket(int packetN, int retransmission)
{
    if(retransmission == 0)
    {
        (selectiveWnd[packetN % windowSize]).value = 1;
        ((selectiveWnd[packetN % windowSize]).packetTimer).seqNum = packetN;
        //printf("updated selective repeat\n");

        int pos = getWheelPosition();
        startTimer(packetN, pos);

        details.mySeq = packetN+1;
    }
}

void ackSentPacket(int ackN)
{
    printf("ricevuto ack per il mio pacchetto inviato con numero di sequenza = %d\n\n\n", ackN);


    if ((selectiveWnd[ackN % windowSize]).value != 0 && (selectiveWnd[ackN % windowSize]).value != 2)
    {
        printf("aggiorno la selective repeat\n");
        ((selectiveWnd)[ackN % windowSize]).value = 2;
    }

    //printWindow();
    slideWindow();
}

void printWindow()
{
    int i;
    printf("\n |");
    for (i = 0; i < windowSize; i++)
    {
        if (i == sendBase % windowSize)
        {
            printf(" (%d) |", (selectiveWnd)[i].value);
        }
        else {
            printf(" %d |", (selectiveWnd)[i].value);
        }
    }
    printf("\n");
}

void slideWindow()
{
    while ((selectiveWnd[sendBase % windowSize]).value == 2)
    {
        selectiveWnd[sendBase % windowSize].value = 0;
        sendBase = sendBase + 1;
    }
}


//----------------------------------------------------------------------------------------------------------------THREAD

void createThread(pthread_t * thread, void * function, void * arguments)
{
    if(pthread_create(thread, NULL, function, arguments) != 0)
    {
        perror("error in pthread_create");
    }
    //printf("thread creato\n");
}

void initPipe()
{
    if(pipe(pipeFd) == -1)
    {
        perror("error in pipe open");
    }

    if (fcntl(pipeFd[0], F_SETFL, O_NONBLOCK) == -1)
    {
        perror("error in fcntl");
    }

}

void mtxLock(pthread_mutex_t * mtx)
{
    if(pthread_mutex_lock(mtx) == -1)
    {
        perror("error on mutex lock");
    }
}

void mtxUnlock(pthread_mutex_t * mtx)
{
    if(pthread_mutex_unlock(mtx) == -1)
    {
        perror("error on mutex unlock");
    }
}

//-----------------------------------------------------------------------------------------------------------------TIMER
void * timerFunction()
{
    printf("timer thread attivato\n\n");
    struct timer * currentTimer;
    struct pipeMessage rtxN;

    memset(&rtxN, 0, sizeof(struct pipeMessage));

    for(;;)
    {

        initTimerWheel();
        currentTimeSlot = 0;

        //while (*TIMGB == 1)
        for (;;) {

            currentTimer = timerWheel[currentTimeSlot].nextTimer;

            if (currentTimer != NULL)
            {
                printf("ho trovato un assert in posizione %d\n", currentTimeSlot);
                rtxN.seqNum = currentTimer->seqNum;
                rtxN.isFinal = 0;
                if(write(pipeFd[1], &rtxN, sizeof(struct pipeMessage)) == -1)
                {
                    perror("error in pipe write");
                }

                memset(&rtxN, 0, sizeof(struct pipeMessage));
            }
//            else
//            {
//                printf("cella vuota\n");
//            }

            clockTick();
            usleep((useconds_t) nanoSleep);

        }
        exit(EXIT_SUCCESS);
    }
}

void clockTick()
{
    mtxLock(&posinwheelMTX);

    currentTimeSlot = (currentTimeSlot + 1) % timerSize;

    mtxUnlock(&posinwheelMTX);
}

int getWheelPosition()
{
    mtxLock(&posinwheelMTX);
    int pos = (currentTimeSlot + offset)%timerSize;

    mtxUnlock(&posinwheelMTX);
    //printf("timer will be set in position %d\n\n", pos);
    return(pos);
}

void startTimer(int packetN, int posInWheel)
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

    //printf("setting timer in wheel position %d\n", posInWheel);
    (timerWheel[posInWheel]).nextTimer = &(selectiveWnd[(packetN)%(windowSize)].packetTimer);
    //selectiveWnd[(packetN)%(windowSize)].wheelTimer = packetTimer;
}

void initTimerWheel()
{
    printf("inizializzo ruota del timer\n");
    for(int i = 0; i < timerSize; i++)
    {
        timerWheel[i].nextTimer = NULL;
    }
    //printf("inizializzazione terminata\n\n");
}

//----------------------------------------------------------------------------------------------------------------

/*
void retransmissionServer( int pipeRT, datagram * packet, int firstPacket, char * FN)
{
    int sequenceNumber,fd;
    ssize_t readByte;
    datagram sndPacket;



    //stessa cosa di retransmission client
    //-----------------------------------------------------------------------------------------------------------------------------------

    while (read(pipeRT, &sequenceNumber, sizeof(int)) == -1) {
        perror("1: error in read pipe");
        sleep(1);
    }

    //------------------------------------------------------------------------------------------------------------------------------------


    sndPacket.seqNum = sequenceNumber;
    sndPacket.opID = packet->opID;
    sndPacket.isFinal = 0;
    sndPacket.command = packet->command;

    fd = open(FN, O_RDONLY);
    while (fd == -1) {
        perror("1: error on open file, retransmission");
        sleep(1);
        fd = open(FN, O_RDONLY);
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
    if (write(details.sockfd, (char *) &sndPacket, sizeof(datagram)) == -1) {
        perror("datagram send error");
    }
    if (close(fd) == -1) {
        perror("0: error on close, retransmission");
    }

}

void retransmissionClient( int pipeRT, datagram * packet, int firstPacket, char * FN)
{

    //per come stiamo strutturando adesso io credo che questo vada cambiato, la read sulla pipe dovrebbe già essere
    //avvenuta, qui si passa il pipeMessage e questo manda solo il paccheto ricostruito

    int sequenceNumber, fd;
    ssize_t readByte;
    struct timer *packetTimer = malloc(sizeof(struct timer));
    datagram sndPacket;



    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    //----------------------------------------------------------------ERRORE----------------------------------------------------------------------

    //non legge un intero dalla pipe, così va in segmentation fault. legge un pipeMessage
    while (read(pipeRT, &sequenceNumber, sizeof(int)) == -1) {
        perror("1: error in read pipe");
        sleep(1);
    }
    //--------------------------------------------------------------------------------------------------------------------------------------------
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%




    packetTimer->seqNum = sequenceNumber;
    if (packet->command == 0 || packet->command == 2 || (packet->command == 1 && sequenceNumber == firstPacket))
    {
        sentPacket(sndPacket.seqNum, 1);

        printf("mando il paccetto ritrasmesso\n");
        sendDatagram(&details, packet);

    }
    else    //packet.command == 1 && firstpacket != 0
    {

        sndPacket.seqNum = sequenceNumber;
        sndPacket.opID = packet->opID;
        printf("RETRANSMISSION : ritrasmetto pacchetto con numero di sequenza %d\n", sndPacket.seqNum);

        fd = openFile(FN);

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

        sendDatagram(&details, &sndPacket);

        if (close(fd) == -1) {
            perror("0: error on close, retransmission");
        }

    }
}

 */

int checkPipe(struct pipeMessage *rtxN)
{
    if(read(pipeFd[0], rtxN, sizeof(struct pipeMessage)) == -1)
    {
        if(errno != EAGAIN)
        {
            perror("error in pipe read");
            return -1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        printf("\n\nho trovato un rtxN\n\n");
        memset(rtxN, 0, sizeof(struct pipeMessage));
        return 1;
    }
}

void sendDatagram(struct details * details, struct datagram_t * sndPacket)
{
    if (send(details->sockfd, (char *) sndPacket, sizeof(datagram), 0) == -1) {
        perror("datagram send error");
    }
}

void sendACK(int socketfd, handshake *ACK, struct sockaddr_in * servAddr, socklen_t servLen)
{
    ssize_t sentData;
    sentData = sendto(socketfd, (char *) ACK, sizeof(handshake), 0, (struct sockaddr* ) servAddr, servLen);
    if(sentData == -1)
    {
        perror("error in sending data\n");
        exit(EXIT_FAILURE);
    }
    //printf("sent ACK number %d\n", ACK->sequenceNum);
}

void receiveACK(int mainSocket, struct sockaddr * address, socklen_t *slen)
{
    handshake *ACK = malloc(sizeof(handshake));
    if(ACK == NULL)
    {
        perror("error in malloc");
    }

    ssize_t msgLen = recvfrom(mainSocket, (char *) ACK, sizeof(handshake), 0, address, slen);
    if(msgLen == -1)
    {
        perror("error in recvfrom");
    }
    //aggiorno selective repeat e blocco timer

    ackSentPacket(ACK->sequenceNum);

    free(ACK);

}

void acceptConnection(int mainSocket, handshake * ACK, struct sockaddr * address, socklen_t *slen)
{
    ssize_t msgLen = recvfrom(mainSocket, (char *) ACK, sizeof(handshake), 0, address, slen);
    if(msgLen == -1)
    {
        perror("error in recvfrom");
    }
}

int openFile(char * fileName)
{
    int fd = open(fileName, O_RDONLY);
    while (fd == -1) {
        perror("1: error on open file, retransmission");
        sleep(1);
        fd = open(fileName, O_RDONLY);
    }
    return fd;
}

void closeFile(int fd)
{
    if(close(fd) == -1)
    {
        perror("error in file close\n");
        exit(EXIT_FAILURE);
    }
}

void sendSignalThread(pthread_mutex_t * mtx, pthread_cond_t * condition)
{
    mtxLock(mtx);
    if(pthread_cond_signal(condition) != 0)
    {
        perror("error in cond signal");
    }
    mtxUnlock(mtx);
}