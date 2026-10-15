#include "server.h"
#include <sys/select.h>
#include <errno.h>
#include <stdatomic.h>

int logging = 0;

void thread_printf(const char *format, ...)
{
    if (logging == 0)
    {
        return; // Skip printing if logging is disabled for this thread
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

static char **gCachedSentences = NULL;
static int gCachedLineCount = 0;
static char gCachedFileName[256] = {0};
static pthread_mutex_t gSentenceCacheMutex = PTHREAD_MUTEX_INITIALIZER;

static char *getRandomSentenceCached(const char *fileName)
{
    pthread_mutex_lock(&gSentenceCacheMutex);
    const char *fn = fileName ? fileName : "input.txt";
    if (!gCachedSentences || strcmp(gCachedFileName, fn) != 0)
    {
        if (gCachedSentences)
        {
            for (int i = 0; i < gCachedLineCount; i++) free(gCachedSentences[i]);
            free(gCachedSentences);
            gCachedSentences = NULL;
            gCachedLineCount = 0;
        }
        gCachedSentences = readFileLines(fn, &gCachedLineCount);
        if (gCachedSentences && gCachedLineCount > 0)
        {
            strncpy(gCachedFileName, fn, sizeof(gCachedFileName) - 1);
            gCachedFileName[sizeof(gCachedFileName) - 1] = '\0';
            for (int i = 0; i < gCachedLineCount; i++)
            {
                gCachedSentences[i][strcspn(gCachedSentences[i], "\r\n")] = '\0';
            }
        }
    }
    char *result = NULL;
    if (gCachedSentences && gCachedLineCount > 0)
    {
        result = strdup(gCachedSentences[rand() % gCachedLineCount]);
    }
    pthread_mutex_unlock(&gSentenceCacheMutex);
    return result;
}

static void freeSentenceCache(void)
{
    pthread_mutex_lock(&gSentenceCacheMutex);
    if (gCachedSentences)
    {
        for (int i = 0; i < gCachedLineCount; i++)
        {
            free(gCachedSentences[i]);
        }
        free(gCachedSentences);
        gCachedSentences = NULL;
        gCachedLineCount = 0;
        gCachedFileName[0] = '\0';
    }
    pthread_mutex_unlock(&gSentenceCacheMutex);
}


// Init Done
int serverInit(Game *game, const char *fileName)
{
    // Initialize game
    game->serverSocket = -1;
    game->actualPlayerCount = 0;
    game->finishedPlayers = 0;
    game->gameStarted = false;
    game->threads = NULL;
    game->socketsData = NULL;
    game->refreshingData = NULL;

    game->players = calloc(game->maxPlayerCount, sizeof(Player *));
    if (game->players == NULL)
    {
        perror("Failed to allocate memory for players");
        return -1;
    }

    // Initialize the threads
    game->threads = (pthread_t *)calloc(game->maxPlayerCount, sizeof(pthread_t));
    if (game->threads == NULL)
    {
        perror("Failed to allocate memory for threads");
        goto fail;
    }

    // Initialize the SocketData
    game->socketsData = malloc(sizeof(SocketsData));
    if (game->socketsData == NULL)
    {
        perror("Failed to allocate memory for socketsData");
        goto fail;
    }
    game->socketsData->sentence = NULL;
    game->socketsData->playerCount = game->maxPlayerCount;
    game->socketsData->names = calloc(game->maxPlayerCount, sizeof(char *));
    if (game->socketsData->names == NULL)
    {
        perror("Failed to allocate memory for names");
        goto fail;
    }

    char *chosen = getRandomSentenceCached(fileName);
    if (chosen == NULL)
    {
        perror("Failed to read file lines or file is empty");
        goto fail;
    }

    if (game->socketsData->sentence)
    {
        free(game->socketsData->sentence);
    }
    game->socketsData->sentence = chosen;
    return 0;

fail:
    if (game->socketsData)
    {
        if (game->socketsData->sentence) free(game->socketsData->sentence);
        if (game->socketsData->names) free(game->socketsData->names);
        free(game->socketsData);
        game->socketsData = NULL;
    }
    if (game->threads)
    {
        free(game->threads);
        game->threads = NULL;
    }
    if (game->players)
    {
        free(game->players);
        game->players = NULL;
    }
    return -1;
}

void broadcastProgress(Game *game, RefreshingData *refreshingData)
{
    if (game == NULL || refreshingData == NULL)
    {
        return;
    }
    char *buffer = serializeRefreshingData(refreshingData, game->maxPlayerCount);
    if (buffer == NULL)
    {
        return;
    }
    size_t len = strlen(buffer);
    for (int i = 0; i < game->maxPlayerCount; i++)
    {
        if (game->players[i] == NULL || game->players[i]->disconnected || game->players[i]->activeSocket < 0)
        {
            continue;
        }
        if (send_all(game->players[i]->activeSocket, buffer, len) < 0)
        {
            thread_printf("Broadcast failed for player %d, shutting down socket\n", i);
            shutdown(game->players[i]->activeSocket, SHUT_RDWR);
            game->players[i]->disconnected = true;
        }
    }
    free(buffer);
}


void *handleClient(void *arg)
{
    ThreadData *d = (ThreadData *)arg;
    Game *game = d->game;
    int playerIndex = d->playerIndex;
    EventQueue *eq = (EventQueue *)d->eventQueue;
    int clientSocket = game->players[playerIndex]->activeSocket;

    // Wait for the Central Hub to signal start, monitoring socket liveness in lobby
    pthread_mutex_lock(d->mutex);
    while (!game->gameStarted)
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 100000000; // 100ms
        if (ts.tv_nsec >= 1000000000)
        {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }
        pthread_cond_timedwait(d->startCond, d->mutex, &ts);

        if (game->gameStarted) break;

        // Check if player disconnected while waiting in the lobby
        char peekBuf[1];
        int peek = recv(clientSocket, peekBuf, 1, MSG_PEEK | MSG_DONTWAIT);
        if (peek == 0 || (peek < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
        {
            thread_printf("Player %d (%s) disconnected while waiting in lobby\n",
                          playerIndex, game->players[playerIndex]->name ? game->players[playerIndex]->name : "unknown");
            if (d->roomMutex) pthread_mutex_lock(d->roomMutex);
            game->players[playerIndex]->disconnected = true;
            active_socket_destroy(clientSocket);
            game->players[playerIndex]->activeSocket = -1;
            if (d->roomMutex) pthread_mutex_unlock(d->roomMutex);
            pthread_mutex_unlock(d->mutex);
            return NULL;
        }
    }
    pthread_mutex_unlock(d->mutex);

    if (clientSocket < 0 || game->players[playerIndex]->disconnected)
    {
        return NULL;
    }

    // READER LOOP: Pure input ingestion pushing to MPSC EventQueue
    while (true)
    {
        if (game->players[playerIndex]->disconnected || game->players[playerIndex]->activeSocket < 0)
        {
            break;
        }
        char answer;
        int bytes_received = recv(clientSocket, &answer, 1, 0);
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);

        if (bytes_received < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {
                continue;
            }
            GameEvent ev;
            ev.type = EVENT_DISCONNECT;
            ev.playerIndex = playerIndex;
            ev.timestamp = ts;
            event_queue_push(eq, ev);
            break;
        }
        else if (bytes_received == 0)
        {
            GameEvent ev;
            ev.type = EVENT_DISCONNECT;
            ev.playerIndex = playerIndex;
            ev.timestamp = ts;
            event_queue_push(eq, ev);
            break;
        }

        if (answer == '3')
        {
            GameEvent ev;
            ev.type = EVENT_WORD_COMPLETED;
            ev.playerIndex = playerIndex;
            ev.timestamp = ts;
            event_queue_push(eq, ev);
        }
        else if (answer == '2')
        {
            GameEvent ev;
            ev.type = EVENT_SENTENCE_FINISHED;
            ev.playerIndex = playerIndex;
            ev.timestamp = ts;
            event_queue_push(eq, ev);

            // Read trailing self-reported time if sent by legacy client/test (non-blocking)
            char drainBuf[64];
            recv(clientSocket, drainBuf, sizeof(drainBuf), MSG_DONTWAIT);
            break;
        }
    }

    return NULL;
}

static void sanitizeRoomCode(char *str)
{
    if (!str) return;
    int j = 0;
    for (int i = 0; str[i]; i++)
    {
        unsigned char c = (unsigned char)str[i];
        if (isalnum(c) || c == '_' || c == '-')
        {
            str[j++] = (char)toupper(c);
        }
    }
    str[j] = '\0';
}

static void sanitizePlayerName(char *str)
{
    if (!str) return;
    int j = 0;
    for (int i = 0; str[i]; i++)
    {
        unsigned char c = (unsigned char)str[i];
        if (isalnum(c) || c == '_' || c == '-' || c == ' ')
        {
            str[j++] = (char)c;
        }
    }
    str[j] = '\0';
}

void parseClientHandshake(const char *rawBuffer, int bytesReceived, char *outRoomCode, size_t roomCodeSize, char *outName, size_t nameSize)
{
    if (outRoomCode && roomCodeSize > 0)
    {
        strncpy(outRoomCode, "DEFAULT", roomCodeSize);
        outRoomCode[roomCodeSize - 1] = '\0';
    }
    if (outName && nameSize > 0)
    {
        strncpy(outName, "Player", nameSize);
        outName[nameSize - 1] = '\0';
    }

    if (rawBuffer == NULL || bytesReceived <= 0)
    {
        return;
    }

    char copy[128];
    size_t copyLen = (bytesReceived < (int)sizeof(copy) - 1) ? (size_t)bytesReceived : sizeof(copy) - 1;
    memcpy(copy, rawBuffer, copyLen);
    copy[copyLen] = '\0';

    // Trim trailing CR/LF/spaces
    while (copyLen > 0 && (copy[copyLen - 1] == '\r' || copy[copyLen - 1] == '\n' || copy[copyLen - 1] == ' '))
    {
        copy[--copyLen] = '\0';
    }

    if (strncmp(copy, "JOIN ", 5) == 0)
    {
        char *ptr = copy + 5;
        while (*ptr == ' ') ptr++;

        char *room = ptr;
        while (*ptr && *ptr != ' ') ptr++;
        if (*ptr)
        {
            *ptr = '\0';
            ptr++;
            while (*ptr == ' ') ptr++;
        }

        char *name = ptr;
        char *end = name + strlen(name);
        while (end > name && (*(end - 1) == ' ' || *(end - 1) == '\r' || *(end - 1) == '\n'))
        {
            *(--end) = '\0';
        }

        if (outRoomCode && roomCodeSize > 0 && strlen(room) > 0)
        {
            strncpy(outRoomCode, room, roomCodeSize);
            outRoomCode[roomCodeSize - 1] = '\0';
        }
        if (outName && nameSize > 0 && strlen(name) > 0)
        {
            strncpy(outName, name, nameSize);
            outName[nameSize - 1] = '\0';
        }
    }
    else
    {
        if (outName && nameSize > 0 && copyLen > 0)
        {
            strncpy(outName, copy, nameSize);
            outName[nameSize - 1] = '\0';
        }
    }

    if (outRoomCode && roomCodeSize > 0)
    {
        sanitizeRoomCode(outRoomCode);
        if (strlen(outRoomCode) == 0)
        {
            strncpy(outRoomCode, "DEFAULT", roomCodeSize);
            outRoomCode[roomCodeSize - 1] = '\0';
        }
    }
    if (outName && nameSize > 0)
    {
        sanitizePlayerName(outName);
        if (strlen(outName) == 0)
        {
            strncpy(outName, "Player", nameSize);
            outName[nameSize - 1] = '\0';
        }
    }
}

int runGameHub(Game *game, EventQueue *eventQueue, pthread_mutex_t *startMutex, pthread_cond_t *startCond)
{
    if (game == NULL || eventQueue == NULL || startMutex == NULL || startCond == NULL)
    {
        return -1;
    }

    // Calculate totalWordCount from sentence using fast shared word counter
    int totalWordCount = countWords(game->socketsData->sentence);

    // Account for any players who disconnected in the lobby prior to race start
    for (int i = 0; i < game->maxPlayerCount; i++)
    {
        Player *p = game->players[i];
        if (p == NULL || p->disconnected || p->activeSocket < 0)
        {
            if (p)
            {
                p->finished = true;
                p->disconnected = true;
                p->timeTaken = -1.0;
            }
            game->finishedPlayers++;
            thread_printf("Player %d was disconnected prior to race start (DNF)\n", i);
        }
    }

    // Send initial SocketsData and RefreshingData to each connected client sequentially from the hub.
    // This guarantees no concurrent socket writes occur between worker threads and progress broadcasts.
    for (int i = 0; i < game->maxPlayerCount; i++)
    {
        if (game->players[i] && !game->players[i]->disconnected && game->players[i]->activeSocket >= 0)
        {
            char *buf = serializeSocketsData(game->socketsData, i);
            if (buf)
            {
                char lenBuf[11] = {0};
                snprintf(lenBuf, sizeof(lenBuf), "%zu", strlen(buf));
                send_all(game->players[i]->activeSocket, lenBuf, 10);
                send_all(game->players[i]->activeSocket, buf, strlen(buf));
                free(buf);
            }
            char *r = serializeRefreshingData(game->refreshingData, game->maxPlayerCount);
            if (r)
            {
                send_all(game->players[i]->activeSocket, r, strlen(r));
                free(r);
            }
        }
    }

    // Record server monotonic race start timestamp
    struct timespec raceStartTime;
    clock_gettime(CLOCK_MONOTONIC, &raceStartTime);

    // Signal all client worker threads to enter their pure reader loop
    pthread_mutex_lock(startMutex);
    game->gameStarted = true;
    pthread_cond_broadcast(startCond);
    pthread_mutex_unlock(startMutex);

    thread_printf("Central Room Hub started event loop (totalWordCount=%d)\n", totalWordCount);

    // CENTRAL ROOM HUB EVENT LOOP:
    // Exclusively owns game state and sequentially processes client events.
    while (game->finishedPlayers < game->maxPlayerCount)
    {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double totalElapsed = (now.tv_sec - raceStartTime.tv_sec) +
                              (now.tv_nsec - raceStartTime.tv_nsec) / 1e9;
        if (totalElapsed > 180.0)
        {
            thread_printf("Race exceeded maximum duration (180s). Timing out unfinished players.\n");
            for (int i = 0; i < game->maxPlayerCount; i++)
            {
                Player *p = game->players[i];
                if (p && !p->finished)
                {
                    p->finished = true;
                    p->timeTaken = -1.0; // DNF
                    game->finishedPlayers++;
                }
            }
            broadcastProgress(game, game->refreshingData);
            break;
        }

        GameEvent ev;
        if (!event_queue_pop(eventQueue, &ev, 50))
        {
            continue;
        }

        if (ev.playerIndex < 0 || ev.playerIndex >= game->maxPlayerCount)
        {
            continue;
        }

        Player *player = game->players[ev.playerIndex];
        if (player == NULL)
        {
            continue;
        }

        if (ev.type == EVENT_WORD_COMPLETED)
        {
            if (!player->finished && !player->disconnected)
            {
                // Anti-cheat verification: progress cannot exceed totalWordCount
                if (game->refreshingData && game->refreshingData->progress &&
                    game->refreshingData->progress[ev.playerIndex] < totalWordCount)
                {
                    game->refreshingData->progress[ev.playerIndex]++;
                    broadcastProgress(game, game->refreshingData);
                }
            }
        }
        else if (ev.type == EVENT_SENTENCE_FINISHED)
        {
            if (!player->finished && !player->disconnected)
            {
                int currentProgress = (game->refreshingData && game->refreshingData->progress)
                                          ? game->refreshingData->progress[ev.playerIndex]
                                          : 0;
                if (currentProgress >= totalWordCount - 1)
                {
                    player->finished = true;
                    if (game->refreshingData && game->refreshingData->progress)
                    {
                        game->refreshingData->progress[ev.playerIndex] = totalWordCount;
                    }

                    // Server-authoritative timing calculation using monotonic clock
                    double elapsed = (ev.timestamp.tv_sec - raceStartTime.tv_sec) +
                                     (ev.timestamp.tv_nsec - raceStartTime.tv_nsec) / 1e9;
                    if (elapsed < 0.0)
                    {
                        elapsed = 0.0;
                    }
                    player->timeTaken = elapsed;
                    game->finishedPlayers++;

                    thread_printf("Player %d (%s) finished in %.2f s (authoritative)\n",
                                  ev.playerIndex, player->name, player->timeTaken);

                    broadcastProgress(game, game->refreshingData);
                }
                else
                {
                    // Premature finish attempt: disqualify racer (DNF) to prevent lobby deadlock
                    player->finished = true;
                    player->timeTaken = -1.0; // DNF
                    game->finishedPlayers++;
                    thread_printf("Player %d (%s) finished prematurely (progress %d < %d): disqualified (DNF)\n",
                                  ev.playerIndex, player->name, currentProgress, totalWordCount - 1);
                    broadcastProgress(game, game->refreshingData);
                }
            }
        }
        else if (ev.type == EVENT_DISCONNECT)
        {
            if (!player->finished)
            {
                player->finished = true;
                player->disconnected = true;
                player->timeTaken = -1.0; // DNF
                game->finishedPlayers++;
                thread_printf("Player %d (%s) disconnected before finishing (DNF)\n",
                              ev.playerIndex, player->name);
                broadcastProgress(game, game->refreshingData);
            }
            else if (!player->disconnected)
            {
                player->disconnected = true;
                thread_printf("Player %d (%s) disconnected after finishing\n",
                              ev.playerIndex, player->name);
                broadcastProgress(game, game->refreshingData);
            }
        }
    }

    event_queue_close(eventQueue);
    thread_printf("All players finished or disconnected. Joining reader threads.\n");

    // Unblock any reader thread that may still be waiting inside recv() (e.g. on timeout or abort)
    for (int i = 0; i < game->maxPlayerCount; i++)
    {
        if (game->players[i] && !game->players[i]->finished && !game->players[i]->disconnected && game->players[i]->activeSocket >= 0)
        {
            shutdown(game->players[i]->activeSocket, SHUT_RD);
        }
    }

    for (int i = 0; i < game->maxPlayerCount; i++)
    {
        if (game->threads[i] != 0)
        {
            thread_printf("Joining thread %d\n", i);
            pthread_join(game->threads[i], NULL);
            game->threads[i] = 0;
        }
    }

    Times t;
    t.times = malloc(game->maxPlayerCount * sizeof(double));
    if (t.times == NULL)
    {
        perror("Failed to allocate memory for times");
        return -1;
    }

    for (int i = 0; i < game->maxPlayerCount; i++)
    {
        t.times[i] = game->players[i] ? game->players[i]->timeTaken : -1.0;
    }

    // Send results to clients using send_all
    char *timesBuffer = serializeTimes(&t, game->maxPlayerCount);
    if (timesBuffer != NULL)
    {
        int timesBufferSize = strlen(timesBuffer);
        for (int i = 0; i < game->maxPlayerCount; i++)
        {
            if (game->players[i] == NULL || game->players[i]->disconnected || game->players[i]->activeSocket < 0)
            {
                continue;
            }
            if (send_all(game->players[i]->activeSocket, timesBuffer, timesBufferSize) < 0 ||
                send_all(game->players[i]->activeSocket, "\n", 1) < 0)
            {
                perror("Failed to send result to client");
                game->players[i]->disconnected = true;
                continue;
            }
            thread_printf("Sent results to client %d: '%s'\n", i, timesBuffer);
        }
        free(timesBuffer);
    }

    free(t.times);
    thread_printf("Results sent to clients\n");

    return 0;
}

static pthread_mutex_t gCoordinatorMutex = PTHREAD_MUTEX_INITIALIZER;
static RoomHub *gRoomHead = NULL;
volatile sig_atomic_t gCoordinatorRunning = 0;
static int gCoordinatorServerSocket = -1;
static atomic_int gActiveConnectionWorkers = 0;

#define MAX_COORDINATOR_ROOMS 32

static int coordinator_count_rooms_unlocked(void)
{
    int count = 0;
    for (RoomHub *cur = gRoomHead; cur != NULL; cur = cur->next)
    {
        count++;
    }
    return count;
}

static int countActivePlayers(Game *game)
{
    if (!game || !game->players) return 0;
    int count = 0;
    for (int i = 0; i < game->actualPlayerCount; i++)
    {
        if (game->players[i] && !game->players[i]->disconnected)
        {
            count++;
        }
    }
    return count;
}

static void generateRoomSummary(RoomHub *hub)
{
    if (hub == NULL || hub->game == NULL) return;
    Game *game = hub->game;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "roomCode", hub->roomCode);
    cJSON_AddStringToObject(root, "state", "FINISHED");
    cJSON_AddNumberToObject(root, "maxPlayers", game->maxPlayerCount);
    cJSON_AddNumberToObject(root, "finishedPlayers", game->finishedPlayers);
    cJSON *pArr = cJSON_CreateArray();
    for (int i = 0; i < game->maxPlayerCount; i++)
    {
        Player *p = game->players[i];
        if (!p) continue;
        cJSON *pObj = cJSON_CreateObject();
        cJSON_AddStringToObject(pObj, "name", p->name ? p->name : "Unknown");
        cJSON_AddNumberToObject(pObj, "time", p->timeTaken);
        cJSON_AddBoolToObject(pObj, "disconnected", p->disconnected);
        cJSON_AddItemToArray(pArr, pObj);
    }
    cJSON_AddItemToObject(root, "players", pArr);
    char *jsonStr = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (jsonStr)
    {
        strncpy(hub->summaryJson, jsonStr, sizeof(hub->summaryJson) - 1);
        hub->summaryJson[sizeof(hub->summaryJson) - 1] = '\0';
        free(jsonStr);
    }
}

static void *roomRunner(void *arg)
{
    RoomHub *hub = (RoomHub *)arg;
    if (hub == NULL) return NULL;

    pthread_mutex_lock(&hub->roomMutex);
    while (countActivePlayers(hub->game) < hub->game->maxPlayerCount && !hub->closeRequested)
    {
        pthread_cond_wait(&hub->roomCond, &hub->roomMutex);
    }
    if (hub->closeRequested)
    {
        pthread_mutex_unlock(&hub->roomMutex);
        return NULL;
    }
    hub->state = ROOM_STATE_IN_PROGRESS;
    pthread_mutex_unlock(&hub->roomMutex);

    thread_printf("Room '%s' is full. Starting race!\n", hub->roomCode);
    usleep(50000);

    runGameHub(hub->game, &hub->eventQueue, &hub->startMutex, &hub->startCond);

    pthread_mutex_lock(&hub->roomMutex);
    hub->state = ROOM_STATE_FINISHED;
    hub->finishedAt = time(NULL);
    generateRoomSummary(hub);
    pthread_mutex_unlock(&hub->roomMutex);

    thread_printf("Room '%s' finished race. Summary ready.\n", hub->roomCode);
    return NULL;
}

RoomHub *coordinator_create_room(const char *roomCode, int maxPlayers, const char *fileName)
{
    if (roomCode == NULL || roomCode[0] == '\0') return NULL;
    if (coordinator_count_rooms_unlocked() >= MAX_COORDINATOR_ROOMS)
    {
        thread_printf("Room limit reached (%d max). Cannot create room '%s'\n", MAX_COORDINATOR_ROOMS, roomCode);
        return NULL;
    }
    if (maxPlayers < 1) maxPlayers = 2;
    if (maxPlayers > 16) maxPlayers = 16;

    RoomHub *hub = calloc(1, sizeof(RoomHub));
    if (!hub) return NULL;

    strncpy(hub->roomCode, roomCode, sizeof(hub->roomCode) - 1);
    hub->maxPlayers = maxPlayers;
    hub->state = ROOM_STATE_WAITING;
    hub->createdAt = time(NULL);
    hub->finishedAt = 0;
    hub->closeRequested = false;

    pthread_mutex_init(&hub->roomMutex, NULL);
    pthread_cond_init(&hub->roomCond, NULL);
    event_queue_init(&hub->eventQueue);
    pthread_mutex_init(&hub->startMutex, NULL);
    pthread_cond_init(&hub->startCond, NULL);

    hub->game = calloc(1, sizeof(Game));
    if (!hub->game)
    {
        event_queue_destroy(&hub->eventQueue);
        pthread_mutex_destroy(&hub->startMutex);
        pthread_cond_destroy(&hub->startCond);
        pthread_mutex_destroy(&hub->roomMutex);
        pthread_cond_destroy(&hub->roomCond);
        free(hub);
        return NULL;
    }
    hub->game->maxPlayerCount = maxPlayers;
    strncpy(hub->game->roomCode, roomCode, sizeof(hub->game->roomCode) - 1);

    if (serverInit(hub->game, fileName) < 0)
    {
        event_queue_destroy(&hub->eventQueue);
        pthread_mutex_destroy(&hub->startMutex);
        pthread_cond_destroy(&hub->startCond);
        pthread_mutex_destroy(&hub->roomMutex);
        pthread_cond_destroy(&hub->roomCond);
        free(hub->game);
        free(hub);
        return NULL;
    }

    hub->game->refreshingData = calloc(1, sizeof(RefreshingData));
    hub->game->refreshingData->progress = calloc(maxPlayers, sizeof(int));

    hub->threadData = calloc(maxPlayers, sizeof(ThreadData));

    int threadRc = pthread_create(&hub->runnerThread, NULL, roomRunner, hub);
    if (threadRc != 0)
    {
        thread_printf("Failed to create runner thread for room '%s'\n", roomCode);
        hub->runnerStarted = false;
        hub->runnerThread = 0;
        event_queue_destroy(&hub->eventQueue);
        pthread_mutex_destroy(&hub->startMutex);
        pthread_cond_destroy(&hub->startCond);
        pthread_mutex_destroy(&hub->roomMutex);
        pthread_cond_destroy(&hub->roomCond);
        if (hub->threadData) free(hub->threadData);
        serverDestroy(hub->game);
        free(hub);
        return NULL;
    }
    hub->runnerStarted = true;

    hub->next = gRoomHead;
    gRoomHead = hub;

    return hub;
}

RoomHub *coordinator_get_room(const char *roomCode)
{
    if (roomCode == NULL) return NULL;
    RoomHub *cur = gRoomHead;
    while (cur)
    {
        if (strcmp(cur->roomCode, roomCode) == 0)
        {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

static RoomHub *coordinator_extract_room_unlocked(const char *roomCode)
{
    RoomHub *prev = NULL;
    RoomHub *cur = gRoomHead;
    while (cur)
    {
        if (strcmp(cur->roomCode, roomCode) == 0)
        {
            if (prev)
            {
                prev->next = cur->next;
            }
            else
            {
                gRoomHead = cur->next;
            }
            cur->next = NULL;
            return cur;
        }
        prev = cur;
        cur = cur->next;
    }
    return NULL;
}

void coordinator_destroy_room(RoomHub *hub)
{
    if (hub == NULL) return;

    hub->closeRequested = true;

    // 1. Shutdown sockets cleanly so any blocking recv() in worker threads wakes up
    if (hub->game != NULL && hub->game->players != NULL)
    {
        for (int i = 0; i < hub->game->actualPlayerCount; i++)
        {
            if (hub->game->players[i] && hub->game->players[i]->activeSocket >= 0)
            {
                shutdown(hub->game->players[i]->activeSocket, SHUT_RDWR);
            }
        }
    }

    // 2. Wake and unblock any client threads waiting to start
    pthread_mutex_lock(&hub->startMutex);
    if (hub->game)
    {
        hub->game->gameStarted = true;
    }
    pthread_cond_broadcast(&hub->startCond);
    pthread_mutex_unlock(&hub->startMutex);

    // 3. Wake room runner
    pthread_mutex_lock(&hub->roomMutex);
    pthread_cond_signal(&hub->roomCond);
    pthread_mutex_unlock(&hub->roomMutex);

    // 4. Join runner thread safely
    if (hub->runnerStarted)
    {
        pthread_join(hub->runnerThread, NULL);
        hub->runnerStarted = false;
    }

    // 5. Join any remaining worker threads
    if (hub->game != NULL && hub->game->threads != NULL)
    {
        for (int i = 0; i < hub->game->actualPlayerCount; i++)
        {
            if (hub->game->threads[i] != 0)
            {
                pthread_join(hub->game->threads[i], NULL);
                hub->game->threads[i] = 0;
            }
        }
    }

    // 6. Close and destroy player sockets now that worker threads have safely terminated
    if (hub->game != NULL && hub->game->players != NULL)
    {
        for (int i = 0; i < hub->game->actualPlayerCount; i++)
        {
            if (hub->game->players[i] && hub->game->players[i]->activeSocket >= 0)
            {
                active_socket_destroy(hub->game->players[i]->activeSocket);
                hub->game->players[i]->activeSocket = -1;
            }
        }
    }

    event_queue_destroy(&hub->eventQueue);
    pthread_mutex_destroy(&hub->startMutex);
    pthread_cond_destroy(&hub->startCond);
    pthread_mutex_destroy(&hub->roomMutex);
    pthread_cond_destroy(&hub->roomCond);

    if (hub->threadData)
    {
        free(hub->threadData);
    }
    if (hub->game)
    {
        serverDestroy(hub->game);
    }
    free(hub);
}

static RoomHub *coordinator_prune_finished_rooms_unlocked(int maxAgeSeconds)
{
    time_t now = time(NULL);
    RoomHub *toDestroy = NULL;
    RoomHub **curr = &gRoomHead;
    while (*curr)
    {
        RoomHub *hub = *curr;
        pthread_mutex_lock(&hub->roomMutex);
        bool pruneFinished = (hub->state == ROOM_STATE_FINISHED && (now - hub->finishedAt) >= maxAgeSeconds);
        bool pruneAbandoned = (hub->state == ROOM_STATE_WAITING && countActivePlayers(hub->game) == 0 && (now - hub->createdAt) >= 300);
        pthread_mutex_unlock(&hub->roomMutex);
        if (pruneFinished || pruneAbandoned)
        {
            *curr = hub->next;
            hub->next = toDestroy;
            toDestroy = hub;
        }
        else
        {
            curr = &(*curr)->next;
        }
    }
    return toDestroy;
}

typedef struct CoordinatorConn {
    int clientSocket;
    char fileName[256];
    int defaultMaxPlayers;
} CoordinatorConn;

static void *coordinator_connection_worker(void *arg)
{
    CoordinatorConn *conn = (CoordinatorConn *)arg;
    int clientSocket = conn->clientSocket;
    int defaultMaxPlayers = conn->defaultMaxPlayers;
    const char *actualFileName = conn->fileName;

    struct timeval rcvTimeout = {2, 0}; // 2-second timeout to prevent indefinite hangs during handshake
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &rcvTimeout, sizeof(rcvTimeout));

    char buffer[128] = {0};
    int bytes = 0;
    while (bytes < (int)sizeof(buffer) - 1)
    {
        int r = recv(clientSocket, buffer + bytes, 1, 0);
        if (r <= 0)
        {
            if (r < 0 && errno == EINTR) continue;
            break;
        }
        bytes += r;
        if (buffer[bytes - 1] == '\n') break;
    }
    if (bytes <= 0)
    {
        active_socket_destroy(clientSocket);
        free(conn);
        atomic_fetch_sub(&gActiveConnectionWorkers, 1);
        return NULL;
    }

    if (bytes == (int)sizeof(buffer) - 1 && buffer[bytes - 1] != '\n')
    {
        char drainChar;
        while (recv(clientSocket, &drainChar, 1, 0) > 0)
        {
            if (drainChar == '\n') break;
        }
    }
    buffer[bytes] = '\0';

    // 1. INFO command
    if (strncmp(buffer, "INFO", 4) == 0)
    {
        pthread_mutex_lock(&gCoordinatorMutex);
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "status", "healthy");
        cJSON *roomsArray = cJSON_CreateArray();
        for (RoomHub *cur = gRoomHead; cur != NULL; cur = cur->next)
        {
            pthread_mutex_lock(&cur->roomMutex);
            cJSON *rObj = cJSON_CreateObject();
            cJSON_AddStringToObject(rObj, "code", cur->roomCode);
            const char *st = (cur->state == ROOM_STATE_WAITING) ? "WAITING" :
                             (cur->state == ROOM_STATE_IN_PROGRESS) ? "IN_PROGRESS" : "FINISHED";
            cJSON_AddStringToObject(rObj, "state", st);
            int currentPlayers = countActivePlayers(cur->game);
            cJSON_AddNumberToObject(rObj, "players", currentPlayers);
            cJSON_AddNumberToObject(rObj, "maxPlayers", cur->maxPlayers);
            pthread_mutex_unlock(&cur->roomMutex);
            cJSON_AddItemToArray(roomsArray, rObj);
        }
        cJSON_AddItemToObject(root, "rooms", roomsArray);
        char *jsonStr = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        pthread_mutex_unlock(&gCoordinatorMutex);

        if (jsonStr)
        {
            send_all(clientSocket, jsonStr, strlen(jsonStr));
            send_all(clientSocket, "\n", 1);
            free(jsonStr);
        }
        active_socket_destroy(clientSocket);
        free(conn);
        atomic_fetch_sub(&gActiveConnectionWorkers, 1);
        return NULL;
    }

    // 2. CREATE command: CREATE <roomCode> <maxPlayers>
    if (strncmp(buffer, "CREATE ", 7) == 0)
    {
        char rawRoom[64] = {0};
        int reqMax = defaultMaxPlayers;
        sscanf(buffer + 7, "%63s %d", rawRoom, &reqMax);
        char reqRoom[16] = {0};
        strncpy(reqRoom, rawRoom, sizeof(reqRoom) - 1);
        sanitizeRoomCode(reqRoom);
        if (strlen(reqRoom) == 0) strncpy(reqRoom, "DEFAULT", sizeof(reqRoom) - 1);
        if (reqMax < 1) reqMax = 2;
        if (reqMax > 16) reqMax = 16;

        RoomHub *roomToDestroy = NULL;
        pthread_mutex_lock(&gCoordinatorMutex);
        RoomHub *existing = coordinator_get_room(reqRoom);
        if (existing != NULL)
        {
            pthread_mutex_lock(&existing->roomMutex);
            bool isFinished = (existing->state == ROOM_STATE_FINISHED);
            pthread_mutex_unlock(&existing->roomMutex);
            if (isFinished)
            {
                roomToDestroy = coordinator_extract_room_unlocked(reqRoom);
                existing = NULL;
            }
        }
        if (existing != NULL)
        {
            pthread_mutex_unlock(&gCoordinatorMutex);
            if (roomToDestroy) coordinator_destroy_room(roomToDestroy);
            char resp[128];
            snprintf(resp, sizeof(resp), "{\"status\":\"error\",\"message\":\"room_already_exists\",\"room\":\"%s\"}\n", reqRoom);
            send_all(clientSocket, resp, strlen(resp));
            active_socket_destroy(clientSocket);
            free(conn);
            atomic_fetch_sub(&gActiveConnectionWorkers, 1);
            return NULL;
        }
        if (coordinator_count_rooms_unlocked() >= MAX_COORDINATOR_ROOMS)
        {
            pthread_mutex_unlock(&gCoordinatorMutex);
            if (roomToDestroy) coordinator_destroy_room(roomToDestroy);
            char resp[128];
            snprintf(resp, sizeof(resp), "{\"status\":\"error\",\"message\":\"room_limit_reached\",\"room\":\"%s\"}\n", reqRoom);
            send_all(clientSocket, resp, strlen(resp));
            active_socket_destroy(clientSocket);
            free(conn);
            atomic_fetch_sub(&gActiveConnectionWorkers, 1);
            return NULL;
        }
        RoomHub *created = coordinator_create_room(reqRoom, reqMax, actualFileName);
        pthread_mutex_unlock(&gCoordinatorMutex);

        if (roomToDestroy)
        {
            coordinator_destroy_room(roomToDestroy);
        }

        char resp[128];
        if (created)
        {
            snprintf(resp, sizeof(resp), "{\"status\":\"ok\",\"room\":\"%s\",\"maxPlayers\":%d}\n", reqRoom, reqMax);
        }
        else
        {
            snprintf(resp, sizeof(resp), "{\"status\":\"error\",\"message\":\"room_creation_failed\",\"room\":\"%s\"}\n", reqRoom);
        }
        send_all(clientSocket, resp, strlen(resp));
        active_socket_destroy(clientSocket);
        free(conn);
        atomic_fetch_sub(&gActiveConnectionWorkers, 1);
        return NULL;
    }

    // 3. SUMMARY command: SUMMARY <roomCode>
    if (strncmp(buffer, "SUMMARY ", 8) == 0)
    {
        char reqRoom[16] = {0};
        sscanf(buffer + 8, "%15s", reqRoom);
        sanitizeRoomCode(reqRoom);

        pthread_mutex_lock(&gCoordinatorMutex);
        RoomHub *existing = coordinator_get_room(reqRoom);
        char resp[4096] = {0};
        if (existing != NULL)
        {
            pthread_mutex_lock(&existing->roomMutex);
            if (existing->summaryJson[0] != '\0')
            {
                snprintf(resp, sizeof(resp), "%s\n", existing->summaryJson);
            }
            else
            {
                snprintf(resp, sizeof(resp), "{\"status\":\"in_progress\",\"room\":\"%s\"}\n", reqRoom);
            }
            pthread_mutex_unlock(&existing->roomMutex);
        }
        else
        {
            snprintf(resp, sizeof(resp), "{\"error\":\"not_found\",\"room\":\"%s\"}\n", reqRoom);
        }
        pthread_mutex_unlock(&gCoordinatorMutex);

        send_all(clientSocket, resp, strlen(resp));
        active_socket_destroy(clientSocket);
        free(conn);
        atomic_fetch_sub(&gActiveConnectionWorkers, 1);
        return NULL;
    }

    // 4. Validate JOIN command or legacy player name
    bool isJoinCmd = (strncmp(buffer, "JOIN ", 5) == 0);
    bool isValidLegacy = false;
    if (!isJoinCmd)
    {
        char firstWord[32] = {0};
        if (sscanf(buffer, "%31s", firstWord) == 1 && strlen(firstWord) > 0)
        {
            if (strncmp(firstWord, "GET", 3) != 0 &&
                strncmp(firstWord, "POST", 4) != 0 &&
                strncmp(firstWord, "HEAD", 4) != 0 &&
                strncmp(firstWord, "OPTIONS", 7) != 0 &&
                strchr(buffer, '/') == NULL &&
                strchr(buffer, '\\') == NULL)
            {
                isValidLegacy = true;
            }
        }
    }

    if (!isJoinCmd && !isValidLegacy)
    {
        const char *unknownCmd = "ERROR: Unknown command\n";
        send_all(clientSocket, unknownCmd, strlen(unknownCmd));
        active_socket_destroy(clientSocket);
        free(conn);
        atomic_fetch_sub(&gActiveConnectionWorkers, 1);
        return NULL;
    }

    // 5. JOIN handshake
    char roomCode[16] = {0};
    char playerName[32] = {0};
    parseClientHandshake(buffer, bytes, roomCode, sizeof(roomCode), playerName, sizeof(playerName));

    RoomHub *roomToDestroy = NULL;
    pthread_mutex_lock(&gCoordinatorMutex);
    RoomHub *hub = coordinator_get_room(roomCode);
    if (hub != NULL)
    {
        pthread_mutex_lock(&hub->roomMutex);
        bool isFinished = (hub->state == ROOM_STATE_FINISHED);
        pthread_mutex_unlock(&hub->roomMutex);
        if (isFinished)
        {
            roomToDestroy = coordinator_extract_room_unlocked(roomCode);
            hub = NULL;
        }
    }
    if (hub == NULL)
    {
        hub = coordinator_create_room(roomCode, defaultMaxPlayers, actualFileName);
    }

    if (hub == NULL)
    {
        pthread_mutex_unlock(&gCoordinatorMutex);
        if (roomToDestroy)
        {
            coordinator_destroy_room(roomToDestroy);
        }
        const char *errMsg = "ROOM_FULL_OR_ACTIVE\n";
        send_all(clientSocket, errMsg, strlen(errMsg));
        active_socket_destroy(clientSocket);
        free(conn);
        atomic_fetch_sub(&gActiveConnectionWorkers, 1);
        return NULL;
    }

    // Lock the room mutex and unlock gCoordinatorMutex early so slot recycling and player registration
    // do not block other coordinator commands, and state/capacity are inspected safely under roomMutex
    pthread_mutex_lock(&hub->roomMutex);
    pthread_mutex_unlock(&gCoordinatorMutex);

    if (roomToDestroy)
    {
        coordinator_destroy_room(roomToDestroy);
        roomToDestroy = NULL;
    }

    if (hub->state != ROOM_STATE_WAITING || countActivePlayers(hub->game) >= hub->game->maxPlayerCount)
    {
        pthread_mutex_unlock(&hub->roomMutex);
        const char *errMsg = "ROOM_FULL_OR_ACTIVE\n";
        send_all(clientSocket, errMsg, strlen(errMsg));
        active_socket_destroy(clientSocket);
        free(conn);
        atomic_fetch_sub(&gActiveConnectionWorkers, 1);
        return NULL;
    }

    Player *p = malloc(sizeof(Player));
    char *pName = strdup(playerName);
    if (!p || !pName)
    {
        if (p) free(p);
        if (pName) free(pName);
        pthread_mutex_unlock(&hub->roomMutex);
        const char *errMsg = "SERVER_ERROR\n";
        send_all(clientSocket, errMsg, strlen(errMsg));
        active_socket_destroy(clientSocket);
        free(conn);
        atomic_fetch_sub(&gActiveConnectionWorkers, 1);
        return NULL;
    }

    // Reuse any slot vacated by a disconnected player in this waiting lobby
    int playerIdx = -1;
    for (int i = 0; i < hub->game->actualPlayerCount; i++)
    {
        if (hub->game->players[i] && hub->game->players[i]->disconnected)
        {
            playerIdx = i;
            if (hub->game->threads[i] != 0)
            {
                pthread_join(hub->game->threads[i], NULL);
                hub->game->threads[i] = 0;
            }
            if (hub->game->players[i]->name) free(hub->game->players[i]->name);
            free(hub->game->players[i]);
            hub->game->players[i] = NULL;
            break;
        }
    }
    if (playerIdx == -1)
    {
        playerIdx = hub->game->actualPlayerCount++;
    }

    p->activeSocket = clientSocket;
    p->name = pName;
    p->finished = false;
    p->disconnected = false;
    p->timeTaken = -1.0;

    hub->game->players[playerIdx] = p;
    hub->game->socketsData->names[playerIdx] = p->name;

    hub->threadData[playerIdx].refreshingData = hub->game->refreshingData;
    hub->threadData[playerIdx].mutex = &hub->startMutex;
    hub->threadData[playerIdx].startCond = &hub->startCond;
    hub->threadData[playerIdx].game = hub->game;
    hub->threadData[playerIdx].playerIndex = playerIdx;
    hub->threadData[playerIdx].eventQueue = &hub->eventQueue;
    hub->threadData[playerIdx].roomMutex = &hub->roomMutex;

    // Reset SO_RCVTIMEO so race typing does not timeout or drop players
    struct timeval noTimeout = {0, 0};
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &noTimeout, sizeof(noTimeout));

    // Set SO_SNDTIMEO (250ms) so slow or stalled network peers do not block central hub broadcasts
    struct timeval sndTimeout = {0, 250000};
    setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, &sndTimeout, sizeof(sndTimeout));

    int threadRc = pthread_create(&hub->game->threads[playerIdx], NULL, handleClient, &hub->threadData[playerIdx]);
    if (threadRc != 0)
    {
        hub->game->threads[playerIdx] = 0;
        p->disconnected = true;
        p->activeSocket = -1;
        pthread_mutex_unlock(&hub->roomMutex);
        const char *errMsg = "SERVER_ERROR\n";
        send_all(clientSocket, errMsg, strlen(errMsg));
        active_socket_destroy(clientSocket);
        free(conn);
        atomic_fetch_sub(&gActiveConnectionWorkers, 1);
        return NULL;
    }

    if (countActivePlayers(hub->game) == hub->game->maxPlayerCount)
    {
        pthread_cond_signal(&hub->roomCond);
    }
    pthread_mutex_unlock(&hub->roomMutex);

    free(conn);
    atomic_fetch_sub(&gActiveConnectionWorkers, 1);
    return NULL;
}

int startCoordinatorServer(const int port, const char *fileName, int defaultMaxPlayers)
{
    if (defaultMaxPlayers < 1) defaultMaxPlayers = 2;
    if (defaultMaxPlayers > 16) defaultMaxPlayers = 16;
    const char *actualFileName = (fileName != NULL) ? fileName : "input.txt";

    gCoordinatorServerSocket = passive_socket_init(port);
    if (gCoordinatorServerSocket < 0)
    {
        perror("Failed to start coordinator listening socket");
        return -1;
    }

    gCoordinatorRunning = true;
    thread_printf("Coordinator daemon running on port %d (defaultMaxPlayers=%d, file=%s)\n",
                  port, defaultMaxPlayers, actualFileName);

    while (gCoordinatorRunning)
    {
        pthread_mutex_lock(&gCoordinatorMutex);
        RoomHub *toDestroy = coordinator_prune_finished_rooms_unlocked(30);
        pthread_mutex_unlock(&gCoordinatorMutex);
        while (toDestroy)
        {
            RoomHub *next = toDestroy->next;
            coordinator_destroy_room(toDestroy);
            toDestroy = next;
        }

        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(gCoordinatorServerSocket, &readFds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000; // 200 ms timeout

        int sel = select(gCoordinatorServerSocket + 1, &readFds, NULL, NULL, &tv);
        if (sel <= 0)
        {
            continue;
        }

        int clientSocket = passive_socket_wait_for_client(gCoordinatorServerSocket);
        if (clientSocket < 0)
        {
            continue;
        }

        if (atomic_load(&gActiveConnectionWorkers) >= 64)
        {
            send_all(clientSocket, "ERROR: Server busy, try again later\n", 36);
            active_socket_destroy(clientSocket);
            continue;
        }

        CoordinatorConn *conn = malloc(sizeof(CoordinatorConn));
        if (!conn)
        {
            active_socket_destroy(clientSocket);
            continue;
        }
        conn->clientSocket = clientSocket;
        strncpy(conn->fileName, actualFileName, sizeof(conn->fileName) - 1);
        conn->fileName[sizeof(conn->fileName) - 1] = '\0';
        conn->defaultMaxPlayers = defaultMaxPlayers;

        atomic_fetch_add(&gActiveConnectionWorkers, 1);

        pthread_t worker;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&worker, &attr, coordinator_connection_worker, conn) != 0)
        {
            atomic_fetch_sub(&gActiveConnectionWorkers, 1);
            active_socket_destroy(clientSocket);
            free(conn);
        }
        pthread_attr_destroy(&attr);
    }

    thread_printf("Coordinator loop ended. Cleaning up.\n");
    return 0;
}

void stopCoordinatorServer(void)
{
    gCoordinatorRunning = false;
    if (gCoordinatorServerSocket >= 0)
    {
        passive_socket_destroy(gCoordinatorServerSocket);
        gCoordinatorServerSocket = -1;
    }

    for (int i = 0; i < 20 && atomic_load(&gActiveConnectionWorkers) > 0; i++)
    {
        usleep(50000); // 50ms wait
    }

    pthread_mutex_lock(&gCoordinatorMutex);
    RoomHub *cur = gRoomHead;
    gRoomHead = NULL;
    pthread_mutex_unlock(&gCoordinatorMutex);

    while (cur)
    {
        RoomHub *next = cur->next;
        coordinator_destroy_room(cur);
        cur = next;
    }
    freeSentenceCache();
}


void serverDestroy(Game *game)
{
    thread_printf("Deleting game\n");
    if (game != NULL)
    {
        if (game->players != NULL)
        {
            for (int i = 0; i < game->maxPlayerCount; i++)
            {
                if (game->players[i] != NULL)
                {
                    if (game->players[i]->name != NULL)
                    {
                        free(game->players[i]->name);
                    }
                    if (game->players[i]->activeSocket != -1)
                    {
                        usleep(20000); // 20ms flush
                        active_socket_destroy(game->players[i]->activeSocket);
                        game->players[i]->activeSocket = -1;
                    }
                    free(game->players[i]);
                }
            }
            free(game->players);
        }

        if (game->socketsData != NULL)
        {
            if (game->socketsData->sentence != NULL)
            {
                free(game->socketsData->sentence);
            }
            if (game->socketsData->names != NULL)
            {
                free(game->socketsData->names);
            }
            free(game->socketsData);
        }

        if (game->threads != NULL)
        {
            free(game->threads);
        }

        if (game->refreshingData != NULL)
        {
            if (game->refreshingData->progress != NULL)
            {
                free(game->refreshingData->progress);
            }
            free(game->refreshingData);
        }
        if (game->serverSocket != -1)
        {
            active_socket_destroy(game->serverSocket);
        }
        free(game);
    }
}