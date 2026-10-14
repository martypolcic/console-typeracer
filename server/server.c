#include "server.h"

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

// Init Done
int serverInit(Game *game, const char *fileName)
{
    // Initialize game
    game->serverSocket = -1;
    game->actualPlayerCount = 0;
    game->players = malloc(game->maxPlayerCount * sizeof(Player *));
    if (game->players == NULL)
    {
        perror("Failed to allocate memory for players");
        return -1;
    }
    game->finishedPlayers = 0;

    // Initialize the threads
    game->threads = (pthread_t *)malloc(game->maxPlayerCount * sizeof(pthread_t));
    if (game->threads == NULL)
    {
        perror("Failed to allocate memory for threads");
        return -1;
    }

    // Initialize the SocketData
    game->socketsData = malloc(sizeof(SocketsData));
    if (game->socketsData == NULL)
    {
        perror("Failed to allocate memory for socketsData");
        return -1;
    }
    game->socketsData->playerCount = game->maxPlayerCount;
    game->socketsData->names = malloc(game->maxPlayerCount * sizeof(char *));
    if (game->socketsData->names == NULL)
    {
        perror("Failed to allocate memory for names");
        return -1;
    }

    int lineCount = 0;
    char **sentences = readFileLines(fileName, &lineCount);
    if (sentences == NULL || lineCount <= 0)
    {
        perror("Failed to read file lines or file is empty");
        return -1;
    }
    int sentenceIndex = rand() % lineCount;

    sentences[sentenceIndex][strcspn(sentences[sentenceIndex], "\r\n")] = 0;

    game->socketsData->sentence = malloc((strlen(sentences[sentenceIndex]) + 1) * sizeof(char));
    if (game->socketsData->sentence == NULL)
    {
        perror("Failed to allocate memory for sentence");
        for (int i = 0; i < lineCount; i++)
        {
            free(sentences[i]);
        }
        free(sentences);
        return -1;
    }
    strcpy(game->socketsData->sentence, sentences[sentenceIndex]);
    for (int i = 0; i < lineCount; i++)
    {
        free(sentences[i]);
    }
    free(sentences);
    return 0;
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
            thread_printf("Broadcast failed for player %d, marking disconnected\n", i);
            game->players[i]->disconnected = true;
        }
    }
    free(buffer);
}

int refresh(Game *game, pthread_mutex_t *mutex, RefreshingData *refreshingData, int *playerIndex)
{
    (void)mutex;
    if (playerIndex != NULL && refreshingData != NULL && *playerIndex >= 0 && *playerIndex < game->maxPlayerCount)
    {
        refreshingData->progress[*playerIndex]++;
    }
    broadcastProgress(game, refreshingData);
    return 0;
}

void *handleClient(void *arg)
{
    ThreadData *d = (ThreadData *)arg;
    Game *game = d->game;
    RefreshingData *refreshingData = d->refreshingData;
    int playerIndex = d->playerIndex;
    EventQueue *eq = (EventQueue *)d->eventQueue;
    int clientSocket = game->players[playerIndex]->activeSocket;

    // Wait for the Central Hub to signal start
    pthread_mutex_lock(d->mutex);
    pthread_cond_wait(d->startCond, d->mutex);
    pthread_mutex_unlock(d->mutex);

    // Serialize and send SocketsData to this client
    char *buffer = serializeSocketsData(game->socketsData, playerIndex);
    if (buffer == NULL)
    {
        GameEvent ev;
        ev.type = EVENT_DISCONNECT;
        ev.playerIndex = playerIndex;
        clock_gettime(CLOCK_MONOTONIC, &ev.timestamp);
        event_queue_push(eq, ev);
        return NULL;
    }

    char buferLength[11] = {0};
    snprintf(buferLength, sizeof(buferLength), "%zu", strlen(buffer));

    if (send_all(clientSocket, buferLength, 10) < 0 ||
        send_all(clientSocket, buffer, strlen(buffer)) < 0)
    {
        perror("Failed to send socketsData to client");
        free(buffer);
        GameEvent ev;
        ev.type = EVENT_DISCONNECT;
        ev.playerIndex = playerIndex;
        clock_gettime(CLOCK_MONOTONIC, &ev.timestamp);
        event_queue_push(eq, ev);
        return NULL;
    }
    free(buffer);

    // Send initial RefreshingData snapshot to this client
    char *r = serializeRefreshingData(refreshingData, game->maxPlayerCount);
    if (r != NULL)
    {
        if (send_all(clientSocket, r, strlen(r)) < 0)
        {
            perror("Failed to send initial refreshingData to client");
            free(r);
            GameEvent ev;
            ev.type = EVENT_DISCONNECT;
            ev.playerIndex = playerIndex;
            clock_gettime(CLOCK_MONOTONIC, &ev.timestamp);
            event_queue_push(eq, ev);
            return NULL;
        }
        free(r);
    }

    // READER LOOP: Pure input ingestion pushing to MPSC EventQueue
    while (true)
    {
        char answer;
        int bytes_received = recv(clientSocket, &answer, 1, 0);
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);

        if (bytes_received <= 0)
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

            // Read trailing self-reported time if sent by legacy client/test
            char drainBuf[64];
            recv(clientSocket, drainBuf, sizeof(drainBuf), 0);
            break;
        }
    }

    return NULL;
}

int startServer(const int port, Game *game)
{
    // Create server socket
    game->serverSocket = passive_socket_init(port);
    if (game->serverSocket < 0)
    {
        return -1;
    }
    thread_printf("Server started on port %d.\n", port);

    int serverSocket = game->serverSocket;
    int maxPlayers = game->maxPlayerCount;
    int actualPlayerCount = game->actualPlayerCount;

    int new_sockets[maxPlayers];
    ThreadData threadData[maxPlayers];

    // Initialize EventQueue
    EventQueue eventQueue;
    event_queue_init(&eventQueue);

    // Initialize synchronization for worker thread launch
    pthread_mutex_t startMutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t startCond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

    // Initialize RefreshingData
    RefreshingData *refreshingData = malloc(sizeof(RefreshingData));
    if (refreshingData == NULL)
    {
        perror("Failed to allocate memory for refreshingData");
        event_queue_destroy(&eventQueue);
        return -1;
    }
    game->refreshingData = refreshingData;
    refreshingData->progress = calloc(game->maxPlayerCount, sizeof(int));

    // Calculate totalWordCount from sentence for authoritative server-side verification
    int totalWordCount = 0;
    char **parsedWords = parseSentence(game->socketsData->sentence, &totalWordCount);
    if (parsedWords)
    {
        for (int i = 0; i < totalWordCount; i++)
        {
            if (parsedWords[i])
            {
                free(parsedWords[i]);
            }
        }
        free(parsedWords);
    }

    // Accept clients
    while (actualPlayerCount < maxPlayers)
    {
        new_sockets[actualPlayerCount] = passive_socket_wait_for_client(serverSocket);
        if (new_sockets[actualPlayerCount] < 0)
        {
            continue;
        }

        Player *player = malloc(sizeof(Player));
        if (player == NULL)
        {
            perror("Failed to allocate memory for player");
            active_socket_destroy(new_sockets[actualPlayerCount]);
            event_queue_destroy(&eventQueue);
            return -1;
        }

        player->activeSocket = new_sockets[actualPlayerCount];
        char *name = calloc(21, sizeof(char));
        if (name == NULL)
        {
            perror("Failed to allocate memory for name");
            active_socket_destroy(new_sockets[actualPlayerCount]);
            player->activeSocket = -1;
            free(player);
            event_queue_destroy(&eventQueue);
            return -1;
        }

        int bytes_received = recv(player->activeSocket, name, 20, 0);
        if (bytes_received <= 0)
        {
            thread_printf("Client with socket %d didnt send name.\n", player->activeSocket);
            active_socket_destroy(player->activeSocket);
            player->activeSocket = -1;
            free(name);
            free(player);
            continue;
        }

        name[bytes_received] = '\0';
        name[strcspn(name, "\r\n")] = 0;
        player->name = name;
        player->finished = false;
        player->disconnected = false;
        player->timeTaken = -1.0;

        game->socketsData->names[actualPlayerCount] = player->name;
        game->players[actualPlayerCount] = player;

        threadData[actualPlayerCount].refreshingData = refreshingData;
        threadData[actualPlayerCount].mutex = &startMutex;
        threadData[actualPlayerCount].startCond = &startCond;
        threadData[actualPlayerCount].finishCond = NULL;
        threadData[actualPlayerCount].timeCond = NULL;
        threadData[actualPlayerCount].game = game;
        threadData[actualPlayerCount].playerIndex = actualPlayerCount;
        threadData[actualPlayerCount].eventQueue = &eventQueue;

        pthread_create(&game->threads[actualPlayerCount], NULL, handleClient, &threadData[actualPlayerCount]);
        game->actualPlayerCount++;
        actualPlayerCount++;
    }

    // Brief pause to allow connection setup to stabilize
    usleep(50000);

    // Record server monotonic race start timestamp
    struct timespec raceStartTime;
    clock_gettime(CLOCK_MONOTONIC, &raceStartTime);

    // Signal all client worker threads to send initial data and start reading
    pthread_mutex_lock(&startMutex);
    pthread_cond_broadcast(&startCond);
    pthread_mutex_unlock(&startMutex);

    thread_printf("Central Room Hub started event loop (totalWordCount=%d)\n", totalWordCount);

    // CENTRAL ROOM HUB EVENT LOOP:
    // Exclusively owns game state and sequentially processes client events.
    while (game->finishedPlayers < game->maxPlayerCount)
    {
        GameEvent ev;
        if (!event_queue_pop(&eventQueue, &ev, 50))
        {
            continue;
        }

        if (ev.playerIndex < 0 || ev.playerIndex >= game->maxPlayerCount)
        {
            continue;
        }

        Player *player = game->players[ev.playerIndex];

        if (ev.type == EVENT_WORD_COMPLETED)
        {
            if (!player->finished && !player->disconnected)
            {
                // Anti-cheat verification: progress cannot exceed totalWordCount
                if (refreshingData->progress[ev.playerIndex] < totalWordCount)
                {
                    refreshingData->progress[ev.playerIndex]++;
                    broadcastProgress(game, refreshingData);
                }
            }
        }
        else if (ev.type == EVENT_SENTENCE_FINISHED)
        {
            if (!player->finished && !player->disconnected)
            {
                player->finished = true;
                refreshingData->progress[ev.playerIndex] = totalWordCount;

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

                broadcastProgress(game, refreshingData);
            }
        }
        else if (ev.type == EVENT_DISCONNECT)
        {
            if (!player->disconnected)
            {
                player->disconnected = true;
                if (!player->finished)
                {
                    player->finished = true;
                    player->timeTaken = -1.0; // DNF
                    game->finishedPlayers++;
                    thread_printf("Player %d (%s) disconnected before finishing (DNF)\n",
                                  ev.playerIndex, player->name);
                }
                else
                {
                    thread_printf("Player %d (%s) disconnected after finishing\n",
                                  ev.playerIndex, player->name);
                }
                broadcastProgress(game, refreshingData);
            }
        }
    }

    event_queue_close(&eventQueue);
    thread_printf("All players finished or disconnected. Joining reader threads.\n");

    for (int i = 0; i < maxPlayers; i++)
    {
        thread_printf("Joining thread %d\n", i);
        pthread_join(game->threads[i], NULL);
    }

    event_queue_destroy(&eventQueue);
    pthread_mutex_destroy(&startMutex);
    pthread_cond_destroy(&startCond);

    Times t;
    t.times = malloc(game->maxPlayerCount * sizeof(double));
    if (t.times == NULL)
    {
        perror("Failed to allocate memory for times");
        return -1;
    }

    for (int i = 0; i < game->maxPlayerCount; i++)
    {
        t.times[i] = game->players[i]->timeTaken;
    }

    // Send results to clients using send_all
    char *timesBuffer = serializeTimes(&t, game->maxPlayerCount);
    int timesBufferSize = strlen(timesBuffer);
    char timesBufferSizeBuffer[11] = {0};
    snprintf(timesBufferSizeBuffer, 10, "%d", timesBufferSize);

    for (int i = 0; i < game->maxPlayerCount; i++)
    {
        if (game->players[i]->disconnected || game->players[i]->activeSocket < 0)
        {
            continue;
        }
        if (send_all(game->players[i]->activeSocket, timesBufferSizeBuffer, 10) < 0)
        {
            perror("Failed to send results length to client");
            game->players[i]->disconnected = true;
            continue;
        }
        if (send_all(game->players[i]->activeSocket, timesBuffer, timesBufferSize) < 0)
        {
            perror("Failed to send result to client");
            game->players[i]->disconnected = true;
            continue;
        }
        thread_printf("Sent results to client %d: len=%s, body='%s'\n", i, timesBufferSizeBuffer, timesBuffer);
    }

    free(timesBuffer);
    free(t.times);
    thread_printf("Results sent to clients\n");

    return 0;
}

void serverDestroy(Game *game)
{
    printf("Deleting game\n");
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

void *startupServer(void *arg)
{
    char **argv = (char **)arg;
    srand(time(NULL));
    int port = 54897;
    // int port = 33827;
    int maxPlayers = (argv[1] != NULL) ? atoi(argv[1]) : 2;
    if (maxPlayers < 1)
    {
        maxPlayers = 2;
    }
    char *fileName = (argv[2] != NULL) ? argv[2] : "input.txt";
    int logging_temp = 0;
    if (argv[3] != NULL)
    {
        logging_temp = atoi(argv[3]);
    }
    if (logging_temp == 1)
    {
        logging = 1;
    }
    else
    {
        logging = 0;
    }

    Game *game = malloc(sizeof(Game));
    if (game == NULL)
    {
        perror("Failed to allocate memory for game");
        return NULL;
    }
    game->maxPlayerCount = maxPlayers;

    thread_printf("Initializing server...\n");
    if (serverInit(game, fileName) == -1)
    {
        thread_printf("Error initializing server.\n");
        serverDestroy(game);
        return NULL;
    }

    thread_printf("Starting server on port %d...\n", port);
    thread_printf("Port: %d, Filename: %s, Game: %p\n", port, fileName, game);
    int started = startServer(port, game);
    if (started == -1)
    {
        thread_printf("Error starting server.\n");
        serverDestroy(game);
        return NULL;
    }

    thread_printf("Game over.\n");
    serverDestroy(game);
    thread_printf("Server shutting down\n");
    return NULL;
}