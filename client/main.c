#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/socket.h>


#include "../sockets-lib/socket.h"
#include "typeracerStructures.h"


void renderTrack(int* playerWordCount, int* sentenceWordCount, int* tWidth) {
    printf("Player 1: ");
    int max = ((*tWidth - 20) * *playerWordCount) / *sentenceWordCount;
    for (int i = 0; i < max; i++) printf("-");
    printf(">\n");
}

int main(int argc, char** argv) {
    //int result = system("./build/server/server 5 input.txt");
    int port = 54897;
    int activeSocket = connect_to_server("localhost", port);
    if (activeSocket < 0) {
        printf("Failed to connect to server.\n");
        return 1;
    }
    printf("Connected to server on port %d.\n", port);
    if (argc < 2) {
        printf("Please provide your name as an argument.\n");
        return 1;
    }
    sleep(2);
    if (strlen(argv[1]) < 0 && strlen(argv[1]) > 19) {
        printf("Name must be between 1 and 19 characters.\n");
        return 1;
    }
    char* name = argv[1];
    printf("Sending name to server: %s\n", name);
    if (send(activeSocket, name, 19, 0) < 0) {
        printf("Failed to send name to server.\n");
        return 1;
    }
    sleep(10);
    char buffer[20] = {0};
    int bytes_received = recv(activeSocket, buffer, strlen(buffer), 0);
    printf("Received: %d\n", bytes_received);
    //printf("Received: %p\n", data);
    //printf("Received: %s\n", data->sentence);
    if (bytes_received == 0) {
        printf("Client with socket %d disconnected.\n", activeSocket);
        active_socket_destroy(activeSocket);
        return 0;
    }
    //SocketsData* data = malloc(sizeof(bytes_received));
    //deserialize((char*)buffer, data);
    //printf("Received: %p\n", data);
    for (int i = 0; i < 20; i++) {
        printf("%c \n", buffer[i]);
    }
    sleep(5);
}
