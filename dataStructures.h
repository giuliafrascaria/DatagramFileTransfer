//
// Created by giogge on 08/12/16.
//


#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>

//------------------------------------------------------------------------------------------------------STRUTTURE DATI

struct timer
{
    volatile int seqNum;
    //double lastRTT;
    //short int transmitN;
    struct timer * nextTimer;
    volatile int posInWheel;
    volatile short int isValid;
};

struct selectCell
{
    int value;
    struct timer packetTimer;
    pthread_mutex_t cellMtx;
};

typedef struct datagram_t
{
    volatile int command;
    int opID;
    int seqNum;
    int ackSeqNum;
    short int isFinal;
    char content[512];
} datagram;

typedef struct handshake_t
{
    int ack; //se vale 1 sto ackando il precedente
    int windowsize; //lo imposta il server nella risposta, è opID nelle successive transazioni
    int sequenceNum;
    short int isFinal;
} handshake;

struct details
{
    int windowDimension;
    struct sockaddr_in addr;
    struct sockaddr_in addr2;
    int sockfd;
    int sockfd2;
    socklen_t Size;
    socklen_t Size2;
    int remoteSeq;
    int mySeq;
    int volatile sendBase;
    int volatile firstSeqNum;
    //struct selectCell selectiveWnd[];
};

struct headTimer
{
    struct timer * nextTimer;
};

struct pipeMessage
{
    int seqNum;
    short int isFinal;
};

//----------------------------------------------------------------------------------------------------------------TIMER

void * timerFunction();
void initTimerWheel();
void startTimer(int packetN, int posInWheel);
int getWheelPosition();
void clockTick();

//------------------------------------------------------------------------------------------------TERMINATION & RECOVERY



//--------------------------------------------------------------------------------------------------------RETRANSMISSION

/*
void retransmissionClient( int pipeRT, datagram * packet, int firstPacket, char * FN);

void retransmissionServer( int pipeRT, datagram * packet, int firstPacket, char * FN);
*/

int checkPipe(struct pipeMessage *rtxN, int pipefd);
//------------------------------------------------------------------------------------------------------SELECTIVE REPEAT

void initWindow();

void sentPacket(int packetN, int retransmission);

void ackSentPacket(int ackN);

void printWindow();

void slideWindow();

//----------------------------------------------------------------------------------------------------------------THREAD

void createThread(pthread_t * thread, void * function, void * arguments);

void initPipe(int pipefd[2]);

//---------------------------------------------------------------------------------------------------------CREATE SOCKET

int createSocket();

struct sockaddr_in createStruct(unsigned short portN);

void bindSocket(int sockfd, struct sockaddr * address , socklen_t size);

int checkSocketAck(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, handshake * ACK);

int checkSocketDatagram(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, datagram * packet);

void mtxLock(pthread_mutex_t * mtx);

void mtxUnlock(pthread_mutex_t * mtx);

//---------------------------------------------------------------------------------------SHORT FUNCTION TO SIMPLIFY CODE

void sendDatagram(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen, struct datagram_t * sndPacket);

void sendACK(int socketfd, handshake *ACK, struct sockaddr_in * servAddr, socklen_t servLen);

int receiveACK(int mainSocket, struct sockaddr * address, socklen_t *slen);

int receiveDatagram(int socketfd, int file, struct sockaddr * address, socklen_t *slen, int firstN, size_t finalLen);

int openFile(char * fileName);

void closeFile(int fd);

void acceptConnection(int mainSocket, handshake * ACK, struct sockaddr * address, socklen_t *slen);

void sendSignalThread(pthread_mutex_t * mtx, pthread_cond_t * condition);

int checkWindowSendBase();

void writeOnFile(int file, char * content, int seqnum, int firstnum ,size_t len);

void tellSenderSendACK(int packetN, short int isFinal);

datagram rebuildDatagram(int fd, struct pipeMessage pm);

void ACKandRTXcycle(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen);

void getResponse(int socket, struct sockaddr_in * address, socklen_t *slen, int fd);

void waitForAckCycle(int socket, struct sockaddr * address, socklen_t *slen);

int getFileLen(int fd);

char * stringParser(char * string);

void waitForFirstPacketSender(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen);

void waitForFirstPacketListener(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen);

//----------------------------------------------------------------------------------------------------------------------

#endif //DATASTRUCTURES_H