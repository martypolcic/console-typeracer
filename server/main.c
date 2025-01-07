#include <stdio.h>
#include <stdlib.h>

#include "typeracerStructures.h"
#include "../sockets-lib/socket.h"
#include "server.h"

int main(int argc, char** argv) {
    srand(time(NULL));
    int port = 54897;
    if (argc < 3) {
        printf("Usage: %s <max-players> <filename>\n", argv[0]);
        return 1;
    }
    int maxPlayers = atoi(argv[1]);
    char* fileName = argv[2];

    Game* game = malloc(sizeof(Game));
    if (game == NULL) {
        perror("Failed to allocate memory for game");
        return 1;
    }
    game->maxPlayerCount = maxPlayers;

    printf("Initializing server...\n");
    serverInit(game, fileName);

    printf("Starting server on port %d...\n", port);
    printf("Port: %d, Filename: %s, Game: %p\n", port, fileName, game);
    int started = startServer(port, fileName, game);
    if (started == -1) {
        printf("Error starting server.\n");
        return 1;
    }
    printf("Server started on port %d.\n", port);

    printf("Game over.\n");
    int destroyed = gameDestroy(game);
    if (destroyed == -1) {
        printf("Error destroying game.\n");
        return 1;
    }
    printf("Server shutting down\n");
    return 0;
}