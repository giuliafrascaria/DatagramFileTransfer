#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/sendfile.h>
#include "dataStructures.h"

#define WINDOWSIZE 256
#define TIMERSIZE 2048
#define NANOSLEEP 5000000

#define PULLDIR "/home/giogge/Documenti/clientHome/"


int timerSize = TIMERSIZE;
int nanoSleep = NANOSLEEP;
int windowSize = WINDOWSIZE;
int sendBase;
int pipeFd[2];
int pipeSendACK[2];
volatile int currentTimeSlot, globalOpID;
volatile int fdList, finalLen;
struct headTimer timerWheel[TIMERSIZE] = {NULL};
datagram packet;


void retransmitForPush(int fd, struct pipeMessage * rtx);
void pushSender();
void waitForFirstPacket();
void clientSendFunction();
void waitForFirstPacketPush();
void * clientListenFunction();
void sendSYN(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);
void sendSYN2(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);
void send_ACK(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, int synackSN);
void printfListInSTDOUT();
void pushListener();
void initProcess();
void startClientConnection(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);
void listenCycle();
void parseInput(char * s);
void listPullListener(int fd, int command);
int checkUserInput(char * buffer);
int waitForSYNACK(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);
int getFileLen(int fd);
char * stringParser(char * string);

// %%%%%%%%%%%%%%%%%%%%%%%    globali    %%%%%%%%%%%%%%%%%%%%%%%%%%

struct details details;
pthread_t listenThread, timerThread;
struct selectCell selectiveWnd[WINDOWSIZE];

pthread_mutex_t condMTX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condMTX2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t secondConnectionCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t senderCond = PTHREAD_COND_INITIALIZER;

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

//------------------------------------------------------------------------------------------------------------------MAIN
int main()
{

    printf("connection is starting... \n\n\n");
    clientSendFunction();
    exit(EXIT_SUCCESS);
}

void clientSendFunction()
{
    initPipe(pipeFd);
    initPipe(pipeSendACK);

    initProcess();

    struct pipeMessage rtxN;
    memset(&rtxN, 0, sizeof(struct pipeMessage));

    for(;;)
    {
        if(pthread_cond_wait(&senderCond, &condMTX2) != 0)
        {
            perror("error in sender cond wait");
        }

        printf("\n\nsono il sender e sono stato svegliato\n\n");
        if(packet.command == 0 || packet.command == 2)
        {
            sendDatagram(details.sockfd, &details.addr, details.Size, &packet);
            ACKandRTXcycle(details.sockfd, &details.addr, details.Size);
        }
        else
        {
            pushSender();
        }
    }
}

void initProcess()
{
    initWindow();

    // %%%%%%%%%%%%%%%%    thread       %%%%%%%%%%%%%%%%%

    createThread(&listenThread, clientListenFunction, NULL);

    createThread(&timerThread, timerFunction, NULL);

    // %%%%%%%%%%%%%%%%    variabili    %%%%%%%%%%%%%%%%%

    int socketfd;
    struct sockaddr_in senderServerAddress;
    socklen_t serverLen = sizeof(struct sockaddr_in);

    //----------------------------------------------------


    senderServerAddress = createStruct(4242); //create struct with server port
    socketfd = createSocket();

    if (fcntl(socketfd, F_SETFL, O_NONBLOCK) == -1)
    {
        perror("error in fcntl");
    }
    //printf("ho settato la socket a non bloccante\n");

    //printf("starting handshake procedure\n\n");
    startClientConnection( &senderServerAddress, serverLen, socketfd);
}

void startClientConnection(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd)
{
    sendSYN(servAddr, servLen, socketfd);
    int rcvSequence = waitForSYNACK(servAddr, servLen, socketfd);
    if(rcvSequence == -1)
    {
        perror("error in connection");
    }
    else if(rcvSequence > 0)//ho ricevuto il SYNACK
    {

        //salvo i dettagli di connessione in details
        details.addr = *servAddr;
        details.Size = servLen;
        details.sockfd = socketfd;

        //segnalo al listener
        sendSignalThread(&condMTX, &secondConnectionCond);

    }
    else //se ritorna -1 devo ritrasmettere
    {
        initWindow();
        startClientConnection(servAddr, servLen, socketfd);
    }

}

void * clientListenFunction()
{
    printf("listener thread attivato\n\n");

    details.Size2 = sizeof(struct sockaddr_in);
    details.sockfd2 = createSocket();

    if(pthread_cond_wait(&secondConnectionCond, &condMTX) != 0)
    {
        perror("error in cond wait");
    }

    printf("inizio seconda connessione\n\n");

    sendSYN2(&(details.addr), details.Size, details.sockfd2);
    waitForSYNACK(&(details.addr2), details.Size2, details.sockfd2);
    send_ACK(&(details.addr2), details.Size2, details.sockfd2, details.remoteSeq);

    listenCycle();
    return (EXIT_SUCCESS);
}

void listenCycle()
{
    char * s = malloc(512);
    int timeout = 0;
    int res = 0;
    if(s == NULL)
    {
        perror("error in malloc");
    }

    if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) == -1)
    {
        perror("error in fcntl");
    }

    for(;;)
    {
        res = 0;
        printf("insert command : \n");

        while(!res)
        {
            res = checkUserInput(s);
            if(res == -1)
            {
                perror("error in stdin read");
            }
            else if(res == 0)
            {
                if(usleep(100000) == -1)
                {
                    perror("error on usleep");
                }
                timeout++;
            }
            else
            {
                printf("processing request\n");

                parseInput(s);
                //sendSignalThread(&condMTX, &secondConnectionCond);
                timeout = 0;
                //provvisorio
                //poi mi devo mettere a sentire i dati ricevuti dalla socket
                //res = 0;
            }

            if(timeout == 120000)
            {
                perror("timeout on input listen");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void parseInput(char * s)
{
    if (strncmp(s,"list", 4) == 0)//---------------------------------------------------------------listener list command
    {
        char listFilename[17];

        memset(listFilename,0,17);
        strcpy(listFilename, "lsTempXXXXXX");
        fdList = mkstemp(listFilename);
        while(fdList == -1)
        {
            perror("1: error in list tempfile open");
            fdList = mkstemp(listFilename);
        }
        unlink(listFilename);

        listPullListener(fdList, 0);
    }
    else if (strncmp(s, "push", 4) == 0)//---------------------------------------------------------listener push command
    {
        printf("'push'\n");
        if (sscanf(s, "%*s %s", packet.content) == EOF) {
            perror("1: error in reading words from standard input, first sscanf push");
        }
        pushListener();
    }
    else if (strncmp(s, "pull", 4) == 0)//---------------------------------------------------------listener pull command
    {
        printf("'pull'\n");

        char * content = malloc(500);
        char * fileName;
        int fdPull;

        if (sscanf(s, "%*s %s", content) == EOF) {
            perror("1: error in reading words from standard input, first sscanf pull");
            free(s);
            free(content);
        }

        memset(packet.content, 0, 512);
        strcpy(packet.content, content);

        printf("richiesta di pull per il pacchetto %s\n", content);

        fileName = malloc(512);
        strcat(fileName, PULLDIR);
        strcat(fileName, content);

        printf("|    path : %s \n", fileName);

        if ((fdPull = open(fileName, O_CREAT | O_TRUNC | O_RDWR, 00777)) == -1)
        {
            perror("1: file already exists on push");
        }

        printf("|    file created with name : %s \n", content);

        listPullListener(fdPull, 2);
    }
    else if (strncmp(s, "quit", 4) == 0)//---------------------------------------------------------listener quit command
    {
        printf("quit'\n");
    }
    else if (strncmp(s, "help", 4) == 0)//---------------------------------------------------------listener help command
    {
        printf("'help\n");
    }
    else
    {
        printf("command does not exist, enter 'list', 'push', 'pull', 'help' or 'exit'\n");
    }
}

void listPullListener(int fd, int command)
{

    //---------------------proteggere con mutex
    packet.command = command;
    packet.isFinal = 1;
    packet.opID =  rand() % 2048;
    packet.seqNum = details.mySeq;
    //-----------------------------------------

    sendSignalThread(&condMTX2, &senderCond);

    //aspetto datagrammi
    getResponse(details.sockfd2, &(details.addr2), &(details.Size2), fd);

    if(command == 0)
    {
        printfListInSTDOUT();
    }

}

int checkUserInput(char * buffer)
{
    ssize_t res;
    res = read(STDIN_FILENO, buffer, 512);
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

void printfListInSTDOUT()
{
    printf("\n----------------------LIST-----------------------\n\n");

    off_t count = lseek(fdList, 0L, SEEK_END);
    while (count == -1) {
        perror("1: error in file size measurement\n");
        sleep(1);
        count = lseek(fdList, 0L, SEEK_END);
    }
    while(lseek(fdList, 0L, SEEK_SET) == -1){
        perror("1: error in lseek");
        lseek(fdList, 0L, SEEK_SET);
    }

    while (sendfile(STDOUT_FILENO, fdList, 0L, (size_t) count) == -1) {
        perror("error in sendfile");
        sendfile(STDOUT_FILENO, fdList, 0L, (size_t) count);
    }

    printf("\n------------------------------------------------\n\n");
    if (ftruncate(fdList, 0) == -1) {
        perror("0: error in truncating file");
    }

    if( close(fdList) == -1)
    {
        perror("0: error in list file close");
    }
}

void pushListener()
{
    //aspetto ack
    //---------------------proteggere con mutex
    packet.command = 1;
    packet.isFinal = 0;
    packet.opID =  rand() % 2048;
    globalOpID = packet.opID;
    packet.seqNum = details.mySeq;
    details.firstSeqNum = details.mySeq;
    //-----------------------------------------

    sendSignalThread(&condMTX2, &senderCond);
    waitForFirstPacketPush();
    waitForAckCycle(details.sockfd2, (struct sockaddr *) &details.addr2, &details.Size2);
    printf("--------------SONO USCITO-------------------\n\n\n\n");
}

void pushSender()
{
    int seqnum = details.mySeq, finalSeq = -1, isFinal = 0;
    ssize_t readByte;
    datagram sndPacket;
    struct pipeMessage rtx;

    int fd = open(packet.content, O_RDONLY);
    if(fd == -1){
        perror("error in open");
    }

    int len = getFileLen(fd);
    memset(sndPacket.content, 0, 512);
    char * s = malloc(100);
    if(s == NULL){
        perror("error in malloc");
    }
    s = stringParser(packet.content);
    if(sprintf(sndPacket.content, "%s %d", s, len) < 0)
    {
        perror("error in sprintf");
    }

    printf("sono arrivato fin qui, la stringa da inviare è %s con numero di sequenza iniziale : %d\n", sndPacket.content, seqnum);
    sndPacket.seqNum = seqnum;
    sndPacket.command = 1;
    sndPacket.isFinal = 1;
    sendDatagram(details.sockfd, &(details.addr), details.Size, &sndPacket);
    waitForFirstPacket();
    seqnum = details.mySeq;

    isFinal = 0;
    while(details.sendBase != finalSeq || isFinal == 0)
    {
        while(seqnum%WINDOWSIZE - details.sendBase > 256)
        {
            if(checkPipe(&rtx, pipeFd[0]) != 0)
            {
                retransmitForPush(fd, &rtx);
            }
        }
        if(isFinal == 0)
        {
            if (checkPipe(&rtx, pipeFd[0]) == 0)
            {
                memset(sndPacket.content, 0, 512);
                readByte = read(fd, sndPacket.content, 512);
                if (readByte < 512 && readByte >= 0)
                {
                    finalSeq = seqnum;
                    isFinal = 1;
                    printf("il pacchetto è finale (grandezza ultimo pacchetto : %d)\n\n\n\n", (int) readByte);
                }
                sndPacket.isFinal = (short) isFinal;
                sndPacket.ackSeqNum = details.remoteSeq;
                sndPacket.seqNum = seqnum;
                sndPacket.opID = globalOpID;
                //printf("ho inviato un pacchetto ackando %u\n", details.remoteSeq);
                sendDatagram(details.sockfd, &(details.addr), details.Size, &sndPacket);

                seqnum = details.mySeq;
            }
            else
                retransmitForPush(fd, &rtx);
        }
        else
        {
            if(checkPipe(&rtx, pipeFd[0]) != 0)
            {
                retransmitForPush(fd, &rtx);
            }
        }
    }
    memset(sndPacket.content, 0, 512);
    sndPacket.isFinal = -1;
    sendDatagram(details.sockfd, &(details.addr), details.Size, &sndPacket);
    printf("inviato il pacchetto definitivo con isFinal = -1 \n");
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
    return (int) len;
}

void waitForFirstPacket()
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
            datagram * packetRTX = rebuildDatagram(*pm);
            sendDatagram(details.sockfd, &details.addr, details.Size, packetRTX);
            memset(pm, 0, sizeof(struct pipeMessage));
            printf("\n\nritrasmetto\n");
        }
    }
}

void waitForFirstPacketPush()
{
    while(receiveACK(details.sockfd2, (struct sockaddr *) &details.addr2, &details.Size2) == 0){}

    handshake ack;
    ack.isFinal = 1;
    if(write(pipeSendACK[1], &ack, sizeof(handshake))==-1){
        perror("error in write on pipe");
    }
}

void retransmitForPush(int fd, struct pipeMessage * rtx)
{
    printf("ritrasmetto\n");
    datagram sndPacket;
    if(lseek(fd, 512*(rtx->seqNum - details.firstSeqNum), SEEK_SET) == -1){
        perror("errore in lseek");
    }
    if(read(fd, sndPacket.content, 512)==-1){
        perror("error in read");
    }

    sndPacket.isFinal = rtx->isFinal;
    sndPacket.ackSeqNum = details.remoteSeq;
    sndPacket.seqNum = rtx->seqNum;
    sndPacket.opID = globalOpID;
    sendDatagram(details.sockfd, &(details.addr), details.Size, &sndPacket);
}

char * stringParser(char * string)
{
    char * sToReturn  = malloc(512);
    char* start=strrchr(string,'/'); /* Find the last '/' */
    strcpy(sToReturn, start+1);
    return sToReturn;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% CONNESSIONE

void sendSYN(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd)
{
    handshake SYN;

    srandom((unsigned int)getpid());
    SYN.sequenceNum = (int) random() % 4096;

    sendBase = SYN.sequenceNum;

    // il prossimo seqnum utile
    details.remoteSeq = SYN.sequenceNum;

    sendACK(socketfd, &SYN, servAddr, servLen);
    sentPacket(SYN.sequenceNum, 0);


    printf("ho inviato il SYN. numero di sequenza : %d\n", SYN.sequenceNum);
}

void sendSYN2(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd)
{
    handshake SYN;

    SYN.sequenceNum = details.mySeq;
    SYN.ack = details.remoteSeq;

    sendACK(socketfd, &SYN, servAddr, servLen);
    sentPacket(SYN.sequenceNum, 0);


    printf("ho inviato il SYN2. numero di sequenza : %d e ack per il server %d\n", SYN.sequenceNum, SYN.ack);
}

int waitForSYNACK(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd)
{
    handshake SYNACK;
    int sockResult;
    struct pipeMessage rtxN;
    for(;;)
    {
        if(checkPipe(&rtxN, pipeFd[0]))
        {
            printf("devo ritrasmettere\n");
            return 0;
        }
        sockResult = checkSocketAck(servAddr, servLen, socketfd, &SYNACK);
        if(sockResult == -1)
        {
            perror("error in socket read");
            return -1;
        }
        if(sockResult == 1)
        {
            printf("SYNACK ricevuto. numero di sequenza : %d\n", SYNACK.sequenceNum);
            ackSentPacket(SYNACK.ack);
            details.remoteSeq = SYNACK.sequenceNum;//---------------------------------------------serve al syn2
            //--------------------------------------------INIT GLOBAL DETAILS
            return SYNACK.sequenceNum;
        }
    }
}

void send_ACK(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, int synackSN)
{
    handshake ACK;
    ACK.ack = synackSN;
    ACK.sequenceNum = details.remoteSeq;
    ACK.windowsize = windowSize;

    sendACK(socketfd, &ACK, servAddr, servLen);
    //sentPacket(ACK.sequenceNum, 0);
    printf("ACK finale inviato. Numero di sequenza : %d\n", ACK.sequenceNum);
}

