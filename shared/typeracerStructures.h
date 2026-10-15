#ifndef TYPERACERSTRUCTURES_H
#define TYPERACERSTRUCTURES_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/time.h>

typedef struct Player
{
    int activeSocket;
    char *name;
    double timeTaken;
    bool finished;
    bool disconnected;
} Player;

typedef struct Times
{
    double *times;
    int count;
} Times;

typedef struct RefreshingData
{
    int *progress;
    int count;
} RefreshingData;

typedef struct SocketsData
{
    char **names;
    char *sentence;
    int playerCount;
    int playerIndex;
} SocketsData;

typedef struct Game
{
    int serverSocket;
    Player **players;
    int maxPlayerCount;
    int actualPlayerCount;
    pthread_t *threads;
    int finishedPlayers;
    SocketsData *socketsData;
    RefreshingData *refreshingData;
    char roomCode[16];
    bool gameStarted;
} Game;

typedef struct ThreadData
{
    Game *game;
    RefreshingData *refreshingData;
    pthread_mutex_t *mutex;
    pthread_cond_t *startCond;
    int playerIndex;
    void *eventQueue;
    pthread_mutex_t *roomMutex;
} ThreadData;

typedef struct Client
{
    int activeSocket;
    int playerIndex;
    int currentWordIndex;
    char *playerWord;
    char **words;
    int charsDone;
    int wordCount;
    char **playerNames;
    int playerCount;
    Times finalTimes;
    bool hasFinalTimes;
} Client;

typedef struct data
{
    int *progress;
    Client *client;
    pthread_mutex_t *mutex;
    bool serverDisconnected;
} data;

#endif // TYPERACERSTRUCTURES_H
