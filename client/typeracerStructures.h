#include <pthread.h>
#include <sys/time.h>

typedef struct Player {
    int activeSocket;
    char* name;
    int currentWordIndex;
    struct timeval startTime, endTime;
} Player;

typedef struct RefreshingData {
    int* progress;
} RefreshingData;

typedef struct SocketsData {
    char** names;
    char* sentence;
    int wordsCount;
    int playerCount;
} SocketsData;

typedef struct Game {
    int serverSocket;
    Player** players;
    int maxPlayerCount; //TODO: Change lobby master logic
    int actualPlayerCount;
    pthread_t* threads;
    pthread_mutex_t* mutex;
    pthread_cond_t* startCond;
    int wordsCount;
    int finishedPlayers;
    SocketsData* data;
    RefreshingData* refreshingData;
} Game;