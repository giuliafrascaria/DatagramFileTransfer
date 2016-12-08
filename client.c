
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <zconf.h>


//MAIN
//------------------------------------------------------------------------------------------------------------------MAIN
int main(int argc, char * argv[])
{

    if(argc == 1)
    {
        printf("%s\n\n", argv[0]);
    }

    printf("connection is starting... \n\n\n");

    clientSendFunction();

    exit(EXIT_SUCCESS);
}

void * clientSendFunction() {

    struct flock readPipeLock;

    readPipeLock.l_type = F_RDLCK;
    readPipeLock.l_whence = SEEK_CUR;
    readPipeLock.l_start = 0;
    readPipeLock.l_len = sizeof(struct pipeMessage);

    FILE *fdLog = fopen(LOGSENDER, "w+");
    while (fdLog == NULL) {
        perror("1: error in log open");
        sleep(1);
        fdLog = fopen(LOGSENDER, "w+");
        //trova una soluzione, magari in caso di errore fai digitare al'utente un directory
    }

    if (pthread_mutex_lock(&mtx) != 0) {
        perror("error in mutex lock");
    }
    if (pipe(pipefd) == -1) {
        perror("2: error in pipe open\n");
    }
    if (pthread_mutex_unlock(&mtx) != 0) {
        perror("error in mutex unlock");
    }

    int socketfd;
    int fd;
    int slot;
    struct sockaddr_in senderServerAddress;
    senderServerAddress = createStruct(4242); //create struct with server port

    socketfd = createSocket();
    socklen_t serverLen = sizeof(struct sockaddr_in);

    //alarm(5);
    if (pthread_mutex_lock(&mtx) != 0) {
        perror("error in lock");
    }
    LOS = 2;
    if (pthread_mutex_unlock(&mtx) != 0) {
        perror("error in lock");
    }
    //sleep(10);
    printf("starting handshake procedure\n\n");
    startClientConnection(&sendServer, &senderServerAddress, serverLen, socketfd, fdLog);
    //alarm(0);

    printf("finished first handshake\n");
    struct details *details = &sendServer;

    handshake PROVA;
    if (recvfrom(socketfd, (char *) &PROVA, sizeof(handshake), 0, (struct sockaddr *) &senderServerAddress,
                 &serverLen) == -1) {
        perror("non riesco");
    }
    fprintf(fdLog, "ho ricevuto %d\n", PROVA.sequenceNum);


    clientCreateThread(&listenThread, clientListenFunction);

    //close(pipefd[1]);

    fprintf(fdLog, "in attesa che il listener sblocchi la variabile\n");

    fflush(fdLog);

    /*while(daEliminare == 0)
    {

    }*/
    while (pthread_cond_wait(&cond, &mut) != 0) {
        perror("1: error on cond_wait");
        sleep(1);
        hmt = nonFatalErr(details, hmt, 1);
    }
    hmt = 0;


    fprintf(fdLog, "variabile sbloccata\n");

    if (connect(socketfd, (struct sockaddr *) &sendServer.addr, sendServer.Size) == -1) {
        perror("failed to connect to server socket\n");
        exit(EXIT_FAILURE);
    }

    fprintf(fdLog, "il sender del client si è connesso\n");


    if (pthread_create(&timerThread, NULL, (void *) clientTimerFunction, &sendServer) != 0) {
        perror("error in pthread_create");
        exit(EXIT_FAILURE);
    }

    if (pipe(pipeRT) == -1) {
        perror("error in pipe open\n");
    }
/*
    if(pthread_create(&retransmissionThread, NULL, (void *) clientRetransmissionFunction, &sendServer) != 0)
    {
        perror("error in pthread_create");
        exit(EXIT_FAILURE);
    }
*/
    //struct timespec tstart={0,0};
    struct timespec tstart;
    tstart.tv_nsec = 0;
    tstart.tv_sec = 0;
    // datagram packet;
    //pthread_mutex_lock (&mut);
    firstPacket = 0;

    handshake acknowledgement;
    acknowledgement.ack = 9000;
    fprintf(fdLog, "tentativo di connessione al server\n");
    if (send(socketfd, &acknowledgement, sizeof(handshake), 0) < 0) {
        perror("error in send");
    }
    fprintf(fdLog, "connessione stabilita\n\n");

    if (pthread_mutex_lock(&mtx) != 0) {
        perror("error in mutex lock");
    }
    details->sockfd = socketfd;
    if (pthread_mutex_unlock(&mtx) != 0) {
        perror("error in mutex unlock");
    }

    //struct pipeMessage ackmsg;
    //handshake newAcknowledgement;


    fflush(fdLog);  //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%	FFLUSH

    for (int k = 0; k < WINDOWSIZE; k++) {
        details->selectiveWnd[k].wheelTimer = malloc(sizeof(struct timer));
    }
}

void startClientConnection(struct details * serv, struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, FILE * fdLog)
{
    int rc = alarm(4);
    printf("%d\n\n", rc);

    handshake SYN;
    srandom((unsigned int)getpid());
    //srandom(seed);
    SYN.sequenceNum = rand() % 4096;

    size_t SYNsize = sizeof(handshake);

    //mando il primo datagramma senza connettermi
    ssize_t sentData;
    sentData = sendto(socketfd, (char *) &SYN, SYNsize, 0, (struct sockaddr* ) servAddr, servLen);

    //printf("sleep artificiale\n");
    //sleep(6);

    if(sentData == -1)
    {
        perror("error in sending data\n");
        exit(EXIT_FAILURE);
    }


        /* else if(sentData != SYNsize)
         {
             perror("non ho scritto niente\n");
             exit(EXIT_FAILURE);
         }*/
    else
    {
        fprintf(fdLog, "ho mandato il SYN\n");
        //aspetto di ricevere la conferma e fare il bind alla porta dedicata
        //ssize_t msgLen;
        struct sockaddr_in serverPrivateAddress;
        socklen_t serverPrivateLen = sizeof(struct sockaddr_in);

        //serverPrivateAddress = createStruct(0);

        handshake SYN_ACK;
        size_t SYNACKsize = sizeof(handshake);
        //ora se il server apre una porta effimera dovrei avere un valore doverso in serverAddress
        //rimango bloccato in chiamata di receive
        //alarm(2);


        if(pthread_mutex_lock(&mtx) != 0)
        {
            perror("lock error");
        }
        LOS = 2;
        if(pthread_mutex_unlock(&mtx) != 0)
        {
            perror("unlock error");
        }

        printf("waiting for synack\n\n");

        if(recvfrom(socketfd, (char *) &SYN_ACK, SYNACKsize, 0, (struct sockaddr*) &serverPrivateAddress, &serverPrivateLen) == -1)
        {
            perror("error in socket read\n");
            //exit(EXIT_FAILURE);
        }


        /*if(pthread_mutex_lock(&mtx) != 0)
        {
            perror("lock error");
        }
        //LOS = 2;
        if(pthread_mutex_unlock(&mtx) != 0)
        {
            perror("unlock error");
        }*/

        //alarm(0);
        fprintf(fdLog, "ho ricevuto il SYNACK\n");
        //devo verificare che il campo ack di SYN_ACK sia il mio seqnum + 1
        if(SYN_ACK.ack != (SYN.sequenceNum + 1))
        {
            perror("error in receive ack\n");
        }

        int serverSeqNum;
        serverSeqNum = SYN_ACK.sequenceNum;


        //salvo tutti i dettagli della comunicazione che mi ha trasmesso il server
        if(pthread_mutex_lock(&mtx) != 0)
        {
            perror("error in mutex lock");
        }
        serv->servSeq = serverSeqNum;
        serv->addr = serverPrivateAddress;
        serv->windowDimension = WINDOWSIZE;
        //----------------------------------------------------------PERCHÈ NON FUNZIONA CON 2048 E SUPERIORI ????
        serv->Size = serverPrivateLen;
        serv->mySeq = SYN.sequenceNum + 1;

        if(pthread_mutex_unlock(&mtx)!=0)
        {
            perror("error in mutex unlock");
        }

        initWindow(serv->windowDimension, serv->selectiveWnd);
        /*for (int i = 0; i < (serv->windowDimension); i++) {
            //serv->selectiveWnd[i] = malloc(sizeof (struct selectCell));
            (serv->selectiveWnd[i]).value = 0;
            printf("--- %d \n", (serv->selectiveWnd[i]).value);
        }*/

        //ora in teoria dovrei avere in serverAddress la porta effimera a me dedicata, mi connetto
/*		if(connect(socketfd, (struct sockaddr *) &serverPrivateAddress, serverPrivateLen) == -1)
		{
			perror("failed to connect to server socket\n");
			exit(EXIT_FAILURE);
		}*/

        handshake ACK;
        ACK.ack = serverSeqNum + 1;
        size_t ACKsize = sizeof(handshake);
        ACK.sequenceNum = serv->mySeq;

        fprintf(fdLog, "IL NUMERO DI SEQUENZA È : %d \n", ACK.sequenceNum);

        /* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  */
        /* %%%%%%%%%       da vedere assolutamente     %%%%%%%%%%%  */
        /* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%  */

        ACK.windowsize = WINDOWSIZE;

        fprintf(fdLog, "sono riuscita a connettermi a una connessione dedicata\n");
//		if(write(socketfd, (char *) &ACK, ACKsize) == -1)
//		{
//			perror("error in server ACK\n");
//			exit(EXIT_FAILURE);
//		}

        if(sendto(socketfd, (char *)&ACK, ACKsize, 0, (struct sockaddr *) &serverPrivateAddress, serverPrivateLen) == -1)
        {
            perror("mandai male\n");
        }

        //aggiorno il sequence number della connessione al primo disponibile
        serv->mySeq++;
        serv->addr = serverPrivateAddress;
        fprintf(fdLog, "ho mandato l'ACK\n");
        fprintf(fdLog, "server sequence: %d\n", serverSeqNum);
    }
    fflush(fdLog);

    alarm(0);
}



void secondHandshake(struct details * serv, struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, FILE * fdLog)
{
    alarm(4);

    handshake SYN;
    unsigned int seed = 17;
    srandom(seed);
    SYN.sequenceNum = rand() % 1024;

    size_t SYNsize = sizeof(handshake);

    //mando il primo datagramma senza connettermi
    ssize_t sentData = sendto(socketfd, (char *) &SYN, SYNsize, 0, (struct sockaddr* ) servAddr, servLen);
    if(sentData == -1)
    {
        perror("error in sending data\n");
        exit(EXIT_FAILURE);
    }

    else
    {
        fprintf(fdLog, "ho mandato il SYN 2\n");
        //aspetto di ricevere la conferma e fare il bind alla porta dedicata
        //ssize_t msgLen;
        struct sockaddr_in serverPrivateAddress;


        memset(&serverPrivateAddress, 0, sizeof(struct sockaddr_in));
        serverPrivateAddress.sin_family = AF_INET;
        serverPrivateAddress.sin_port = htons( servAddr->sin_port ); //DA CONTROLLARE --------------------------------
        serverPrivateAddress.sin_addr.s_addr = htonl( INADDR_ANY );


        socklen_t serverPrivateLen = sizeof(struct sockaddr_in);

        //serverPrivateAddress = createStruct(0);

        handshake SYN_ACK;
        size_t SYNACKsize = sizeof(handshake);
        //ora se il server apre una porta effimera dovrei avere un valore doverso in serverAddress
        //rimango bloccato in chiamata di receive
        if(recvfrom(socketfd, (char *) &SYN_ACK, SYNACKsize, 0, (struct sockaddr*) &serverPrivateAddress, &serverPrivateLen) == -1)
        {
            perror("error in socket read\n");
            //exit(EXIT_FAILURE);
        }
        fprintf(fdLog, "ho ricevuto il SYNACK\n");



        //ora in teoria dovrei avere in serverAddress la porta effimera a me dedicata, mi connetto
        if(connect(socketfd, (struct sockaddr *) &serverPrivateAddress, serverPrivateLen) == -1)
        {
            perror("failed to connect to server socket\n");
            exit(EXIT_FAILURE);
        }

        if(pthread_mutex_lock(&mtx) != 0)
        {
            perror("error in mutex lock");
        }
        serv->addr = serverPrivateAddress;
        serv->Size = serverPrivateLen;
        if(pthread_mutex_unlock(&mtx)!=0)
        {
            perror("error in mutex unlock");
        }

        handshake ACK;
        size_t ACKsize = sizeof(handshake);
        ACK.sequenceNum = serv->mySeq;
        fprintf(fdLog, "IL NUMERO DI SEQUENZA È : %d \n", ACK.sequenceNum);
        fflush(fdLog);

        /*------------------------------------------------------------------------DA VEDERE SSOLUTAMENTE*/
        ACK.windowsize = WINDOWSIZE;
        /*------------------------------------------------------------------------DA VEDERE SSOLUTAMENTE*/

        fprintf(fdLog, "sono riuscita a connettermi a una connessione dedicata\n");
        if(write(socketfd, (char *) &ACK, ACKsize) == -1)
        {
            perror("error in server ACK\n");
            exit(EXIT_FAILURE);
        }

        //aggiorno il sequence number della connessione al primo disponibile

        fprintf(fdLog, "ho mandato l'ACK\n");
    }
    fflush(fdLog);

    alarm(0);
}
