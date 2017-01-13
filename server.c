#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <dirent.h>
#include "server.h"

#define WINDOWSIZE 2048
#define TIMERSIZE 2048
#define NANOSLEEP 1000

#define LSDIR "/home/giogge/Documenti/serverHome/"
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

    initPipe(pipeFd);
    initPipe(pipeSendACK);

    createThread(&timerThread, timerFunction, NULL);
    createThread(&senderThread, sendFunction, NULL);

    printf("finita la creazione dei thread\n");

    startServerConnection(details, socketfd, message);

    listenCycle();
}

void * sendFunction()
{

    printf("sender thread attivato\n\n");
//    if(pthread_cond_wait(&secondConnectionCond, &condMTX) != 0)
//    {
//        perror("error in cond wait");
//    }
    condWaitSender(&condMTX, &secondConnectionCond, 1);
    printf("sono dopo la cond wait\n\n");
    startSecondConnection(&details, details.sockfd2);
    finishHandshake();

    //---------------------------------------------------------------------scheletro di sincronizzazione listener sender
    for(;;)
    {
        mtxLock(&syncMTX);
        globalTimerStop = 0;
        mtxUnlock(&syncMTX);

//        printf("mi metto in cond wait\n");
//        if(pthread_cond_wait(&senderCond, &condMTX2) != 0)
//        {
//            perror("error in cond wait");
//        }
        condWaitSender(&condMTX2, &senderCond, 0);
        printf("sono dopo la seconda cond wait, giunse un pacchetto con comando %d\n\n", packet.command);

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
            printf("\n\nè una pull di %s\n\n", packet.content);
            mtxUnlock(&mtxPacketAndDetails);

            sendCycle(2);
        }
        else if(packet.command == 1){
            printf("è una push\n");
            ACKandRTXcycle(details.sockfd2, &details.addr2, details.Size2, 1);
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
        printf("fine, sono pronto\n\n");
    }
}

void listenCycle()
{
    printf("inizio il ciclo di ascolto\n");

    //datagram packet;
    //------------------------------------------------------------------------------------------------------------------
    for(;;)
    {
        mtxLock(&syncMTX);
        globalTimerStop = 0;
        mtxUnlock(&syncMTX);

        printf("\nsono il listener e ricomincio il ciclo\n");
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
                printf("è arrivata una richiesta : numero di sequenza ricevuto = %u\n", packet.seqNum);
                resetDataError();
                sendSignalTimer();

                mtxLock(&mtxPacketAndDetails);
                details.remoteSeq = packet.seqNum;
                mtxUnlock(&mtxPacketAndDetails);


                mtxLock(&syncMTX);
                globalOpID = packet.opID;
                printf("pacchetto ricevuto opID = %d\n", globalOpID);
                mtxUnlock(&syncMTX);

                sendSignalThread(&condMTX2, &senderCond, 0);
                printf("RICHIESTA CON NUMERO DI COMANDO = %d\n\n", packet.command);
                if(packet.command == 0 || packet.command == 2)
                {
                    //waitForFirstPacketListener(details.sockfd, &(details.addr), details.Size);
                    waitForAckCycle(details.sockfd, (struct sockaddr *) &details.addr, &details.Size);
                }
                else if (packet.command == 1)
                {
                    int fd = receiveFirstDatagram(packet.content);
                    tellSenderSendACK(packet.seqNum, packet.isFinal);
                    getResponse(details.sockfd, &(details.addr), &(details.Size), fd, 1);
                }

                timeout = 0;
            }

            if(timeout == 120000)
            {
                perror("timeout on server listen");
                exit(EXIT_FAILURE);
            }
        }
        //arrivo qui quando ho ricevuto cose, chiamo una funzione tipo controllaComando() e sveglia il sender e il timer
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
    printf("ho creato la struct dedicata\n");

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
    printf("ho creato la seconda struct dedicata\n");
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
        printf("ACK ricevuto con numero di sequenza : %d. fine connessione parte 1\n", rcvSequence );
        //avvio il thread listener per connettersi su una nuova socket
        //cond signal e il listener mi manda un secondo SYNACK, chiudendo socket eccetera

        sendSignalThread(&condMTX, &secondConnectionCond, 1);
        //in teoria ora posso connettere la socket
    }
    else //se ritorna 0 devo ritrasmettere
    {
        printf("sto ritrasmettendo\n");

        sendSYNACK(socketFD, socklen , cl);
        terminateConnection(socketFD, clientAddr, socklen, cl);
    }
    printf("esco da terminateConnection\n");
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
            printf("devo ritrasmettere\n");
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
            printf("arrivato ACK del SYN ACK\n");

            mtxLock(&mtxPacketAndDetails);
            details.addr = *clientAddr;
            details.Size = slen;
            details.sockfd = socketFD;
            details.remoteSeq = ACK.sequenceNum;
            mtxUnlock(&mtxPacketAndDetails);

//          printf("prima di aggiornare\n");
//          printWindow();

            ackSentPacket(ACK.ack);

//          printf("dopo l'aggiornamento\n");
//          printWindow();
            //--------------------------------------------INIT GLOBAL DETAILS
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
        if(checkPipe(&rtxN, pipeFd[0]))
        {
            printf("devo ritrasmettere\n");
            return 0;
        }
        sockResult = checkSocketAck(clientAddr, slen, socketFD, &ACK);
        if (sockResult == -1)
        {
            perror("error in socket read");
        }
        if (sockResult == 1)
        {
            ackSentPacket(ACK.ack);
            //--------------------------------------------INIT GLOBAL DETAILS
            return ACK.sequenceNum;
        }
        else if(sockResult == 2)
        {
            initWindow(1);
            return 1;
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
    printf("SYNACK inviato, numero di sequenza : %d\n", SYN_ACK.sequenceNum);

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
    printf("SYNACK inviato, numero di sequenza : %d\n", SYN_ACK.sequenceNum);

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
//                dprintf(fd, "%s\n", ent->d_name);
            }
        }
        closedir (dir);
    }
    else
        perror ("errore nell'apertura della directory");

    int i, j;
    char * temp;
    if((temp = malloc(500)) == NULL){
        perror("error on malloc temp");
    }

    for(i=0; i < count-2 ; i++){
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

    for (int k = 0; k < count-2; k++)
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
    printf("sono in receiveFirstDatagram\n");
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

    printf("file da aprire: %s\n", fileName);

    if((fd = open(fileName, O_RDWR | O_TRUNC | O_CREAT, 0777)) == -1)
    {
        perror("error in opening/creating file");
    }

    if ((lseek(fd, 0L, SEEK_SET)) == -1)
    {
        perror("TE LO AVEVO DETTO");
    }
    mtxLock(&mtxPacketAndDetails);
    details.firstSeqNum = packet.seqNum + 1;
    mtxUnlock(&mtxPacketAndDetails);
    return fd;
}

void sendCycle(int command)
{
    printf("sono il sender del server, sto per fare la list o la pull\n");
    int fd;
    datagram sndPacket;

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
                    } else if (readByte != 0) {
                        sndPacket.packetLen = readByte;
                        sndPacket.isFinal = (short) isFinal; //mi sa che non serve

                        mtxLock(&mtxPacketAndDetails);
                        sndPacket.ackSeqNum = details.remoteSeq;
                        mtxUnlock(&mtxPacketAndDetails);

                        sndPacket.seqNum = getSeqNum();

                        sndPacket.opID = getOpID();
                        //printf("ho inviato un pacchetto %d\n", sndPacket.seqNum);
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
        printf("esco dal ciclo di isFinal == 0\n");
        if (!thereIsAnError) {
            while (getSendBase() % WINDOWSIZE != finalSeq % WINDOWSIZE) {
                if (checkPipe(&rtx, pipeFd[0]) != 0) {
//                    printf("ritrasmetto5\n");
                    sndPacket = rebuildDatagram(fd, rtx, sndPacket.command);
                    sendDatagram(details.sockfd2, &(details.addr2), details.Size2, &sndPacket, 1);
                }
            }
            printf("sono qui\n");
            memset(sndPacket.content, 0, 512);
            sndPacket.isFinal = -1;
            sndPacket.seqNum = getSeqNum();
            sendDatagram(details.sockfd2, &(details.addr2), details.Size2, &sndPacket, 0);
            printf("inviato il pacchetto definitivo con isFinal = -1 e seq %d \n", sndPacket.seqNum);
            while (checkPipe(&finalAck, pipeSendACK[0]) == 0) {
                if (checkPipe(&rtx, pipeFd[0]) != 0) {
                    printf("ritrasmetto5\n");
                    sendDatagram(details.sockfd2, &(details.addr2), details.Size2, &sndPacket, 1);
                    printf("inviato il pacchetto definitivo con isFinal = -1 \n");
                }
            }
            printf("HO FINITO ESCO DAL CICLO\n");
        }
    }
    else {
        memset(sndPacket.content, 0, 512);
        sndPacket.isFinal = -2;
        sndPacket.seqNum = getSeqNum();
        sndPacket.opID = getOpID();
        sendDatagram(details.sockfd2, &(details.addr2), details.Size2, &sndPacket, 0);
        setDataError();
        printf("esco da sendCycle con un errore \n");
    }
}
