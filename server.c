#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <dirent.h>
#include "server.h"


//#define NANOSLEEP 10000


//#define LSDIR "/home/giogge/Documenti/DFThome/"
//#define LSDIR "/home/dandi/Downloads/"

int timerSize = TIMERSIZE;
int nanoSleep = NANOSLEEP;
int windowSize = WINDOWSIZE;
int pipeFd[2];
int pipeSendACK[2];
datagram packet;
struct RTTsample currentRTT; //presente in datagram.h

volatile int globalOpID, globalTimerStop = 0;


struct details details;

pthread_t timerThread;
pthread_t senderThread;

pthread_mutex_t syncMTX = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t condMTX = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t secondConnectionCond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t condMTX2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t senderCond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t mtxPacketAndDetails = PTHREAD_MUTEX_INITIALIZER;


void sendCycle(int command);
void listenCycle();
int waitForAck(int socketFD, struct sockaddr_in * clientAddr);
void terminateConnection(int socketFD, struct sockaddr_in * clientAddr, socklen_t socklen, struct details *cl);
void sendSYNACK(int privateSocket, socklen_t socklen , struct details * cl);
void sendSYNACK2(int privateSocket, socklen_t socklen , struct details * cl);
int waitForAck2(int socketFD, struct sockaddr_in * clientAddr);
void finishHandshake();
int ls();
int receiveFirstDatagram(char * content);

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%        <<-------------<   FUNZIONI

void listenFunction(int socketfd, struct details * details, handshake * message)
{
    initWindow(0);

    char buffer[100];
    printf("richiesta dal client %s\n\n\n", inet_ntop(AF_INET, &((details->addr).sin_addr), buffer, 100));
    printf("Received packet from %s, port %d\n",
                        inet_ntoa(details->addr.sin_addr), ntohs(details->addr.sin_port));


    initPipe(pipeFd);
    initPipe(pipeSendACK);

    createThread(&timerThread, timerFunction, NULL);
    createThread(&senderThread, sendFunction, NULL);
    startServerConnection(details, socketfd, message);

    listenCycle();
}

void * sendFunction()
{
    condWaitSender(&condMTX, &secondConnectionCond, 1);
    startSecondConnection(&details, details.sockfd2);
    finishHandshake();

    //---------------------------------------------------------------------scheletro di sincronizzazione listener sender
    for(;;)
    {
        mtxLock(&syncMTX);
        globalTimerStop = 0;
        mtxUnlock(&syncMTX);
        condWaitSender(&condMTX2, &senderCond, 0);
        //-----------------------------------------------------------------------------------------------------------------
        if(packet.command == 0)
        {
            mtxLock(&mtxPacketAndDetails);
            details.sendBase = details.mySeq;
            details.firstSeqNum = details.mySeq;
            mtxUnlock(&mtxPacketAndDetails);

            sendCycle(0);
        }
        else if(packet.command == 2)
        {
            mtxLock(&mtxPacketAndDetails);
            details.sendBase = details.mySeq;
            details.firstSeqNum = details.mySeq;
            printf("pull : %s\n\n", packet.content);
            mtxUnlock(&mtxPacketAndDetails);

            sendCycle(2);
        }
        else if(packet.command == 1){
            printf("push\n");
            ACKandRTXcycle(details.sockfd2, &details.addr2, details.Size2, 1);
        }
        else if(packet.command == 3)
        {
            datagram TERMPACKET;
            TERMPACKET.ackSeqNum = packet.seqNum;
            TERMPACKET.seqNum = getSeqNum();
            TERMPACKET.isFinal = -1;
            printf("invio conferma di chiusura connessione\n");
            sendDatagram(details.sockfd2, &details.addr2, details.Size2, &TERMPACKET, 0);
            printf("controllo ritrasmissioni\n");
            waitForFirstPacketSender(details.sockfd2, &details.addr2, details.Size2);
        }
    }

}

void finishHandshake()
{
    //aspetto ultimo ack
    int res = waitForAck2(details.sockfd2, &(details.addr2));
    if(res == 0)
    {
        //ritrasmetto
        sendSYNACK2(details.sockfd2, details.Size2, &details);
        finishHandshake();
    }
    else
    {
        //ho finito la connessione, aspetto che mi svegli il listener
        mtxLock(&syncMTX);
        globalTimerStop = 0;
        mtxUnlock(&syncMTX);
    }
}

void listenCycle()
{
    //------------------------------------------------------------------------------------------------------------------
    for(;;)
    {
        mtxLock(&syncMTX);
        globalTimerStop = 0;
        mtxUnlock(&syncMTX);

        printf("\nWaiting for commands\n");
        memset(&packet, 0, sizeof(datagram));
        int res = 0;
        int timeout = 0;
        while(!res)
        {
            mtxLock(&mtxPacketAndDetails);
            res = checkSocketDatagram(&(details.addr), details.Size, details.sockfd, &packet);
            mtxUnlock(&mtxPacketAndDetails);
            if(res == -1)
            {
                perror("error in socket read");
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
                resetDataError();
                sendSignalTimer();

                mtxLock(&mtxPacketAndDetails);
                details.remoteSeq = packet.seqNum;
                mtxUnlock(&mtxPacketAndDetails);


                mtxLock(&syncMTX);
                globalOpID = packet.opID;
                mtxUnlock(&syncMTX);

                sendSignalThread(&condMTX2, &senderCond, 0);
                printf("Request received, command = %d\n\n", packet.command);
                if(packet.command == 0 || packet.command == 2)
                {
                    waitForAckCycle(details.sockfd, (struct sockaddr *) &details.addr, &details.Size);
                }
                else if (packet.command == 1)
                {
                    int fd = receiveFirstDatagram(packet.content);
                    tellSenderSendACK(packet.seqNum, packet.isFinal);
                    getResponse(details.sockfd, &(details.addr), &(details.Size), fd, 1);
                }
                else if(packet.command == 3)
                {
                    printf("aspetto ack\n");
                    waitForAckCycle(details.sockfd, (struct sockaddr *) &details.addr, &details.Size);
                    printf("aspetto secondo ack\n");
                    tellSenderSendACK(100, 100);
                    printf("fine\n");
                    exit(EXIT_SUCCESS);
                }

                timeout = 0;
            }

            if(timeout == 120000)
            {
                perror("timeout on server listen");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void startServerConnection(struct details * cl, int socketfd, handshake * message)
{
    //chiudo la socket pubblica nel processo figlio
    closeFile(socketfd);

    //apro la socket dedicata al client su una porta casuale
    int privateSocket;
    privateSocket = createSocket();
    socklen_t socklen = sizeof(struct sockaddr_in);

    //collego una struct legata a una porta effimera, dedicata al client
    struct sockaddr_in serverAddress;
    //mi metto su una porta effimera, indicandola con sin_port = 0
    serverAddress = createStruct(0); //create struct with ephemeral port

    //per il client rimango in ascolto su questa socket
    bindSocket(privateSocket, (struct sockaddr *) &serverAddress, socklen);

    mtxLock(&mtxPacketAndDetails);
    details.remoteSeq = (message->sequenceNum);
    mtxUnlock(&mtxPacketAndDetails);

    //mando il datagramma ancora senza connettermi
    while(readGlobalTimerStop() != 2){}

    sendSYNACK(privateSocket, socklen, cl);
    terminateConnection(privateSocket, &(cl->addr), socklen, cl);

}

void startSecondConnection(struct details * cl, int socketfd)
{
    //chiudo la socket pubblica nel processo figlio
    closeFile(socketfd);

    //apro la socket dedicata al client su una porta casuale
    mtxLock(&mtxPacketAndDetails);
    details.sockfd2 = createSocket();
    details.Size2 = sizeof(struct sockaddr_in);
    mtxUnlock(&mtxPacketAndDetails);

    //collego una struct legata a una porta effimera, dedicata al client
    struct sockaddr_in serverAddress;
    //mi metto su una porta effimera, indicandola con sin_port = 0
    serverAddress = createStruct(0); //create struct with ephemeral port
    mtxLock(&mtxPacketAndDetails);
    details.addr2 = serverAddress;
    mtxUnlock(&mtxPacketAndDetails);

    //per il client rimango in ascolto su questa socket
    bindSocket(details.sockfd2, (struct sockaddr *) &(details.addr2), details.Size2);

    //mando il datagramma ancora senza connettermi
    sendSYNACK2(details.sockfd2, details.Size2, cl);

}

void terminateConnection(int socketFD, struct sockaddr_in * clientAddr, socklen_t socklen, struct details *cl )
{
    int rcvSequence = waitForAck(socketFD, clientAddr);
    if(rcvSequence == -1)
    {
        perror("error in connection");
    }
    else if(rcvSequence > 0)
    {
        //printf("ACK received, sequence num : %d. end first connection\n", rcvSequence );

        printf("Second data stream from %s, port %d\n",
               inet_ntoa(clientAddr->sin_addr), ntohs(clientAddr->sin_port));
        //avvio il thread listener per connettersi su una nuova socket
        //cond signal e il listener mi manda un secondo SYNACK, chiudendo socket eccetera

        sendSignalThread(&condMTX, &secondConnectionCond, 1);

    }
    else //se ritorna 0 devo ritrasmettere
    {
        sendSYNACK(socketFD, socklen , cl);
        terminateConnection(socketFD, clientAddr, socklen, cl);
    }
}

int waitForAck(int socketFD, struct sockaddr_in * clientAddr)
{
    if (fcntl(socketFD, F_SETFL, O_NONBLOCK) == -1)
    {
        perror("error in fcntl");
    }
    socklen_t slen = sizeof(struct sockaddr_in);
    handshake ACK;
    struct pipeMessage rtxN;
    int sockResult;
    for(;;)
    {
        if(checkPipe(&rtxN, pipeFd[0]))
        {
            //ritrasmissione
            return 0;
        }
        sockResult = checkSocketAck(clientAddr, slen, socketFD, &ACK);
        if (sockResult == -1)
        {
            perror("error in socket read");
            return -1;
        }
        if (sockResult == 1)
        {
            //printf("ACK received\n");

            mtxLock(&mtxPacketAndDetails);
            details.addr = *clientAddr;
            details.Size = slen;
            details.sockfd = socketFD;
            details.remoteSeq = ACK.sequenceNum;
            mtxUnlock(&mtxPacketAndDetails);

            ackSentPacket(ACK.ack);
            return ACK.sequenceNum;
        }
    }
}

int waitForAck2(int socketFD, struct sockaddr_in * clientAddr)
{
    if (fcntl(socketFD, F_SETFL, O_NONBLOCK) == -1)
    {
        perror("error in fcntl");
    }
    socklen_t slen = sizeof(struct sockaddr_in);
    handshake ACK;
    struct pipeMessage rtxN;
    int sockResult;
    for(;;)
    {
        sockResult = checkSocketAck(clientAddr, slen, socketFD, &ACK);
        if (sockResult == -1)
        {
            perror("error in socket read");

        }
        if (sockResult == 1)
        {
            ackSentPacket(ACK.ack);
            return ACK.sequenceNum;
        }
        else if(checkPipe(&rtxN, pipeFd[0]))
        {
            //ritrasmissione
            return 0;
        }
    }
}

void sendSYNACK(int privateSocket, socklen_t socklen , struct details * cl)
{
    sendSignalTimer();
    handshake SYN_ACK;
    srandom((unsigned int)getpid());
    SYN_ACK.sequenceNum = rand() % 4096;


    sendSignalTimer();

    mtxLock(&mtxPacketAndDetails);
    details.mySeq = SYN_ACK.sequenceNum;
    SYN_ACK.ack = details.remoteSeq;
    details.sendBase = SYN_ACK.sequenceNum;
    mtxUnlock(&mtxPacketAndDetails);

    sendACK(privateSocket, &SYN_ACK, &(cl->addr), socklen);

    sentPacket(SYN_ACK.sequenceNum, 0);
    //printf("SYNACK sent : %d\n", SYN_ACK.sequenceNum);

}

void sendSYNACK2(int privateSocket, socklen_t socklen , struct details * cl)
{
    handshake SYN_ACK;

    mtxLock(&mtxPacketAndDetails);
    SYN_ACK.sequenceNum = details.mySeq;
    SYN_ACK.ack = details.remoteSeq;
    mtxUnlock(&mtxPacketAndDetails);

    sendACK(privateSocket, &SYN_ACK, &(cl->addr), socklen);

    sentPacket(SYN_ACK.sequenceNum, 0);
    //printf("second SYNACK sent : %d\n", SYN_ACK.sequenceNum);

}

int ls()
{

    char listFilename[17];
    memset(listFilename,0,17);
    strcpy(listFilename, "lsTempXXXXXX");

    char * array[1000];
    int count = 0;

    int fd = mkstemp(listFilename);
    while(fd == -1)
    {
        perror("1: error in list tempfile open");
        fd = mkstemp(listFilename);

    }
    unlink(listFilename);


    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (LSDIR)) != NULL)
    {
        while ((ent = readdir (dir)) != NULL)
        {
            if((ent->d_name)[0] != '.')
            {
                if((array[count] = malloc(500)) == NULL)
                    perror("error in malloc");

                strcpy(array[count], ent->d_name);

                count++;
            }
        }
        closedir (dir);
    }
    else {
        perror("error on directory open");
        return -1;
    }

    int i, j;
    char * temp;
    if((temp = malloc(500)) == NULL){
        perror("error on malloc temp");
    }

    for(i=0; i < count ; i++){
        for(j=i+1; j< count; j++)
        {
            if(strcmp(array[i],array[j]) > 0)
            {
                strcpy(temp,array[i]);
                strcpy(array[i],array[j]);
                strcpy(array[j],temp);
                memset(temp, 0, 100);
            }
        }
    }
    printf("\n\n\n");

    for (int k = 0; k < count; k++)
    {
        dprintf(fd, "%s\n", array[k]);
    }
    lseek(fd, 0, SEEK_SET);

    //--------------------------------------------------------------------------------------------------------




    //--------------------------------------------------------------------------------------------------------
    return fd;
}

int receiveFirstDatagram(char * content)
{
    int fd;
    char * fileName;
    char *s = malloc(100);
    if (s == NULL)
    {
        perror("error in malloc");
    }

    mtxLock(&syncMTX);
    if (sscanf(content, "%s", s) == EOF)
    {
        perror("1: error in reading words from standard input, first sscanf push");
    }
    mtxUnlock(&syncMTX);
    //GIULIA
    fileName = malloc(512);
    if(fileName == NULL)
    {
        perror("error in malloc");
    }
    if(sprintf(fileName, "%s%s", LSDIR, s) == -1)
    {
        perror("error in srintf");
    }

    printf("file to open: %s\n", fileName);

    if((fd = open(fileName, O_RDWR | O_TRUNC | O_CREAT, 0777)) == -1)
    {
        perror("error in opening/creating file");
    }

    if ((lseek(fd, 0L, SEEK_SET)) == -1)
    {
        perror("error on lseek");
    }
    mtxLock(&mtxPacketAndDetails);
    details.firstSeqNum = packet.seqNum + 1;
    mtxUnlock(&mtxPacketAndDetails);
    return fd;
}

void sendCycle(int command)
{
    int fd;
    datagram sndPacket;
    int errorSendDatagram = 0;

    if(command == 0)
    {
        fd = ls();
    }
    else
    {
        //bisogna fare la pull del file scritto nel pacchetto
        char * absolutepath = malloc(512);
        if (absolutepath == NULL)
        {
            perror("error in path malloc");
        }

        if(sprintf(absolutepath, "%s%s", LSDIR, packet.content) == -1)
        {
            perror("error on sprintf");
        }
        printf("sprintf result %s\n\n", absolutepath);
        fd = openFile(absolutepath);
    }
    if(fd != -1) {
        memset(sndPacket.content, 0, 512);

        mtxLock(&mtxPacketAndDetails);
        details.firstSeqNum = details.mySeq;
        mtxUnlock(&mtxPacketAndDetails);

        int finalSeq = -1;
        int isFinal = 0;
        ssize_t readByte;
        int thereIsAnError = 0;
        struct pipeMessage rtx;
        struct pipeMessage finalAck;

        while (isFinal == 0)
        {
            while (!canISend() && !getDataError()) {
                if (checkPipe(&rtx, pipeFd[0]) != 0) {
                    sndPacket = rebuildDatagram(fd, rtx, sndPacket.command);
                    sendDatagram(details.sockfd2, &(details.addr2), details.Size2, &sndPacket, 1);
                }
            }

            if (!getDataError()) {
                if (checkPipe(&rtx, pipeFd[0]) == 0) {
                    memset(sndPacket.content, 0, 512);
                    readByte = read(fd, sndPacket.content, 512);
                    if (readByte == -1) {
                        perror("error in file read");
                        isFinal = 1;
                        thereIsAnError = 1;
                        errorSendDatagram = 1;
                    } else if (readByte != 0) {
                        sndPacket.packetLen = readByte;
                        sndPacket.isFinal = (short) isFinal; //mi sa che non serve

                        mtxLock(&mtxPacketAndDetails);
                        sndPacket.ackSeqNum = details.remoteSeq;
                        mtxUnlock(&mtxPacketAndDetails);

                        sndPacket.seqNum = getSeqNum();

                        sndPacket.opID = getOpID();
                        sendDatagram(details.sockfd2, &(details.addr2), details.Size2, &sndPacket, 0);
                    } else {
                        isFinal = 1;
                        finalSeq = getSeqNum();
                    }
                } else {
                    //ritrasmetti
                    sndPacket = rebuildDatagram(fd, rtx, sndPacket.command);
                    sendDatagram(details.sockfd2, &(details.addr2), details.Size2, &sndPacket, 1);
                }
            } else {
                isFinal = 1;
                thereIsAnError = 1;
            }
        }
        if (!thereIsAnError) {
            while (getSendBase() % WINDOWSIZE != finalSeq % WINDOWSIZE) {
                if (checkPipe(&rtx, pipeFd[0]) != 0) {
//                  retansmission
                    sndPacket = rebuildDatagram(fd, rtx, sndPacket.command);
                    sendDatagram(details.sockfd2, &(details.addr2), details.Size2, &sndPacket, 1);
                }
            }
            memset(sndPacket.content, 0, 512);
            sndPacket.isFinal = -1;
            sndPacket.seqNum = getSeqNum();
            sendDatagram(details.sockfd2, &(details.addr2), details.Size2, &sndPacket, 0);
            printf("isFinal = -1 sent : %d \n", sndPacket.seqNum);
            while (checkPipe(&finalAck, pipeSendACK[0]) == 0) {
                if (checkPipe(&rtx, pipeFd[0]) != 0) {
                    //ritrasmissione
                    sendDatagram(details.sockfd2, &(details.addr2), details.Size2, &sndPacket, 1);
                    printf("isFinal = -1 sent on retrasmission \n");
                }
            }
        }
    }
    else {
        errorSendDatagram = 1;
    }
    if (errorSendDatagram == 1)
    {
        memset(sndPacket.content, 0, 512);
        sndPacket.isFinal = -2;
        sndPacket.seqNum = getSeqNum();
        sndPacket.opID = getOpID();
        sendDatagram(details.sockfd2, &(details.addr2), details.Size2, &sndPacket, 0);
        setDataError();
        printf("exit_failure (sendCycle)\n");
    }
}
