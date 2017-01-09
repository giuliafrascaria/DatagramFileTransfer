//
// Created by giogge on 09/12/16.
//

#ifndef DFT_SERVER_H
#define DFT_SERVER_H

#include "dataStructures.h"

void listenFunction(int socketfd, struct details * details, handshake * message);
void * sendFunction();
void startServerConnection(struct details * cl, int socketfd, handshake * message);
void startSecondConnection(struct details * cl, int socketfd);
void sendSYNACK2(int privateSocket, socklen_t socklen , struct details * cl);

#endif //DFT_SERVER_H