#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/sendfile.h>
//#include <sys/time.h>
#include "dataStructures.h"

#define WINDOWSIZE 2048
#define TIMERSIZE 2048
#define NANOSLEEP 10000

#define PULLDIR "/home/giogge/Documenti/clientHome/"
//#define PULLDIR "/home/dandi/exp/"


int timerSize = TIMERSIZE;
int nanoSleep = NANOSLEEP;
int windowSize = WINDOWSIZE;
int pipeFd[2];
int pipeSendACK[2];
volatile int globalTimerStop = 0;
volatile int globalOpID;
volatile int fdList;
struct RTTsample currentRTT; //presente in datagram.h

datagram packet;

int fdglob;


//void retransmitForPush(int fd, struct pipeMessage * rtx);
void pushSender();
void clientSendFunction();
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
void initProcessDetails();
void putDataInPacketPush(datagram * packet, int isFinal);
int checkUserInput(char * buffer);
int waitForSYNACK(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd);
int getFileLen(int fd);
char * stringParser(char * string);
void clientExitProc(struct details *details);


// %%%%%%%%%%%%%%%%%%%%%%%    globali    %%%%%%%%%%%%%%%%%%%%%%%%%%

struct details details;
pthread_t listenThread, timerThread;


pthread_mutex_t syncMTX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condMTX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condMTX2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtxPacketAndDetails = PTHREAD_MUTEX_INITIALIZER;
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
        condWaitSender(&condMTX2, &senderCond, 0);

        if(packet.command == 0 || packet.command == 2)
        {
            sendDatagram(details.sockfd, &details.addr, details.Size, &packet, 0);
            ACKandRTXcycle(details.sockfd, &details.addr, details.Size, packet.command);
        }
        else if(packet.command == 1)
        {
            pushSender();
        }
        else if(packet.command == 3)
        {
            //quit procedure
            /*printf("entro in clientExit\n");
            clientExitProc(details);

            pthread_join(listenThread, NULL);

            exit(EXIT_SUCCESS);*/
        }
    }
}

void initProcess()
{
    initWindow(0);

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

    while(readGlobalTimerStop() != 2){}
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
        mtxLock(&mtxPacketAndDetails);
        details.addr = *servAddr;
        details.Size = servLen;
        details.sockfd = socketfd;
        mtxUnlock(&mtxPacketAndDetails);

        //segnalo al listener
        sendSignalThread(&condMTX, &secondConnectionCond, 1);

    }
    else //se ritorna -1 devo ritrasmettere
    {
        initWindow(0);
        startClientConnection(servAddr, servLen, socketfd);
    }

}

void * clientListenFunction()
{
    mtxLock(&mtxPacketAndDetails);
    details.Size2 = sizeof(struct sockaddr_in);
    details.sockfd2 = createSocket();
    mtxUnlock(&mtxPacketAndDetails);

    condWaitSender(&condMTX, &secondConnectionCond, 1);

//    starting second connection

    sendSYN2(&(details.addr), details.Size, details.sockfd2);

    while(waitForSYNACK(&(details.addr2), details.Size2, details.sockfd2) == 0)
    {
        sendSYN2(&(details.addr), details.Size, details.sockfd2);
    }
    send_ACK(&(details.addr2), details.Size2, details.sockfd2, details.remoteSeq);

    mtxLock(&syncMTX);
    globalTimerStop = 0;
    mtxUnlock(&syncMTX);

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
        mtxLock(&syncMTX);
        globalTimerStop = 0;
        mtxUnlock(&syncMTX);
        resetDataError();

        memset(&packet, 0, sizeof(datagram));
        res = 0;
        printf("insert command : \n");
        struct timespec opStart, opEnd;

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
            else {
                //prendo un timestamp
                clock_gettime(CLOCK_MONOTONIC, &opStart);

                //processing request
                sendSignalTimer();
                parseInput(s);
                timeout = 0;

                clock_gettime(CLOCK_MONOTONIC, &opEnd);

                ssize_t len = lseek(fdglob, 0, SEEK_END) + 1;
                if (!getDataError()) {
                    if (len > 0) {
                        long mseconds = (opEnd.tv_nsec / 1000000 - opStart.tv_nsec / 1000000);
                        if (mseconds < 10000) {
                            printf("\n\n\n----------------------------------------------------------------\n");
                            printf("|\toperazione completata in %lu millisecondi  \n", mseconds);
                            printf("|\tdimensione del file: %d kB\n", (int) len / 1000);
                            printf("|\tvelocità media: %.3f MB/s  \n",
                                   (len) / mseconds + 0.001);
                            printf("----------------------------------------------------------------\n");
                            printf("\n\n");
                        } else {
                            printf("\n\n\n----------------------------------------------------------------\n");
                            printf("there was a problem on taking time for this operation\n\n");
                            printf("|\toperazione completata in ? millisecondi  \n");
                            printf("|\tdimensione del file: ? kB\n");
                            printf("|\tvelocità media: ? MB/s  \n");
                            printf("----------------------------------------------------------------\n");
                            printf("\n\n");
                        }
                    }
                }
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

        fdglob = fdList;
        unlink(listFilename);

        listPullListener(fdList, 0);
    }
    else if (strncmp(s, "push", 4) == 0)//---------------------------------------------------------listener push command
    {
//        printf("'push'\n");
        if (sscanf(s, "%*s %s", packet.content) == EOF) {
            perror("1: error in reading words from standard input, first sscanf push");
        }
        pushListener();
    }
    else if (strncmp(s, "pull", 4) == 0)//---------------------------------------------------------listener pull command
    {
//        printf("'pull'\n");

        char * content = malloc(500);
        if(content == NULL)
            perror("error on malloc");

        char * fileName;
        int fdPull;



        if (sscanf(s, "%*s %s", content) == EOF) {
            perror("1: error in reading words from standard input, first sscanf pull");
            free(s);
            free(content);
        }

        mtxLock(&mtxPacketAndDetails);
        memset(packet.content, 0, 512);
        strcpy(packet.content, content);
        mtxUnlock(&mtxPacketAndDetails);

//        printf("richiesta di pull per il pacchetto %s\n", content);

        fileName = malloc(512);



        if(sprintf(fileName, "%s%s", PULLDIR, content) == -1)
        {
            perror("error in filename");
        }

        printf("|    path : %s \n", fileName);

        if ((fdPull = open(fileName, O_CREAT | O_TRUNC | O_RDWR, 00777)) == -1)
        {
            perror("1: file already exists on push");
        }

        fdglob = fdPull;

        printf("|    file created with name : %s \n", content);

        listPullListener(fdPull, 2);
    }
    else if (strncmp(s, "quit", 4) == 0)//---------------------------------------------------------listener quit command
    {
        printf("quit'\n");
        /*
         * printf("Do you want to close the connection? y = yes, n = no \n");
                int ans = getchar();
                if ((char) ans == 'y') {
                    printf("you chose to close the connection\n");

                    printf("inizio la procedura di chiusura\n");

                    packet.command = 3;

                    while(pthread_cond_signal(&condTIM)!= 0)
                    {
                        perror("error in pThread signal");
                        sleep(1);
                        hmt = nonFatalErr(details, hmt, 1);
                    }
                    hmt = 0;
                    while(pthread_cond_signal(&condGS)!= 0)
                    {
                        perror("1: error on cond_signal");
                        sleep(1);
                        hmt = nonFatalErr(details, hmt, 1);
                    }
                    hmt = 0;
                    //pthread_mutex_unlock(&mtxGlobalSleep);

                    ACKwait(&privateListenServer);

                    pthread_exit(NULL); //è per dire, non bisogna farlo qui l'exit success
                }*/
    }
    else if (strncmp(s, "help", 4) == 0)//---------------------------------------------------------listener help command
    {
        printf("\n\n\n");
        printf("DFT\n\n\n");
        printf("'Datagram File Transfer' is a client-server application for remote data storage\n");
        printf("A list of commands with their own behavior can be found below\n\n");
        printf("list : Show each file on server\n");
        printf("pull : Get a copy of a file on your own pc\n");
        printf("push : Save a copy of a file on a remote server\n\n\n");
    }
    else
    {
        printf("command does not exist, enter 'list', 'push', 'pull', 'help' or 'exit'\n");
    }
}

void listPullListener(int fd, int command)
{

    mtxLock(&mtxPacketAndDetails);
    packet.command = command;
    packet.isFinal = 1;
    packet.opID =  rand() % 2048;

    mtxLock(&syncMTX);
    globalOpID = packet.opID;
    mtxUnlock(&syncMTX);

    packet.seqNum = details.mySeq;

    details.firstSeqNum = details.remoteSeq + 1;
//    printf("aspetto datagrammi, e il first seqnum atteso è %d\n", details.firstSeqNum);
    mtxUnlock(&mtxPacketAndDetails);
    //-----------------------------------------

    sendSignalThread(&condMTX2, &senderCond, 0);

    getResponse(details.sockfd2, &(details.addr2), &(details.Size2), fd, command);

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
        //sendfile(STDOUT_FILENO, fdList, 0L, (size_t) count);
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
    //---------------------proteggere con mutex
    mtxLock(&mtxPacketAndDetails);
    packet.command = 1;
    packet.isFinal = 0;
    packet.opID =  rand() % 2048;

    packet.seqNum = details.mySeq;
    details.firstSeqNum = details.mySeq;

    globalOpID = packet.opID;
    mtxUnlock(&mtxPacketAndDetails);




    //-----------------------------------------

    mtxLock(&mtxPacketAndDetails);
    int fd = open(packet.content, O_RDONLY);
    if(fd == -1){
        perror("error in open");
        mtxUnlock(&mtxPacketAndDetails);
    }
    else {
        fdglob = fd;
        mtxUnlock(&mtxPacketAndDetails);
        sendSignalThread(&condMTX2, &senderCond, 0);
        waitForFirstPacketListener(details.sockfd2, &(details.addr2), details.Size2);
        waitForAckCycle(details.sockfd2, (struct sockaddr *) &details.addr2, &details.Size2);
        printf("--------------------------------------------\n\n\n\n");
    }
}

void pushSender()
{
    int finalSeq = -100, isFinal = 0, alreadyDone = 0;
    ssize_t readByte;
    datagram sndPacket;
    struct pipeMessage rtx;
    struct pipeMessage finalAck;

    initProcessDetails();

    int thereIsAnError = 0;
    int len = getFileLen(fdglob);
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


    putDataInPacketPush(&sndPacket, 1);
    if(memcpy(&packet, &sndPacket, sizeof(datagram)) == NULL)
    {
        perror("error in memcpy");
    }
    sendDatagram(details.sockfd, &(details.addr), details.Size, &sndPacket, 0);
    //printWindow();
    waitForFirstPacketSender(details.sockfd, &(details.addr), details.Size);

//    while(((getSendBase()%WINDOWSIZE) != ((finalSeq+1)%WINDOWSIZE)) && (thereIsAnError == 0))
//    {
        while(isFinal == 0) {
            while (!canISend() && !getDataError()) {
                if (checkPipe(&rtx, pipeFd[0]) != 0) {
//                  retransmission
                    sndPacket = rebuildDatagram(fdglob, rtx, 1);
                    sendDatagram(details.sockfd, &(details.addr), details.Size, &sndPacket, 1);
                }
            }
            if (!getDataError()) {
                if (checkPipe(&rtx, pipeFd[0]) == 0) {
                    memset(sndPacket.content, 0, 512);
                    readByte = read(fdglob, sndPacket.content, 512);
                    if (readByte < 0) {
                        perror("error in read");
                    } else if (readByte != 0) {
                        sndPacket.packetLen = readByte;

                        putDataInPacketPush(&sndPacket, isFinal);

                        if (sndPacket.seqNum < details.firstSeqNum && alreadyDone == 0) {
                            incrementRounds();
                            alreadyDone++;
                        } else if (packet.seqNum >= details.firstSeqNum && alreadyDone > 0) {
                            alreadyDone = 0;
                        }

                        sendDatagram(details.sockfd, &(details.addr), details.Size, &sndPacket, 0);
                    } else {
                        isFinal = 1;
                        finalSeq = getSeqNum() - 1;
                    }

                } else {
//                      retransmission
                        sndPacket = rebuildDatagram(fdglob, rtx, 1);
                        sendDatagram(details.sockfd, &(details.addr), details.Size, &sndPacket, 1);
                }
            }
            else
            {
                isFinal = 1;
                thereIsAnError = 1;
                printf("exit_failure\n");
            }
        }
    if(!thereIsAnError)
    {
        while (getSendBase() % WINDOWSIZE != finalSeq % WINDOWSIZE) {
            if (checkPipe(&rtx, pipeFd[0]) != 0) {
//              retransmission
                sndPacket = rebuildDatagram(fdglob, rtx, sndPacket.command);
                sendDatagram(details.sockfd2, &(details.addr2), details.Size2, &sndPacket, 1);
            }
        }
//      sending final packet
        mtxLock(&mtxPacketAndDetails);
        memset(sndPacket.content, 0, 512);
        mtxUnlock(&mtxPacketAndDetails);
        putDataInPacketPush(&sndPacket, -1);
        sendDatagram(details.sockfd, &(details.addr), details.Size, &sndPacket, 0);
//        packet with isFinal = -1 sent
        while (checkPipe(&finalAck, pipeSendACK[0]) == 0) {
            if (checkPipe(&rtx, pipeFd[0]) != 0) {
//              retransmission
                sendDatagram(details.sockfd2, &(details.addr2), details.Size2, &sndPacket, 1);
            }
        }
    }
}

void initProcessDetails()
{
    resetDataError();
    mtxLock(&mtxPacketAndDetails);
    initWindow(1);
    details.mySeq= rand() %2048;
    details.sendBase = details.mySeq;
    details.firstSeqNum = details.sendBase + 1;
    globalOpID = rand() %2048;
    mtxUnlock(&mtxPacketAndDetails);
}

void putDataInPacketPush(datagram * myPacket, int isFinal)
{
    myPacket->isFinal = (short) isFinal;
    myPacket->seqNum = getSeqNum();
    myPacket->command = 1;
    mtxLock(&syncMTX);
    myPacket->opID = globalOpID;
    mtxUnlock(&syncMTX);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% CONNESSIONE

void sendSYN(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd)
{
    handshake SYN;
    sendSignalTimer();

    srandom((unsigned int)getpid());
    SYN.sequenceNum = (int) random() % 4096;

    mtxLock(&mtxPacketAndDetails);
    details.sendBase = SYN.sequenceNum;
    details.mySeq = SYN.sequenceNum;
    //details.remoteSeq = SYN.sequenceNum;
    mtxUnlock(&mtxPacketAndDetails);

    sendACK(socketfd, &SYN, servAddr, servLen);
    sentPacket(SYN.sequenceNum, 0);

//  SYN sent
}

void sendSYN2(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd)
{
    handshake SYN;

    mtxLock(&mtxPacketAndDetails);
    SYN.sequenceNum = details.mySeq;
    SYN.ack = details.remoteSeq;
    mtxUnlock(&mtxPacketAndDetails);

    sendACK(socketfd, &SYN, servAddr, servLen);
    sentPacket(SYN.sequenceNum, 0);


//    SYN2 sent
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
//          retransmission
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
//            synack received
            ackSentPacket(SYNACK.ack);
            mtxLock(&mtxPacketAndDetails);
            details.remoteSeq = SYNACK.sequenceNum;
            mtxUnlock(&mtxPacketAndDetails);
            return SYNACK.sequenceNum;
        }
    }
}

void send_ACK(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, int synackSN)
{
    handshake ACK;
    ACK.ack = synackSN;

    mtxLock(&mtxPacketAndDetails);
    ACK.sequenceNum = details.remoteSeq;
    mtxUnlock(&mtxPacketAndDetails);

    sendACK(socketfd, &ACK, servAddr, servLen);
//    printf("ACK finale inviato. Numero di sequenza : %d ackando il pacchetto %d\n", ACK.sequenceNum, ACK.ack);
}


    //deallocare risorse, chiudere tutti file descriptor
    //il server deve notificare al thread timer che un processo client è terminato
    //avviare disconnessione dal server

    printf("mando il messaggio di chiusura connessione\n");
    if (write(details->sockfd, (char *) &packet, sizeof(datagram)) < 0)
    {
        perror("error in write");
    }
    printf("sono arrivato dopo della write \n");

    printf("sono arrivato dopo la read \n");

    //IMPLEMENTA RITRASMISSIONE

    //devo aspetare il listener del client per poter uscire

}