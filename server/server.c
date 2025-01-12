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

    int lineCount;
    char **sentences = readFileLines(fileName, &lineCount);
    if (sentences == NULL)
    {
        perror("Failed to read file lines");
        return -1;
    }
    int sentenceIndex = rand() % lineCount;

    sentences[sentenceIndex][strcspn(sentences[sentenceIndex], "\r\n")] = 0;

    game->socketsData->sentence = malloc(strlen(sentences[sentenceIndex]) * sizeof(char));
    if (game->socketsData->sentence == NULL)
    {
        perror("Failed to allocate memory for sentence");
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

    // Initialize the mutex
    pthread_mutex_t mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t startCond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    pthread_cond_t finishCond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    pthread_cond_t getTimes = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

    // Initialize the RefreshingData
    RefreshingData *refreshingData = malloc(sizeof(RefreshingData));
    if (refreshingData == NULL)
    {
        perror("Failed to allocate memory for refreshingData");
        return -1;
    }
    game->refreshingData = refreshingData;
    refreshingData->progress = calloc(game->maxPlayerCount, sizeof(int));

    // Accept clients
    while (actualPlayerCount < maxPlayers)
    {
        new_sockets[actualPlayerCount] = passive_socket_wait_for_client(serverSocket);
        if (new_sockets[actualPlayerCount] < -1)
        {
            continue;
        }

        Player *player = malloc(sizeof(Player));
        if (player == NULL)
        {
            perror("Failed to allocate memory for player");
            active_socket_destroy(new_sockets[actualPlayerCount]);
            return -1;
        }

        player->activeSocket = new_sockets[actualPlayerCount];
        char *name = malloc(20);
        if (name == NULL)
        {
            perror("Failed to allocate memory for name");
            active_socket_destroy(new_sockets[actualPlayerCount]);
            player->activeSocket = -1;
            return -1;
        }

        int bytes_received = recv(player->activeSocket, name, 20, 0);
        if (bytes_received == 0)
        {
            thread_printf("Client with socket %d didnt send name.\n", player->activeSocket);
            active_socket_destroy(player->activeSocket);
            player->activeSocket = -1;
            return -1;
        }

        name[strcspn(name, "\n")] = 0;
        player->name = name;
        player->finished = false;
        player->disconnected = false;
        player->timeTaken = -1;

        game->socketsData->names[actualPlayerCount] = player->name;
        game->players[actualPlayerCount] = player;

        threadData[actualPlayerCount].refreshingData = refreshingData;
        threadData[actualPlayerCount].mutex = &mutex;
        threadData[actualPlayerCount].startCond = &startCond;
        threadData[actualPlayerCount].finishCond = &finishCond;
        threadData[actualPlayerCount].timeCond = &getTimes;
        threadData[actualPlayerCount].game = game;
        threadData[actualPlayerCount].playerIndex = actualPlayerCount;

        pthread_create(&game->threads[actualPlayerCount], NULL, handleClient, &threadData[actualPlayerCount]);
        game->actualPlayerCount++;
        actualPlayerCount++;
    }

    sleep(1);

    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&startCond);

    while (game->finishedPlayers < game->maxPlayerCount)
    {
        pthread_cond_wait(&finishCond, &mutex);
        game->finishedPlayers++;
        pthread_mutex_unlock(&mutex);
        thread_printf("Finished players: %d\n", game->finishedPlayers);
        pthread_mutex_lock(&mutex);
    }

    pthread_cond_broadcast(&getTimes);
    thread_printf("All players finished broadcasted times\n");
    pthread_mutex_unlock(&mutex);

    thread_printf("Game finished\n");
    for (int i = 0; i < maxPlayers; i++)
    {
        thread_printf("Joining thread %d\n", i);
        pthread_join(game->threads[i], NULL);
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
        t.times[i] = game->players[i]->timeTaken;
    }

    // Send results to clients
    char *timesBuffer = serializeTimes(&t, game->maxPlayerCount);
    int timesBufferSize = strlen(timesBuffer);
    char *timesBufferSizeBuffer = malloc(10);
    if (timesBufferSizeBuffer == NULL)
    {
        perror("Failed to allocate memory for timesBufferSizeBuffer");
        free(timesBuffer);
        free(timesBufferSizeBuffer);
        free(t.times);
        return -1;
    }
    snprintf(timesBufferSizeBuffer, 10, "%d", timesBufferSize);

    for (int i = 0; i < game->maxPlayerCount; i++)
    {
        if (game->players[i]->disconnected == true)
        {
            continue;
        }
        if (game->players[i]->activeSocket == -1 || send(game->players[i]->activeSocket, timesBufferSizeBuffer, 10, 0) < 0)
        {
            perror("Failed to send results length to client");
            free(timesBuffer);
            free(timesBufferSizeBuffer);
            free(t.times);
            return -1;
        }

        if (game->players[i]->activeSocket == -1 || send(game->players[i]->activeSocket, timesBuffer, timesBufferSize, 0) < 0)
        {
            perror("Failed to send result to client");
            free(timesBuffer);
            free(timesBufferSizeBuffer);
            free(t.times);
            return -1;
        }
    }
    free(timesBuffer);
    free(timesBufferSizeBuffer);
    free(t.times);
    thread_printf("Results sent to clients\n");

    return 0;
}

int refresh(Game *game, pthread_mutex_t *mutex, RefreshingData *refreshingData, int *playerIndex)
{
    pthread_mutex_lock(mutex);
    refreshingData->progress[*playerIndex]++;
    char *buffer = serializeRefreshingData(refreshingData, game->maxPlayerCount);
    for (int i = 0; i < game->maxPlayerCount; i++)
    {
        if (game->players[i]->disconnected)
        {
            thread_printf("Player %d disconnected\n", i);
            continue;
        }
        if (send(game->players[i]->activeSocket, buffer, strlen(buffer), 0) < 0)
        {
            perror("Failed to send progress to client");
            return -1;
        }
    }
    free(buffer);
    pthread_mutex_unlock(mutex);
    return 0;
}

void *handleClient(void *arg)
{
    ThreadData *d = (ThreadData *)arg;
    pthread_mutex_lock(d->mutex);
    Game *game = d->game;
    RefreshingData *refreshingData = d->refreshingData;
    int clientSocket = game->players[d->playerIndex]->activeSocket;
    int playerIndex = d->playerIndex;
    pthread_mutex_unlock(d->mutex);

    pthread_mutex_lock(d->mutex);
    pthread_cond_wait(d->startCond, d->mutex);
    pthread_mutex_unlock(d->mutex);

    char *buffer = serializeSocketsData(game->socketsData, playerIndex);
    char buferLength[10];
    sprintf(buferLength, "%d", strlen(buffer));

    if (send(clientSocket, &buferLength, 10, 0) < 0)
    {
        perror("Failed to send json length to client");
        pthread_mutex_lock(d->mutex);
        game->players[playerIndex]->disconnected = true;
        pthread_cond_signal(d->finishCond);
        pthread_mutex_unlock(d->mutex);
        free(buffer);
        return NULL;
    }

    if (send(clientSocket, buffer, strlen(buffer), 0) < 0)
    {
        perror("Failed to send sentence to client");
        pthread_mutex_lock(d->mutex);
        game->players[playerIndex]->disconnected = true;
        pthread_cond_signal(d->finishCond);
        pthread_mutex_unlock(d->mutex);
        free(buffer);
        return NULL;
    }
    free(buffer);

    pthread_mutex_lock(d->mutex);
    char *r = serializeRefreshingData(refreshingData, game->maxPlayerCount);
    if (send(clientSocket, r, strlen(r), 0) < 0)
    {
        perror("Failed to send progress to client");
        game->players[playerIndex]->disconnected = true;
        pthread_cond_signal(d->finishCond);
        pthread_mutex_unlock(d->mutex);
        if (r != NULL)
        {
            free(r);
        }
        return NULL;
    }
    free(r);

    // Game loop
    while (game->players[playerIndex]->finished == false || game->players[playerIndex]->disconnected == false)
    {
        pthread_mutex_unlock(d->mutex);
        char answer;
        int bytes_received = recv(clientSocket, &answer, 1, 0);
        if (bytes_received == 0)
        {
            pthread_mutex_lock(d->mutex);
            game->players[playerIndex]->disconnected = true;
            game->players[playerIndex]->finished = true;
            pthread_cond_signal(d->finishCond);
            break;
        }
        else
        {
            if (answer == '3')
            {
                if (refresh(game, d->mutex, refreshingData, &playerIndex) == -1)
                {
                    perror("Failed to refresh");
                    pthread_mutex_lock(d->mutex);
                    break;
                }
            }
            else if (answer == '2')
            {
                pthread_mutex_lock(d->mutex);
                game->players[playerIndex]->finished = true;
                game->players[playerIndex]->disconnected = false;
                pthread_mutex_unlock(d->mutex);
                if (refresh(game, d->mutex, refreshingData, &playerIndex) == -1)
                {
                    perror("Failed to refresh");
                    break;
                }
                break;
            }
        }
        pthread_mutex_lock(d->mutex);
    }
    pthread_cond_signal(d->finishCond);
    pthread_cond_wait(d->timeCond, d->mutex);
    pthread_mutex_unlock(d->mutex);

    char timeTakenString[50];
    int time_bytes = recv(clientSocket, timeTakenString, 50, 0);
    if (time_bytes == 0)
    {
        perror("Failed to receive time taken from client");
        return NULL;
    }
    double timeTaken = atof(timeTakenString);
    pthread_mutex_lock(d->mutex);
    game->players[playerIndex]->timeTaken = timeTaken;
    pthread_mutex_unlock(d->mutex);

    return NULL;
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
    int maxPlayers = atoi(argv[1]);
    char *fileName = argv[2];
    int logging_temp = atoi(argv[3]);
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