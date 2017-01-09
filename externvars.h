//
// Created by root on 09/01/17.
//

#ifndef DFT_EXTERNVARS_H
#define DFT_EXTERNVARS_H

#include <sys/types.h>
#include "dataStructures.h"

extern struct details details;
extern int timerSize;
extern int nanoSleep;
extern int windowSize;
extern int pipeFd[2];
extern int pipeSendACK[2];
extern datagram packet;

extern volatile int finalLen, globalTimerStop;

extern int globalOpID;
extern pthread_mutex_t syncMTX;
extern pthread_mutex_t mtxPacketAndDetails;

#endif //DFT_EXTERNVARS_H
