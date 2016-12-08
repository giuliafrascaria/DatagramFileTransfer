//
// Created by giogge on 08/12/16.
//


#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>

struct headTimer
{
    volatile struct timer * nextTimer;
    volatile int assert;
};

struct timer
{
    volatile int seqNum;
    //double lastRTT;
    //short int transmitN;
    volatile struct timer * nextTimer;
    volatile int posInWheel;
    volatile short int isValid;
};

/*
struct head
{
    struct timer * nextTimer;
};
 */

struct pipeMessage
{
    int seqNum;
    short int isFinal;
};

struct selectCell
{
    int value;
    //struct timer packetTimer;
    int seqNum;
    volatile struct timer *wheelTimer;
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
    struct selectCell selectiveWnd[];
};




struct RTTsample
{
    int seqNum;
    long timestamp;
//    short int isValid;
    long RTT;
    long previousEstimate;
};


/*------------------TIMER ------------------------------------------------------------------------------*/


//int calSlot(struct timer *t, double currentRTT);
//void startTimer(timer * wheel, int position, timer * datagramTimer, int currentEstimate);
//void stopTimer(struct timer ** t, struct timer **wheel);
//void initWheel(int dimension, struct timer ** wheel);
//void runTimeWheel(int dimension, timer *wheel);
long updateRTTavg(long previousEstimate, long newRTT);
//int tBackoff(struct timer * currentTimer, int dimension, double pValue);

/*------------------TERMINATION & RECOVERY -------------------------------------------------------------*/

//void clientExitProcedure(struct details *details, int operationID);

//void serverExitProcedure(struct serverDetails *details, int operationID);

//int serverTimeOut();

void * timerFunction(int dimension, pthread_cond_t * condTIM, pthread_mutex_t * mtxTIM, pthread_mutex_t * wheelmtx, pthread_mutex_t * currenttimemtx,
                     pthread_mutex_t * timecellmtx, int NANOTIMER, volatile struct headTimer* wheel, int * currentTimeSlot,
                     volatile short int * TIMGB, int pipeRT, volatile int * dihtr, pthread_mutex_t * mtx);

/*------------------SELECTIVE REPEAT -------------------------------------------------------------*/

struct sockaddr_in createStruct(unsigned short portN);



void initWindow(int dimension, struct selectCell *window);
//int setWindowBase(int packetN, int dimension);

//void sentPacket(int packetN, int dimension, struct selectCell window[dimension], struct timer ** wheel,struct  timer *tpacket);

void sentPacket(pthread_mutex_t *mtxARCVD ,int packetN, struct details * details,
                struct timer * packetTimer, volatile struct headTimer * wheel,
                int slot, int offset, int retransmission);
//int ackSentPacket(int ackN, int sendBase, int dimension, struct selectCell window[dimension],struct  timer **wheel);

void ackSentPacket(pthread_mutex_t * mtxARCVD, int ackN, int currentSlot, struct details *details);

//int checkWindowInterval(int dimension, int packetN, int sendBase, int nextseqnum);

//void addTimer(struct timer * packetTimer, int sequenceNumber, int dimension, struct selectCell window[dimension]);
void startTimer( struct timer * packetTimer,  struct selectCell * detailscell, int packetN, volatile struct headTimer * wheel, int posInWheel);
void nStopTimer(int ackN, struct details * details, int currentSlot);
void takingRTT(struct RTTsample * currentRTT, pthread_mutex_t * timemtx, struct timespec * tend );
//void deleteTimer(int sequenceNumber, int dimension, struct selectCell window[dimension]);

//int ackReceivedPacket(int packetN, int dimension, int recvbase);
//struct selectCell * updateRecvWindow(int packetN, int dimension, struct selectCell window[dimension], int * recvbase);


//---------------------------------------------------------------------------------------------------------CREATE SOCKET
int createSocket();


void AlarmWakeUp(int signum);

/*
 * void retransmitPacket(int CorS, struct details * sendServer, int pipeRT, volatile short int  * hmt, datagram *  packet, int firstPacket,
                      volatile int * currentTimeSlot, pthread_mutex_t * currenttimemtx, char ** FN, struct timer * packetTimer,
                      struct RTTsample * currentRTT, volatile struct headTimer * wheel, pthread_cond_t *condRT,
                      pthread_mutex_t * mtxRT);
*/
void retransmissionClient( int pipeRT, struct details * details, datagram * packet,
                           int firstPacket, volatile struct headTimer * wheel,
                           int currentTimeSlot, struct RTTsample * currentRTT, char ** FN, pthread_mutex_t * mtxARCVD);

void retransmissionServer( int pipeRT, struct details * details, datagram * packet,
                           int firstPacket, volatile struct headTimer * wheel,
                           int currentTimeSlot, struct RTTsample * currentRTT, char ** FN);
/*-------------------------------------------------------------------------------------------------------*/


#endif //FTOUDP_DATASTRUCTURES_H
