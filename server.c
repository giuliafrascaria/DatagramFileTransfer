

void listenFunction(int socketfd, struct details * details, handshake * message, ssize_t messageSize)
{
    initWindow(WINDOWSIZE, selectiveWnd);

    if(messageSize == sizeof(handshake))
    {
        char buffer[100];
        printf("richiesta dal client %s\n\n\n", inet_ntop(AF_INET, &((details->addr).sin_addr), buffer, 100));


        //startServerConnection(details, socketfd, message);

    }
    else
    {
        perror("wrong connection start\n");
        exit(EXIT_FAILURE);
    }


}
