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
int pipeSendACK[2];
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

int checkSocketDatagram(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, datagram * packet)
{
    ssize_t res;
    res = recvfrom(socketfd, (char *) packet, sizeof(datagram), 0, (struct sockaddr *) servAddr, &servLen);

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
        (selectiveWnd[i].packetTimer).nextTimer = NULL;
        if(pthread_mutex_init(&(selectiveWnd[i].cellMtx), NULL) != 0)
        {
            perror("mutex init error");
        }
    }

    printf("inizializzo ruota della selective\n");

}

void sentPacket(int packetN, int retransmission)
{
    if(retransmission == 0)
    {
        mtxLock(&(selectiveWnd[packetN % windowSize]).cellMtx);

        (selectiveWnd[packetN % windowSize]).value = 1;
        ((selectiveWnd[packetN % windowSize]).packetTimer).seqNum = packetN;
        //printf("updated selective repeat\n");

        int pos = getWheelPosition();
        startTimer(packetN, pos);

        mtxUnlock(&(selectiveWnd[packetN % windowSize]).cellMtx);

        details.mySeq = packetN+1;
    }
}

void ackSentPacket(int ackN)
{
    printf("aggiorno selective repeat perchè ho ricevuto ack per = %d\n\n\n", ackN);

    mtxLock(&(selectiveWnd[ackN % windowSize]).cellMtx);

    if ((selectiveWnd[ackN % windowSize]).value != 0 && (selectiveWnd[ackN % windowSize]).value != 2)
    {
        printf("aggiorno la selective repeat\n");
        ((selectiveWnd)[ackN % windowSize]).value = 2;

        //--------------------------------------------------------------------------------andrà protetto con un mutex--------------------

        (((selectiveWnd)[ackN % windowSize]).packetTimer).isValid = 0;
        printf("stoppato il timer in posizione %d\n", (((selectiveWnd)[ackN % windowSize]).packetTimer).posInWheel);
        printf("timer all'indirizzo %p\n", &(((selectiveWnd)[ackN % windowSize]).packetTimer));

        //-------------------------------------------------------------------------------------------------------------------------------
    }

    //printWindow();
    slideWindow();

    mtxUnlock(&(selectiveWnd[ackN % windowSize]).cellMtx);
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
    /*printf("finestra dopo scorrimento\n");
    printWindow();*/
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
    for(;;)
    {

        initTimerWheel();
        currentTimeSlot = 0;

        //while (*TIMGB == 1)
        for (;;) {
            currentTimer = timerWheel[currentTimeSlot].nextTimer;


            while (currentTimer != NULL)
            {
                //printf("ho trovato un assert in posizione %d, validità del timer: %d\n", currentTimeSlot, currentTimer->isValid);
                //printf("indirizzo rilevato dal timer : %p\n", currentTimer);

                rtxN.seqNum = currentTimer->seqNum;

                mtxLock(&(selectiveWnd[currentTimer->seqNum % windowSize].cellMtx));

                //rtxN.isFinal = 0;
                if(currentTimer->isValid)
                {

                    if (write(pipeFd[1], &rtxN, sizeof(struct pipeMessage)) == -1) {
                        perror("error in pipe write");
                    }
                }

                //printf("|%d, %d|", currentTimer->seqNum, currentTimer->isValid);
                currentTimer->isValid = 0;
                mtxUnlock(&(selectiveWnd[currentTimer->seqNum % windowSize].cellMtx));

                memset(&rtxN, 0, sizeof(struct pipeMessage));

                currentTimer = currentTimer->nextTimer;
            }
            //printf("|_|\n");

            clockTick();

            if(usleep((useconds_t) nanoSleep) == -1)
            {
                perror("error on usleep");
            }

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

    (timerWheel[posInWheel]).nextTimer = &(selectiveWnd[(packetN)%(windowSize)].packetTimer);
    printf("indirizzo del timer : %p\n", (timerWheel[posInWheel]).nextTimer);
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

int receiveACK(int mainSocket, struct sockaddr * address, socklen_t *slen)
{
    int isFinal;
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
    ackSentPacket(ACK->sequenceNum);
    isFinal = ACK->isFinal;
    free(ACK);
    return isFinal;
}

int receiveDatagram(int socketfd, int file, struct sockaddr * address, socklen_t *slen, int firstN, size_t finalLen)
{
    datagram * packet = malloc(sizeof(datagram));

    if(packet == NULL)
        perror("error in datagram malloc");

    ssize_t msgLen = recvfrom(socketfd, (char *) packet, sizeof(datagram), 0, address, slen);
    if(msgLen == -1)
        perror("error in recvfrom");

    int isFinal = packet->isFinal;
    int offset = packet->seqNum - firstN;

    if(lseek(file, offset*512, SEEK_SET) == -1)
        perror("lseek error");

    if(isFinal == 0)
    {
        writeOnFile(file, packet->content, 512);
    }
    else
    {
        writeOnFile(file, packet->content, finalLen);
    }

    if(packet->ackSeqNum != 0)
        ackSentPacket(packet->seqNum);

    tellSenderSendACK(packet->seqNum, (short) isFinal);

    if(isFinal == 0)
        return isFinal;
    else
        return packet->seqNum;
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

void waitForAckCycle(int socket, struct sockaddr * address, socklen_t *slen)
{
    while(receiveACK(socket, address, slen) == 0)
    {

    }
    while(checkWindowSendBase() == 0)
    {
        receiveACK(socket, address, slen);
    }
}

void waitForDatagramCycle(int socket, struct sockaddr * address, socklen_t *slen, int file, int firstPacket, size_t finalLen)
{
    int isFinal = 0, lastDatagram = -1;
    isFinal = receiveDatagram(socket, file, address, slen, firstPacket, finalLen);
    while(isFinal == 0 || details.sendBase != lastDatagram)
    {
        isFinal = receiveDatagram(socket, file, address, slen, firstPacket, finalLen);
        if(isFinal != 0)
            lastDatagram = isFinal;
    }
}

int checkWindowSendBase()
{
    for(int i = 0; i < windowSize; i++)
    {
        if(selectiveWnd[i].value == 1)
        {
            return 0;
        }
    }
    return 1;
}

void writeOnFile(int file, char * content, size_t len)
{
    if (write(file, content, len) == -1)
    {
        perror("error in write");
    }
}

void tellSenderSendACK(int packetN, short int isFinal)
{
    struct pipeMessage * tellACK = malloc(sizeof(struct pipeMessage));
    if(tellACK == NULL)
    {
        perror("error in malloc (function tellSenderSendACK)");
    }
    else
    {
        tellACK->seqNum = packetN;
        tellACK->isFinal = isFinal;
        writeOnFile(pipeSendACK[1], (char *) tellACK, sizeof(struct pipeMessage));
    }
}

void sendACKcycle(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen)
{
    int isFinal = 0;
    struct pipeMessage * pm = malloc(sizeof(struct pipeMessage));
    if(pm == NULL)
        perror("error in malloc");

    handshake * ACK ;

    while(isFinal == 0)
    {
        if(read(pipeSendACK[0], pm, sizeof(struct pipeMessage)) == -1)
            perror("error in read");

        isFinal = pm->isFinal;

        ACK = malloc(sizeof(handshake));
        if(ACK == NULL)
            perror("error in malloc");
        else
        {
            ACK->isFinal = (short) isFinal;
            ACK->sequenceNum = pm->seqNum;
            sendACK(socketfd, ACK, servAddr, servLen);
        }
    }
}
