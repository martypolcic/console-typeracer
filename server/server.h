#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>

#include "../sockets-lib/socket.h"
#include "sourceValidator.h"
#include "typeracerStructures.h"
#include "jsonHandler.h"

void serverInit(Game* game, const char* fileName);
int startServer(const int port, const char* fileName, Game* game);
void* clientWaiter(void* arg);
void* handleClient(void* arg);
void broadcastProgress(Game* game);
int gameDestroy(Game* game);
