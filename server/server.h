#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>

#include "../sockets-lib/socket.h"
#include "sourceValidator.h"
#include "typeracerStructures.h"
#include "jsonHandler.h"
#include "eventQueue.h"

extern int logging;

typedef enum RoomState {
    ROOM_STATE_WAITING = 0,
    ROOM_STATE_IN_PROGRESS = 1,
    ROOM_STATE_FINISHED = 2
} RoomState;

typedef struct RoomHub {
    char roomCode[16];
    int maxPlayers;
    RoomState state;
    Game *game;
    pthread_t runnerThread;
    pthread_mutex_t roomMutex;
    pthread_cond_t roomCond;
    EventQueue eventQueue;
    pthread_mutex_t startMutex;
    pthread_cond_t startCond;
    ThreadData *threadData;
    char summaryJson[2048];
    time_t createdAt;
    time_t finishedAt;
    bool runnerStarted;
    bool closeRequested;
    struct RoomHub *next;
} RoomHub;

void thread_printf(const char *format, ...);
int serverInit(Game *game, const char *fileName);
int runGameHub(Game *game, EventQueue *eventQueue, pthread_mutex_t *startMutex, pthread_cond_t *startCond);
void *handleClient(void *arg);
void broadcastProgress(Game *game, RefreshingData *refreshingData);
void serverDestroy(Game *game);
void parseClientHandshake(const char *rawBuffer, int bytesReceived, char *outRoomCode, size_t roomCodeSize, char *outName, size_t nameSize);
RoomHub *coordinator_create_room(const char *roomCode, int maxPlayers, const char *fileName);
RoomHub *coordinator_get_room(const char *roomCode);
void coordinator_destroy_room(RoomHub *hub);

extern volatile sig_atomic_t gCoordinatorRunning;

int startCoordinatorServer(const int port, const char *fileName, int defaultMaxPlayers);
void stopCoordinatorServer(void);
