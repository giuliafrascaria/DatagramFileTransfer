

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <values.h>
#include "dataStructures.h"


#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

struct selectCell selectiveWnd[WINDOWSIZE];
struct headTimer timerWheel[TIMERSIZE] = {NULL};

extern struct details details;
extern int  timerSize, nanoSleep, windowSize;
extern int pipeFd[2];
extern int pipeSendACK[2];
extern volatile int globalTimerStop;
extern datagram packet;
extern int globalOpID;
extern pthread_mutex_t syncMTX;
extern pthread_mutex_t mtxPacketAndDetails;
extern struct RTTsample currentRTT;

struct timespec tstart={0,0};
struct timespec tend={0,0};

volatile int currentTimeSlot = 0;
volatile int rounds = 0;
volatile int roundsSender = 0;
volatile int globalSenderWait = 0;

pthread_mutex_t roundsMTX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t globalSenderWaitMtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t currentTSMTX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t headtimerMTX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtxTimerSleep = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condTimerSleep = PTHREAD_COND_INITIALIZER;
pthread_mutex_t timemtx = PTHREAD_MUTEX_INITIALIZER;

volatile int dataError = 0;


int offset = BASETIMER;

//------------------------------------------------------------------------------------------------------------CONNECTION

struct sockaddr_in createStruct(unsigned short portN)
{
    struct sockaddr_in address;
    socklen_t serverLen = sizeof(struct sockaddr_in);

    memset((void *) &address, 0, serverLen);//reset del contenuto

    address.sin_family = AF_INET;
    address.sin_port = htons(portN);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    //address.sin_addr.s_addr = inet_addr("95.239.229.223");
    //printf("porta : %d\n", address.sin_port );
    //printf("address : %d\n", address.sin_addr.s_addr );

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

void acceptConnection(int mainSocket, handshake * ACK, struct sockaddr * address, socklen_t *slen)
{
    ssize_t msgLen = recvfrom(mainSocket, (char *) ACK, sizeof(handshake), 0, address, slen);
    if(msgLen == -1)
    {
        perror("error in recvfrom");
    }
}

//------------------------------------------------------------------------------------------------------SELECTIVE REPEAT

void initWindow(int times)
{
    memset(selectiveWnd, 0, windowSize * sizeof(struct selectCell));
    int i;
    for(i = 0; i < windowSize; i++)
    {
        if(times == 0)
        {
            if (pthread_mutex_init(&(selectiveWnd[i].cellMtx), NULL) != 0) {
                perror("mutex init error");
            }


            currentRTT.seqNum = -1;
            currentRTT.previousEstimate = 3*(10^9);
            currentRTT.RTT = 3*(10^9);
        }
        mtxLock(&(selectiveWnd[i].cellMtx));
        selectiveWnd[i].value = 0;
        (selectiveWnd[i].packetTimer).nextTimer = NULL;
        mtxUnlock(&(selectiveWnd[i].cellMtx));
    }
}

void sentPacket(int packetN, int retransmission)
{
    mtxLock(&((selectiveWnd[packetN % windowSize]).cellMtx));

    (selectiveWnd[packetN % windowSize]).value = 1;
    ((selectiveWnd[packetN % windowSize]).packetTimer).seqNum = packetN;

    int pos = getWheelPosition();



    startTimer(packetN, pos);

    mtxUnlock(&((selectiveWnd[packetN % windowSize]).cellMtx));

    if(retransmission == 0)
    {
        mtxLock(&mtxPacketAndDetails);
        details.mySeq = (details.mySeq+1)%MAXINT;
        mtxUnlock(&mtxPacketAndDetails);
    }
}

void ackSentPacket(int ackN)
{
    mtxLock(&((selectiveWnd[ackN % windowSize]).cellMtx));

    if ((selectiveWnd[ackN % windowSize]).value != 0 && (selectiveWnd[ackN % windowSize]).value != 2)
    {
        ((selectiveWnd)[ackN % windowSize]).value = 2;
//        if(getWheelPosition() != (((selectiveWnd)[ackN % windowSize]).packetTimer).posInWheel)
//        {
            (((selectiveWnd)[ackN % windowSize]).packetTimer).isValid = 0;
//        }
        mtxUnlock(&((selectiveWnd[ackN % windowSize]).cellMtx));
        slideWindow();
    }
    else {
        mtxUnlock(&((selectiveWnd[ackN % windowSize]).cellMtx));
    }
}

void slideWindow()
{
    int end = 0;
    while(!end)
    {
        mtxLock(&((selectiveWnd[getSendBase()% windowSize]).cellMtx));
        while(selectiveWnd[getSendBase()%windowSize].value == 2)
        {

            selectiveWnd[getSendBase()%windowSize].value = 0;
            mtxUnlock(&((selectiveWnd[getSendBase() % windowSize]).cellMtx));

            mtxLock(&mtxPacketAndDetails);
            details.sendBase = details.sendBase + 1;
            mtxUnlock(&mtxPacketAndDetails);
            mtxLock(&((selectiveWnd[getSendBase()% windowSize]).cellMtx));
        }
        end = 1;
        mtxUnlock(&((selectiveWnd[getSendBase() % windowSize]).cellMtx));
    }
    //printWindow();
}

//--------------------------------------------------------------------------------------SHORT FUNCTIONS TO SIMPLIFY CODE

void createThread(pthread_t * thread, void * function, void * arguments)
{
    if(pthread_create(thread, NULL, function, arguments) != 0)
    {
        perror("error in pthread_create");
    }
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
    if(pthread_mutex_lock(mtx) != 0)
    {
        perror("error on mutex lock");
    }
}

void mtxUnlock(pthread_mutex_t * mtx)
{
    if(pthread_mutex_unlock(mtx) != 0)
    {
        perror("error on mutex unlock");
    }
}

int openFile(char * fileName)
{
    int fd = open(fileName, O_RDONLY);
    if (fd == -1)
    {
        perror("1: error on open file, retransmission");
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

void sendSignalThread(pthread_mutex_t * mtx, pthread_cond_t * condition, int connection)
{
    if (connection == 0) {
        while (getGlobalSenderWait() == 0) {
            sleep(1);
        }
    }
    mtxLock(mtx);
    if(pthread_cond_signal(condition) != 0)
    {
        perror("error in cond signal");
    }
    mtxUnlock(mtx);
}

void condWaitSender(pthread_mutex_t * mutex, pthread_cond_t *cond, int connection)
{
    if(connection == 0) {
        mtxLock(&globalSenderWaitMtx);
        globalSenderWait = 1;
        mtxUnlock(&globalSenderWaitMtx);
    }
    if(pthread_cond_wait(cond, mutex) != 0)
    {
        perror("error in cond wait");
    }
    if(connection == 0) {
        mtxLock(&globalSenderWaitMtx);
        globalSenderWait = 0;
        mtxUnlock(&globalSenderWaitMtx);
    }
}

void writeOnFile(int file, char * content, int seqnum, int firstnum ,size_t len)
{
    int fileoffset = seqnum-firstnum;
    if(fileoffset < 0)
    {
        fileoffset = MAXINT + fileoffset;
    }
    if (firstnum != 0)//-----------------------------------------------è a 0 nella list
    {
        mtxLock(&roundsMTX);
        if ((lseek(file, (fileoffset<<9), SEEK_SET)) == -1) {
            perror("1: lseek error");
            printf("offset = %d, len = %d, seqnum = %d, firstnum = %d\n", fileoffset, (int) len, seqnum, firstnum);
        }
        mtxUnlock(&roundsMTX);
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

int canISend()
{
    int seqNum = getSeqNum() % windowSize;
    int sendBase = getSendBase() % windowSize;
    int offset;
    if (seqNum >= sendBase) {
        if ((seqNum - sendBase) > (WINDOWSIZE - 1)) {
            return 0;
        } else {
            return 1;
        }
    } else
        offset = windowSize + seqNum - sendBase;

    if ((offset) > (WINDOWSIZE - 1)) {
        return 0;
    } else {
        return 1;
    }
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
        return 1;
    }
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
    return (int) len;
}

char * stringParser(char * string)
{
    char * sToReturn  = malloc(512);
    char* start = strrchr(string,'/'); /* Find the last '/' */
    strcpy(sToReturn, start+1);
    return sToReturn;
}

//-----------------------------------------------------------------------------------------------------------------TIMER

void * timerFunction()
{
    int i = 0;


    struct timer * currentTimer, * examinedtimer;
    struct pipeMessage rtxN;
    for(;;)
    {

        initTimerWheel();
        mtxLock(&currentTSMTX);
        currentTimeSlot = 0;
        mtxUnlock(&currentTSMTX);

        if(i == 0)
        {
            i++;
            mtxLock(&syncMTX);
            globalTimerStop = 2;
            mtxUnlock(&syncMTX);
        }

        if(pthread_cond_wait(&condTimerSleep, &mtxTimerSleep) == -1)
        {
            perror("error in cond_wait timer");
        }

//        for(;;){}

        while(readGlobalTimerStop() == 1)
        {

            mtxLock(&headtimerMTX);
            currentTimer = timerWheel[getCurrentTimeSlot()].nextTimer;
            mtxUnlock(&headtimerMTX);

            while (currentTimer != NULL)
            {
//                if(readGlobalTimerStop()==1)
//                {
                //printf("gestione ritrasmissioni\n");
                mtxLock(&(selectiveWnd[currentTimer->seqNum % windowSize].cellMtx));

                if (currentTimer->isValid)
                {
                    rtxN.seqNum = currentTimer->seqNum;
                    if (write(pipeFd[1], &rtxN, sizeof(struct pipeMessage)) == -1) {
                        perror("error in pipe write");
                    }
                    currentTimer->isValid = 0;
                }
                examinedtimer = currentTimer;
                currentTimer = examinedtimer->nextTimer;
                mtxUnlock(&((selectiveWnd[examinedtimer->seqNum % windowSize]).cellMtx));
                memset(&rtxN, 0, sizeof(struct pipeMessage));
//                }

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

    int actualOffset = (int) (currentRTT.previousEstimate / (nanoSleep*1000));
    int maxoffset = MAX(offset, actualOffset);
    int pos = (currentTimeSlot + maxoffset) % timerSize;
    mtxUnlock(&currentTSMTX);
    return(pos);
}

void startTimer(int packetN, int posInWheel)
{
    memset(&((selectiveWnd[(packetN)%(windowSize)]).packetTimer), 0, sizeof(struct timer));

    ((selectiveWnd[packetN%windowSize]).packetTimer).seqNum = packetN;
    ((selectiveWnd[packetN%windowSize]).packetTimer).isValid = 1;
    ((selectiveWnd[packetN%windowSize]).packetTimer).posInWheel = posInWheel;

    mtxLock(&headtimerMTX);
    if((timerWheel[posInWheel]).nextTimer != NULL)
    {
        ((selectiveWnd[packetN%windowSize]).packetTimer).nextTimer = (timerWheel[posInWheel]).nextTimer;
    }
    else
        ((selectiveWnd[packetN%windowSize]).packetTimer).nextTimer = NULL;


    (timerWheel[posInWheel]).nextTimer = &((selectiveWnd[(packetN)%(windowSize)]).packetTimer);

    mtxUnlock(&headtimerMTX);
}

void initTimerWheel()
{
    for(int i = 0; i < timerSize; i++)
    {
        timerWheel[i].nextTimer = NULL;
    }
}

long updateRTTavg(long previousEstimate, long newRTT)
{

    long RTO, RTTVAR, SRTT;
    long R = newRTT;
    //costante data dallo standard, k = 4

    RTTVAR = R>>1;
    RTTVAR = RTTVAR + (RTTVAR>>3) + (labs( newRTT - previousEstimate)>>3);
    SRTT = newRTT - (newRTT>>2) + (previousEstimate>>2);
    RTO = SRTT + (RTTVAR<<2);
    return RTO;
}

void takingRTT()
{
    mtxLock(&timemtx);
    clock_gettime(CLOCK_MONOTONIC, &tend);
    currentRTT.RTT = tend.tv_nsec - currentRTT.timestamp ;
    currentRTT.previousEstimate = updateRTTavg(currentRTT.previousEstimate, currentRTT.RTT);
    mtxUnlock(&timemtx);
}

void startRTTsample(int seq)
{
    mtxLock(&timemtx);
    if (currentRTT.RTT != 0) {

        currentRTT.seqNum = seq;
        clock_gettime(CLOCK_MONOTONIC, &tstart);
        currentRTT.timestamp = tstart.tv_nsec;
        currentRTT.RTT = 0;
    }
    mtxUnlock(&timemtx);
}

void sendSignalTimer()
{
    mtxLock(&syncMTX);
    globalTimerStop = 1;
    mtxUnlock(&syncMTX);
    if(pthread_cond_signal(&condTimerSleep) != 0)
    {
        perror("error in cond_signal timer");
    }
}

//----------------------------------------------------------------------------------------------------------SEND PACKETS

void sendDatagram(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen, struct datagram_t * sndPacket, int rtx)
{
    sentPacket(sndPacket->seqNum, rtx);


    volatile int diceroll = randomGen();

    if(diceroll >= LOSSPROB)
    {
        if (sendto(socketfd, (char *) sndPacket, sizeof(datagram), 0, (struct sockaddr *) servAddr, servLen) == -1) {
            perror("datagram send error");
        }
        //printf("rand = %d, pacchetto mandato\n", diceroll);

        startRTTsample(sndPacket->seqNum);
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
}

void ACKandRTXcycle(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen, int command)
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
            else {
                finish = pm->isFinal;
                if (finish != -2) {
                    if (finish == 1) {
//                        printf("valore di finish (SENDER) = %u con numero di sequenza %d\n", finish, pm->seqNum);
                    }
                    ACK->isFinal = pm->isFinal;
                    ACK->sequenceNum = pm->seqNum;
                    sendACK(socketfd, ACK, servAddr, servLen);
                }
                else
                {
                    setDataError();
                    finish = -1;
                    printf("exit_failure (sender)\n");
                }
            }
            memset(pm, 0, sizeof(struct pipeMessage));
        }
        if(finish != -1 && !getDataError())
        {
            if (checkPipe(pm, pipeFd[0]) == 1)
            {
                if(command == 0 || command == 2)
                {
                    sendDatagram(socketfd, servAddr, servLen, &packet, 1);
//                    printf("ritrasmetto pacchetto iniziale di list o pull\n");
                }
                else if(command == 1 && pm->seqNum == packet.seqNum)
                {
                    sendDatagram(socketfd, servAddr, servLen, &packet, 1);
//                    printf("ritrasmetto pacchetto iniziale di push\n");
                }
                else
                {
                    datagram packetRTX = rebuildDatagram(0 , *pm, command);
                    sendDatagram(socketfd, servAddr, servLen, &packetRTX, 1);
                    memset(pm, 0, sizeof(struct pipeMessage));
//                    printf("ritrasmetto pacchetto di dati push\n");
                }

            }
        }
        if(getDataError()){
            finish = -1;
            printf("exit_failure (sender)\n");
        }
    }
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
            sendDatagram(socketfd, servAddr, servLen, &packet, 1);
            memset(pm, 0, sizeof(struct pipeMessage));
        }
    }
}

datagram rebuildDatagram(int fd, struct pipeMessage pm, int command)
{
    ssize_t readByte;
    datagram sndPacket;

    sndPacket.command = command;

    if (fd != 0)
    {
        mtxLock(&mtxPacketAndDetails);
        int fileoffset = pm.seqNum - details.firstSeqNum;
        if(fileoffset < 0)
        {
            fileoffset = MAXINT + fileoffset;
        }

        if (lseek(fd, (fileoffset<<9), SEEK_SET) == -1)
        {
            perror("errore in lseek");
        }
        sndPacket.ackSeqNum = details.remoteSeq;
        mtxUnlock(&mtxPacketAndDetails);
        readByte = read(fd, sndPacket.content, 512);
        if (readByte == -1) {
            perror("error in read");
        }
        if (readByte == 0) {
            sndPacket.isFinal = -1;
        }
        if (readByte < 512 && readByte > 0) {
            sndPacket.isFinal = 1;
            sndPacket.packetLen = readByte;
        }
        if (readByte == 512) {
            sndPacket.isFinal = 0;
            sndPacket.packetLen = readByte;
        }
    }
    else
    {
        sndPacket.isFinal = 1;
    }


    sndPacket.seqNum = pm.seqNum;
    sndPacket.opID = getOpID();

    return sndPacket;
}

//-------------------------------------------------------------------------------------------------------RECEIVE PACKETS

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
                isFinal = ACK->isFinal;
                ackSentPacket(ACK->sequenceNum);

                if(ACK->sequenceNum == getRTTseq())
                {
                    takingRTT();
                }
                free(ACK);
            }
            else
            {
                datagram * duplicatePacket;
                duplicatePacket = (datagram *) buffer;
                tellSenderSendACK(duplicatePacket->seqNum, duplicatePacket->isFinal);
            }
        }
    }

    return isFinal;
}

void waitForAckCycle(int socket, struct sockaddr * address, socklen_t *slen)
{
    int received = 0;
    int exit = 0;
    while((received != -1 && exit == 0) && !getDataError())
    {
        received = receiveACK(socket, address, slen);
        if (received == -2){
            setDataError();
            exit = 1;
        }
    }
    if(exit == 0 && !getDataError())
        tellSenderSendACK(0, -1);
    else
        printf("error");
}

void getResponse(int socket, struct sockaddr_in * address, socklen_t *slen, int fd, int command)
{
    int isFinal = 0;
    datagram packet;
    int firstPacket;
    mtxLock(&mtxPacketAndDetails);
    firstPacket = details.firstSeqNum;
    mtxUnlock(&mtxPacketAndDetails);
    int ackreceived = 0;
    int alreadyDone = 0;

    while(isFinal != -1)
    {
        if(checkSocketDatagram(address, *slen, socket, &packet) == 1)
        {
            if(packet.opID == getOpID()) {
                if (packet.seqNum == firstPacket - 1) {
                    struct pipeMessage ack;
                    ack.isFinal = 100;
                    ack.seqNum = 100;
                    tellSenderSendACK(ack.seqNum, ack.isFinal);
                }
                else
                {
                    if (packet.isFinal != -2 || getDataError())
                    {
                        if (packet.isFinal == -1) {
                            mtxLock(&mtxPacketAndDetails);
                            details.remoteSeq = packet.seqNum;
                            mtxUnlock(&mtxPacketAndDetails);
                            isFinal = -1;
                            tellSenderSendACK(packet.seqNum, packet.isFinal);
                            memset(&packet, 0, sizeof(datagram));
                        }
                        else
                        {
                            if (packet.seqNum < firstPacket && alreadyDone == 0) {
                                mtxLock(&roundsMTX);
                                rounds++;
                                mtxUnlock(&roundsMTX);
                                alreadyDone++;
                            }
                            else if (packet.seqNum >= firstPacket && alreadyDone > 0)
                            {
                                alreadyDone = 0;
                            }
                            isFinal = packet.isFinal;
                            //----------------------------------------------------------------
                            writeOnFile(fd, packet.content, packet.seqNum, firstPacket, (size_t) packet.packetLen);
                            //----------------------------------------------------------------
                            if (command == 0 || command == 2) {
                                if (!ackreceived) {
                                    ackSentPacket(packet.ackSeqNum);
                                    ackreceived = 1;
                                }

                                if (packet.ackSeqNum == getRTTseq()) {
                                    takingRTT();
                                }
                            }

                            mtxLock(&mtxPacketAndDetails);
                            details.remoteSeq = packet.seqNum;
                            mtxUnlock(&mtxPacketAndDetails);
                            tellSenderSendACK(packet.seqNum, packet.isFinal);
                            memset(&packet, 0, sizeof(datagram));
                        }
                    }
                    else
                    {
                        setDataError();
                        isFinal = -1;
                        printf("error\n\n");
                    }
                }
            }
        }
    }

//    if(!getDataError())
//    {
//        printf("\n\n\nho ricevuto il sommo pacchetto finale\n\n\n");
//    }

}

void waitForFirstPacketListener(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen)
{
    while(receiveACK(socketfd, (struct sockaddr *) servAddr, &servLen) == 0){}

    struct pipeMessage ack;
    ack.isFinal = 100; // as we have decided
    ack.seqNum = 100;
    tellSenderSendACK(ack.seqNum, ack.isFinal);
}

//-----------------------------------------------------------------------------------------------MANAGE GLOBAL VARIABLES

int getGlobalSenderWait()
{
    mtxLock(&globalSenderWaitMtx);
    int r = globalSenderWait;
    mtxUnlock(&globalSenderWaitMtx);
    return r;
}

int getOpID()
{
    int opID;
    mtxLock(&syncMTX);
    opID = globalOpID;
    mtxUnlock(&syncMTX);
    return opID;
}

int getSeqNum()
{
    int seq;
    mtxLock(&mtxPacketAndDetails);
    seq = details.mySeq;
    mtxUnlock(&mtxPacketAndDetails);
    return seq;
}

int getSendBase()
{
    int base;
    mtxLock(&mtxPacketAndDetails);
    base = details.sendBase;
    mtxUnlock(&mtxPacketAndDetails);
    return base;
}

int getCurrentTimeSlot()
{
    int cts;
    mtxLock(&currentTSMTX);
    cts = currentTimeSlot;
    mtxUnlock(&currentTSMTX);
    return cts;
}

void incrementRounds()
{
    mtxLock(&roundsMTX);
    roundsSender++;
    mtxUnlock(&roundsMTX);
}

void setDataError()
{
    mtxLock(&syncMTX);
    dataError = 1;
    mtxUnlock(&syncMTX);
}

int getDataError()
{
    mtxLock(&syncMTX);
    int de = dataError;
    mtxUnlock(&syncMTX);
    return de;
}

void resetDataError()
{
    mtxLock(&syncMTX);
    dataError = 0;
    mtxUnlock(&syncMTX);
}

int getRTTseq()
{
    mtxLock(&timemtx);
    int rttseq = currentRTT.seqNum;
    mtxUnlock(&timemtx);
    return rttseq;
}

unsigned int randomGen()
{
    int seed1 = (int) time(NULL);
    int seed2 = getWheelPosition();
    int seed3 = getRTTseq();

    int seed5 = getSeqNum();
    int seed6 = getOpID();

    int multiplier = (seed1 + getSeqNum())%13;
    int addend = (seed2 + seed3*8)%7;

    unsigned int randomn = (unsigned int) (addend + multiplier*(seed1 + 2*seed2 + seed3 + seed5 + 2*seed6))%1000;
    return randomn;
}
//-----------------------------------------------------------------------------------------------------------------FINE


