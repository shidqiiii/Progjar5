#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>

#define LISTEN_PORT 2020
#define messageBufferSize 512
#define broadcastBufferSize 1024
#define connectionQueue 5
#define maxFileDescriptor 30

void panic(char* errorSource);

int main() {
    // init variables
    int listenerFileDescriptor,
        incomingClientFileDescriptor,
        bytesRead,
        bytesSent;
    socklen_t clientAddressLength;
    struct sockaddr_in  serverAddress,
                        clientAddress;
    char    clientAddressString[INET_ADDRSTRLEN],
            clientMessageBuffer[messageBufferSize],
            broadcastMessageBuffer[broadcastBufferSize];
    fd_set  activeFileDescriptors,
            readyFileDescriptors;
    int running = 1; // TRUE is 1 in C. "There is only ONE truth"....

    // set listener socket
    listenerFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);

    // Init file descriptor set
    FD_ZERO(&activeFileDescriptors); // fill with zeros
    FD_SET(listenerFileDescriptor, &activeFileDescriptors); // add the server listening socket to the file descriptor set

    // Init Server Address (Protocol, IP address, port)
    bzero(&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htons(INADDR_ANY);
    serverAddress.sin_port = htons(LISTEN_PORT);

    // bind the socket to server address
    if (bind(listenerFileDescriptor, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0)
        panic("bind");

    // Begin to listen for connections
    if (listen(listenerFileDescriptor, connectionQueue) < 0)
        panic("listen");

    while(running) {
        readyFileDescriptors = activeFileDescriptors;

        if (select(maxFileDescriptor, &readyFileDescriptors, NULL, NULL, NULL) < 0)
            panic("select");

        for (int currentFileDescriptor = 3; currentFileDescriptor < maxFileDescriptor; currentFileDescriptor++) {
            if (FD_ISSET(currentFileDescriptor, &readyFileDescriptors)) {
                if (currentFileDescriptor == listenerFileDescriptor) {
                    clientAddressLength = sizeof(clientAddress);
                    incomingClientFileDescriptor = accept(currentFileDescriptor, (struct sockaddr *) &clientAddress, &clientAddressLength);
                    if (incomingClientFileDescriptor < 0) {
                        running = 0;
                        panic("accept");
                    }
                    FD_SET(incomingClientFileDescriptor, &activeFileDescriptors);
                    inet_ntop(AF_INET, &clientAddress.sin_addr, clientAddressString, INET_ADDRSTRLEN);
                    printf("[SERVER] New connection from %s:%d will be handled by FD %d\n", clientAddressString, clientAddress.sin_port, incomingClientFileDescriptor);
                }
                else {
                    incomingClientFileDescriptor = currentFileDescriptor;
                    bzero(clientMessageBuffer, sizeof(clientMessageBuffer));
                    bytesRead = read(incomingClientFileDescriptor, clientMessageBuffer, messageBufferSize);
                    if (bytesRead < 0) {
                        running = 0;
                        panic("read");
                    } else if (bytesRead == 0) {
                        printf("[SERVER] Done reading from FD %d, or the client terminated. Connection in this socket will be closed\n", incomingClientFileDescriptor);
                        close(incomingClientFileDescriptor);
                        FD_CLR(currentFileDescriptor, &activeFileDescriptors);
                    } else {
                        inet_ntop(AF_INET, &clientAddress.sin_addr, clientAddressString, INET_ADDRSTRLEN);
                        //printf("[MESSAGE] %s:%d -> %s", clientAddressString, clientAddress.sin_port, clientMessageBuffer);
                        printf("[MESSAGE] %s -> %s", clientAddressString, clientMessageBuffer);
                        //sprintf(broadcastMessageBuffer, "%s:%d -> %s\n", clientAddressString, clientAddress.sin_port, clientMessageBuffer);
                        sprintf(broadcastMessageBuffer, "%s -> %s", clientAddressString, clientMessageBuffer);

                        for (int broadcastFileDescriptor = 3; broadcastFileDescriptor < maxFileDescriptor; broadcastFileDescriptor++) {
                            //printf("[SERVER][VERBOSE] Checking FD %d\n", broadcastFileDescriptor);
                            if (FD_ISSET(broadcastFileDescriptor, &activeFileDescriptors)) {
                                if (broadcastFileDescriptor != listenerFileDescriptor && broadcastFileDescriptor != incomingClientFileDescriptor) {
                                    bytesSent = write(broadcastFileDescriptor, broadcastMessageBuffer, sizeof(broadcastMessageBuffer));
                                    if (bytesSent < 0) {
                                        running = 0;
                                        panic("write");
                                    }
                                    printf("[SERVER] %d bytes sent to %s:%d via FD %d\n", bytesSent, clientAddressString, clientAddress.sin_port, broadcastFileDescriptor);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

void panic(char* errorSource) {
    perror(errorSource);
    exit(EXIT_FAILURE);
}