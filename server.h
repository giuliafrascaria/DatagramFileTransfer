//
// Created by giogge on 09/12/16.
//

#ifndef DFT_SERVER_H
#define DFT_SERVER_H

#include "dataStructures.h"

void listenFunction(int socketfd, struct details * details, handshake * message, ssize_t messageSize);

#endif //DFT_SERVER_H
