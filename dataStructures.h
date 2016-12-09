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
    int seqNum;
    struct timer * wheelTimer;
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
    int sockfd;
    socklen_t Size;
    int servSeq;
    int mySeq;
    int volatile sendBase;
    //struct selectCell selectiveWnd[];
};

struct headTimer
{
    struct timer * nextTimer;
};
//----------------------------------------------------------------------------------------------------------------TIMER


//------------------------------------------------------------------------------------------------TERMINATION & RECOVERY

//------------------------------------------------------------------------------------------------------SELECTIVE REPEAT

void initWindow();

void sentPacket(pthread_mutex_t *mtxARCVD , int packetN, int windowDim,
                struct timer * packetTimer,
                int slot, int offset, int retransmission);

void ackSentPacket(pthread_mutex_t * mtxARCVD, int ackN, int currentSlot, struct details *details);


//---------------------------------------------------------------------------------------------------------CREATE SOCKET
int createSocket();

struct sockaddr_in createStruct(unsigned short portN);

void bindSocket(int sockfd, struct sockaddr * address , socklen_t size);
//----------------------------------------------------------------------------------------------------------------------

void receiveMsg(int mainSocket, handshake * SYN, size_t SYNlen, struct sockaddr * address, socklen_t *slen);

void createThread(pthread_t * thread, void * function, void * arguments);

void * timerFunction();
void initTimerWheel();
void startTimer( struct timer * packetTimer, int packetN, int posInWheel);



#endif //DATASTRUCTURES_H
