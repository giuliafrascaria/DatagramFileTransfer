
#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>

//--------------------------------------------------------------------------------------------------------STRUTTURE DATI
#define WINDOWSIZE 2048
#define TIMERSIZE 2048
#define LOSSPROB 100
#define BASETIMER 200

struct timer
{
    volatile int seqNum;
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
    ssize_t packetLen;
    char content[512];

} datagram;

typedef struct handshake_t
{
    int ack; //se vale 1 sto ackando il precedente
    int sequenceNum;
    short int isFinal;
} handshake;

struct details
{
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

struct RTTsample
{
    int seqNum;
    long timestamp;
    long RTT;
    long previousEstimate;
};


//-------------------------------------------------------------------------------------------------------------PROTOTIPI

//A
void ackSentPacket(int ackN);
void ACKandRTXcycle(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen, int command);
void acceptConnection(int mainSocket, handshake * ACK, struct sockaddr * address, socklen_t *slen);
//B
void bindSocket(int sockfd, struct sockaddr * address , socklen_t size);
//C
void clockTick();
int checkPipe(struct pipeMessage *rtxN, int pipefd);
int createSocket();
void createThread(pthread_t * thread, void * function, void * arguments);
int checkSocketAck(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, handshake * ACK);
int checkSocketDatagram(struct sockaddr_in * servAddr, socklen_t servLen, int socketfd, datagram * packet);
void closeFile(int fd);
void condWaitSender(pthread_mutex_t * mutex, pthread_cond_t *cond, int connection);
int canISend();
//D
//E
//F
//G
int getRTTseq();
int getOpID();
int getSendBase();
int getSeqNum();
int getFileLen(int fd);
int getWheelPosition();
int getDataError();
void getResponse(int socket, struct sockaddr_in * address, socklen_t *slen, int fd, int command);
int getCurrentTimeSlot();
int getGlobalSenderWait();
//H
//I
void initTimerWheel();
void initWindow(int times);
void initPipe(int pipefd[2]);
void incrementRounds();
//J
//K
//L
//M
void mtxLock(pthread_mutex_t * mtx);
void mtxUnlock(pthread_mutex_t * mtx);
//N
//O
int openFile(char * fileName);
//P
//Q
//R
unsigned int randomGen();
int receiveACK(int mainSocket, struct sockaddr * address, socklen_t *slen);
datagram rebuildDatagram(int fd, struct pipeMessage pm, int command);
int readGlobalTimerStop();
void resetDataError();
//S
void startTimer(int packetN, int posInWheel);
void sentPacket(int packetN, int retransmission);
void slideWindow();
void sendDatagram(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen, struct datagram_t * sndPacket, int rtx);
void sendACK(int socketfd, handshake *ACK, struct sockaddr_in * servAddr, socklen_t servLen);
struct sockaddr_in createStruct(unsigned short portN);
void sendSignalThread(pthread_mutex_t * mtx, pthread_cond_t * condition, int connection);
char * stringParser(char * string);
void sendSignalTimer();
void setDataError();
void startRTTsample(int seq);
//T
void * timerFunction();
void tellSenderSendACK(int packetN, short int isFinal);
void takingRTT();
//U
long updateRTTavg(long previousEstimate, long newRTT);
//V
//W
void waitForFirstPacketSender(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen);
void waitForFirstPacketListener(int socketfd, struct sockaddr_in * servAddr, socklen_t servLen);
void writeOnFile(int file, char * content, int seqnum, int firstnum ,size_t len);
void waitForAckCycle(int socket, struct sockaddr * address, socklen_t *slen);
//X
//Y
//Z
//----------------------------------------------------------------------------------------------------------------------

#endif //DATASTRUCTURES_H