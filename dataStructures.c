

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
extern int pipeSendACK[2];
extern volatile int finalLen, globalTimerStop;
extern datagram packet;
extern int globalOpID;

extern pthread_mutex_t syncMTX;

volatile int currentTimeSlot;

pthread_mutex_t currentTSMTX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtxTimerSleep = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condTimerSleep = PTHREAD_COND_INITIALIZER;

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
    //printf("aggiorno selective repeat perchè ho ricevuto ack per = %d\n", ackN);

    mtxLock(&(selectiveWnd[ackN % windowSize]).cellMtx);

    if ((selectiveWnd[ackN % windowSize]).value != 0 && (selectiveWnd[ackN % windowSize]).value != 2)
    {
        //printf("aggiorno la selective repeat\n");
        ((selectiveWnd)[ackN % windowSize]).value = 2;

        //--------------------------------------------------------------------------------andrà protetto con un mutex--------------------

        (((selectiveWnd)[ackN % windowSize]).packetTimer).isValid = 0;
        //printf("stoppato il timer in posizione %d\n", (((selectiveWnd)[ackN % windowSize]).packetTimer).posInWheel);
        //printf("timer all'indirizzo %p\n", &(((selectiveWnd)[ackN % windowSize]).packetTimer));

        //-------------------------------------------------------------------------------------------------------------------------------

        slideWindow();
    }

    mtxUnlock(&(selectiveWnd[ackN % windowSize]).cellMtx);
    //printf("esco da acksentpacket\n");
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

void slideWindow() //secondo me può essere eliminata e messa all'interno di ackSentPacket, alla fine sono tre righe
{
    while(selectiveWnd[details.sendBase%windowSize].value == 2){
        selectiveWnd[details.sendBase%windowSize].value = 0;
        details.sendBase = details.sendBase + 1;
        //printf("mando avanti sendBase\n");
    }
    //printWindow();
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

void initPipe(int pipefd[2])
{
    if(pipe(pipefd) == -1)
    {
        perror("error in pipe open");
    }

    if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) == -1)
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
    //printf("timer thread attivato\n\n");
    struct timer * currentTimer;
    struct pipeMessage rtxN;
    for(;;)
    {

        initTimerWheel();
        mtxLock(&currentTSMTX);
        currentTimeSlot = 0;
        mtxUnlock(&currentTSMTX);

        if(pthread_cond_wait(&condTimerSleep, &mtxTimerSleep) == -1)
        {
            perror("error in cond_wait timer");
        }

        while(readGlobalTimerStop() == 1)
        {
            mtxLock(&currentTSMTX);
            currentTimer = timerWheel[currentTimeSlot].nextTimer;
            mtxUnlock(&currentTSMTX);

            while (currentTimer != NULL) {

                rtxN.seqNum = currentTimer->seqNum;
                mtxLock(&(selectiveWnd[currentTimer->seqNum % windowSize].cellMtx));
                //rtxN.isFinal = 0;
                if (currentTimer->isValid) {
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
            clockTick();
            if (usleep((useconds_t) nanoSleep) == -1) {
                perror("error on usleep");
            }
        }
    }
}

int readGlobalTimerStop()
{
    mtxLock(&syncMTX);
    int var = globalTimerStop;
    mtxUnlock(&syncMTX);
    return var;
}

void clockTick()
{
    mtxLock(&currentTSMTX);

    currentTimeSlot = (currentTimeSlot + 1) % timerSize;

    mtxUnlock(&currentTSMTX);
}

int getWheelPosition()
{
    mtxLock(&currentTSMTX);
    int pos = (currentTimeSlot + offset)%timerSize;

    mtxUnlock(&currentTSMTX);
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
    //printf("indirizzo del timer : %p\n", (timerWheel[posInWheel]).nextTimer);
}

void initTimerWheel()
{
    //printf("inizializzo ruota del timer\n");
    for(int i = 0; i < timerSize; i++)
    {
        timerWheel[i].nextTimer = NULL;
    }
    //printf("inizializzazione terminata\n\n");
}

int checkPipe(struct pipeMessage *rtxN, int pipefd)
{
    memset(rtxN, 0, sizeof(struct pipeMessage));
    if(read(pipefd, rtxN, sizeof(struct pipeMessage)) == -1)
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
        //printf("\n\nho trovato un rtxN\n\n");
        return 1;
    }
}

void sendDatagram(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen, struct datagram_t * sndPacket)
{
    sentPacket(sndPacket->seqNum, 0);

    if (sendto(socketfd, (char *) sndPacket, sizeof(datagram), 0, (struct sockaddr* ) servAddr, servLen)== -1) {
        perror("datagram send error");
    }
    //printf("inviato pacchetto con numero di sequenza %u\n", sndPacket->seqNum);
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
    int isFinal = 0;
    char * buffer = malloc(sizeof(datagram));
    if(buffer == NULL)
    {
        perror("error in buffer malloc");
    }
    else
    {
        handshake *ACK;

        ssize_t msgLen = recvfrom(mainSocket, buffer, sizeof(datagram), 0, address, slen);
        if (msgLen == -1 && errno != EAGAIN)
        {
            perror("error in recvfrom");
        }
        else if (msgLen == -1 && errno == EAGAIN)
            return 0;
        else
        {
            if (msgLen == sizeof(handshake))
            {
                ACK = (handshake *) buffer;
                ackSentPacket(ACK->sequenceNum);
                isFinal = ACK->isFinal;
                //printf("ricevuto ack con numero di sequenza %d\n", ACK->sequenceNum);
                free(ACK);
            }
            else
            {
                printf("ho ricevuto un datagramma invece che un ack, mando l'ack\n");
                datagram * duplicatePacket;
                duplicatePacket = (datagram *) buffer;
                tellSenderSendACK(duplicatePacket->seqNum, duplicatePacket->isFinal);
            }


        }
    }

    return isFinal;
}

/*
int receiveDatagram(int socketfd, struct sockaddr * address, socklen_t *slen, int file, size_t finalLen)
{
    char *buffer = malloc(sizeof(datagram));
    //datagram * pointer = &packet;

    if (buffer == NULL)
        perror("error in datagram malloc");

    ssize_t msgLen = recvfrom(socketfd, buffer, sizeof(buffer), 0, address, slen);
    if (msgLen == -1)
        perror("error in recvfrom");

    if (msgLen != sizeof(handshake)) {
        datagram *packet;
        packet = (datagram *) buffer;

        int isFinal = packet->isFinal;
        int offset = packet->seqNum - details.firstSeqNum;

        if (lseek(file, offset * 512, SEEK_SET) == -1)
            perror("lseek error");

        if (isFinal == 0) {
            writeOnFile(file, packet->content, 512);
        } else {
            writeOnFile(file, packet->content, finalLen);
        }
        if (packet->ackSeqNum != 0)
            ackSentPacket(packet->seqNum);

        tellSenderSendACK(packet->seqNum, (short) isFinal);

        if (isFinal == 0)
            return isFinal;
        else
            return packet->seqNum;
    }
    else {
        handshake *ACK = (handshake *) buffer;
        printf("ricevuto ack\n");
        ackSentPacket(ACK->sequenceNum);
        free(ACK);
        return ACK->isFinal;
    }
}
*/

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
    printf("sto aprendo il file : %s\n", fileName);
    int fd = open(fileName, O_RDONLY);
    if (fd == -1)
    {
        perror("1: error on open file, retransmission");
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
    while(receiveACK(socket, address, slen) != -1)
    {

    }
}

/*void waitForDatagramCycle(int socket, struct sockaddr * address, socklen_t *slen, int file, int firstPacket, size_t finalLen)
{
    int isFinal = 0, lastDatagram = -1;
    isFinal = receiveDatagram(socket, file, address, slen, firstPacket, finalLen);
    while(isFinal == 0 || details.sendBase != lastDatagram)
    {
        isFinal = receiveDatagram(socket, file, address, slen, firstPacket, finalLen);
        if(isFinal != 0)
            lastDatagram = isFinal;
    }
}*/

void getResponse(int socket, struct sockaddr_in * address, socklen_t *slen, int fd)
{
    int isFinal = 0;
    datagram packet;
    int firstPacket = details.remoteSeq + 1;//        lo passo a writeonfile insieme al pacchetto in modo da ricostruire
    int ackreceived = 0;

    mtxLock(&syncMTX);
    int opID = globalOpID;
    mtxUnlock(&syncMTX);

    while(isFinal != -1)
    {
        if(checkSocketDatagram(address, *slen, socket, &packet) == 1)
        {
            if(packet.opID == opID) {
                isFinal = packet.isFinal;
                //----------------------------------------------------------------
                if (isFinal == 0)
                    writeOnFile(fd, packet.content, packet.seqNum, firstPacket, 512);
                else if (isFinal == 1) {
                    writeOnFile(fd, packet.content, packet.seqNum, firstPacket, (size_t) finalLen);
                    printf("ho scritto il pacchetto finale con valore finallen = %d\n", finalLen);
                }
                //----------------------------------------------------------------

                if (!ackreceived) {
                    ackSentPacket(packet.ackSeqNum);
                    ackreceived = 1;
                }


                details.remoteSeq = packet.seqNum;
                tellSenderSendACK(packet.seqNum, packet.isFinal);
                memset(&packet, 0, sizeof(datagram));
            }
        }
        //int checkSocketDatagram(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, datagram * packet)
    }

    printf("\n\n\nho ricevuto il sommo pacchetto finale\n\n\n");
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

void writeOnFile(int file, char * content, int seqnum, int firstnum ,size_t len)
{
    offset = seqnum-firstnum;
    //printf("\nseqnum = %d, firstnum = %d\n", seqnum, firstnum);
    if (firstnum != 0)//-----------------------------------------------è a 0 nella list
    {
        //printf("faccio una lseek\n");
        if ((lseek(file, offset * 512, SEEK_SET)) == -1)
        {
            perror("1: lseek error");
        }
    }

    if (write(file, content, len) == -1)
    {
        perror("error in writeOnFile");
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
        writeOnFile(pipeSendACK[1], (char *) tellACK, 0, 0, sizeof(struct pipeMessage));
    }
}

void ACKandRTXcycle(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen)
{
    int finish = 0;
    struct pipeMessage * pm = malloc(sizeof(struct pipeMessage));
    if(pm == NULL)
        perror("error in malloc");

    handshake * ACK ;
    while(finish != -1)
    {
        if (checkPipe(pm, pipeSendACK[0]) == 1)
        {
            ACK = malloc(sizeof(handshake));

            if (ACK == NULL)
                perror("error in malloc");
            else
            {
                //printf("devo mandare un ack con numero di sequenza : %u\n", pm->seqNum);
                finish = pm->isFinal;
                if(finish == 1)
                    printf("valore di finish (SENDER) = %u\n", finish);
                ACK->isFinal = pm->isFinal;
                ACK->sequenceNum = pm->seqNum;
                sendACK(socketfd, ACK, servAddr, servLen);
            }
            memset(pm, 0, sizeof(struct pipeMessage));
        }
        if (checkPipe(pm, pipeFd[0]) == 1)
        {
            //datagram * packetRTX = rebuildDatagram(*pm);
            //sendDatagram(socketfd, servAddr, servLen, &packet);
            //memset(pm, 0, sizeof(struct pipeMessage));
            printf("\n\nritrasmetto\n");
        }
    }
}

datagram rebuildDatagram(int fd, struct pipeMessage pm)
{
    printf("ritrasmetto\n");
    datagram sndPacket;
    if(lseek(fd, 512*(pm.seqNum - details.firstSeqNum), SEEK_SET) == -1)
    {
        perror("errore in lseek");
    }
    if(read(fd, sndPacket.content, 512)==-1)
    {
        perror("error in read");
    }

    sndPacket.isFinal = pm.isFinal;
    sndPacket.ackSeqNum = details.remoteSeq;
    sndPacket.seqNum = pm.seqNum;

    mtxLock(&syncMTX);
    sndPacket.opID = globalOpID;
    mtxUnlock(&syncMTX);

    return sndPacket;
}

int getFileLen(int fd)
{

    ssize_t len = lseek(fd, 0L, SEEK_END);
    if(len == -1){
        perror("error in lseek");
    }
    if(lseek(fd, 0L, SEEK_SET) == -1){
        perror("error in lseek");
    }
    len = len%512;
    printf("ho calcolato la grandezza del file\n");
    return (int) len;
}

char * stringParser(char * string)
{
    char * sToReturn  = malloc(512);
    char* start = strrchr(string,'/'); /* Find the last '/' */
    strcpy(sToReturn, start+1);
    return sToReturn;
}

void waitForFirstPacketSender(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen)
{
    int finish = 0;
    struct pipeMessage * pm = malloc(sizeof(struct pipeMessage));
    if(pm == NULL)
        perror("error in malloc");

    while(finish != -1)
    {
        if (checkPipe(pm, pipeSendACK[0]) == 1)
        {
            finish = -1;
            free(pm);
        }
        else if (checkPipe(pm, pipeFd[0]) == 1)
        {
            //datagram packetRTX = rebuildDatagram(*pm);
            sendDatagram(socketfd, servAddr, servLen, &packet);
            memset(pm, 0, sizeof(struct pipeMessage));
            printf("\n\nritrasmetto\n");
        }
    }
}

void waitForFirstPacketListener(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen)
{
    while(receiveACK(socketfd, (struct sockaddr *) servAddr, &servLen) == 0){}

    printf("sono uscito da qui \n\n\n");
    handshake ack;
    ack.isFinal = 1;
    if(write(pipeSendACK[1], &ack, sizeof(handshake))==-1)
    {
        perror("error in write on pipe");
    }
}

void sendSignalTimer()
{
    //PROTEGGI CON MUTEX     <<-------------------------------------<
    mtxLock(&syncMTX);
    globalTimerStop = 1;
    mtxUnlock(&syncMTX);
    if(pthread_cond_signal(&condTimerSleep) == -1)
    {
        perror("error in cond_signal timer");
    }
}
