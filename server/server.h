#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <stdarg.h>

#include "../sockets-lib/socket.h"
#include "sourceValidator.h"
#include "typeracerStructures.h"
#include "jsonHandler.h"

extern int logging;

void thread_printf(const char *format, ...);
int serverInit(Game *game, const char *fileName);
int startServer(const int port, Game *game);
void *clientWaiter(void *arg);
void *handleClient(void *arg);
void broadcastProgress(Game *game);
void serverDestroy(Game *game);
int refresh(Game *game, pthread_mutex_t *mutex, RefreshingData *refreshingData, int *playerIndex);
void *startupServer(void *arg);
