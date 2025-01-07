#include "server.h"

// Init Done
void serverInit(Game* game, const char* fileName) {
    //Initialize game
    game->serverSocket = -1;
    game->actualPlayerCount = 0;
    game->players = malloc(game->maxPlayerCount * sizeof(Player*));
    for (int i = 0; i < game->maxPlayerCount; i++) {
        game->players[i] = malloc(sizeof(Player));
    }
    game->wordsCount = 0;
    game->finishedPlayers = 0;

    // Initialize the threads
    game->threads = (pthread_t*)malloc(game->maxPlayerCount * sizeof(pthread_t));
    if (game->threads == NULL) {
        perror("Failed to allocate memory for threads");
        exit(EXIT_FAILURE);
    }

    // Initialize the mutex
    game->mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(game->mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        exit(EXIT_FAILURE);
    }

    // Initialize the condition variable
    game->startCond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
    if (pthread_cond_init(game->startCond, NULL) != 0) {
        perror("Failed to initialize condition variable");
        pthread_mutex_destroy(game->mutex); // Clean up mutex if cond init fails
        exit(EXIT_FAILURE);
    }

    // Initialize the SocketData
    game->socketsData = malloc(sizeof(SocketsData));

    // Initialize the RefreshingData
    game->refreshingData = malloc(sizeof(RefreshingData));
    game->refreshingData->progress = malloc(game->maxPlayerCount * sizeof(int));
}

int startServer(const int port, const char* fileName, Game* game) {
    // Create server socket
    game->serverSocket = passive_socket_init(port);
    if (game->serverSocket < 0) {
        return -1;
    }

    int serverSocket = game->serverSocket;
    int maxPlayers = game->maxPlayerCount;
    int actualPlayerCount = game->actualPlayerCount;

    int new_sockets[maxPlayers];

    ThreadData threadData[maxPlayers];

    // Accept clients
    while (actualPlayerCount < maxPlayers) {
        new_sockets[actualPlayerCount] = passive_socket_wait_for_client(serverSocket);
        if (new_sockets[actualPlayerCount] < -1) {
            perror("Cannot accept client");
            continue;
        }

        Player* player = malloc(sizeof(Player));
        if (player == NULL) {
            perror("Failed to allocate memory for player");
            return -1;
        }

        player->activeSocket = new_sockets[actualPlayerCount];
        player->currentWordIndex = 0;
        char* name = malloc(20 * sizeof(char));
        if (name == NULL) {
            perror("Failed to allocate memory for name");
            return -1;
        }

        int bytes_received = recv(player->activeSocket, name, 19, 0);
        if (bytes_received == 0) {
            printf("Client with socket %d didnt send name.\n", player->activeSocket);
            active_socket_destroy(player->activeSocket);
            return -1;
        }

        pthread_mutex_lock(game->mutex);
        game->actualPlayerCount = actualPlayerCount;
        game->players[game->actualPlayerCount] = player;

        threadData[actualPlayerCount].game = game;
        threadData[actualPlayerCount].playerIndex = actualPlayerCount;

        pthread_create(&game->threads[actualPlayerCount], NULL, handleClient, &threadData[actualPlayerCount]);
        pthread_mutex_unlock(game->mutex);
        actualPlayerCount++;
        sleep(1);

    }
    printf("All players connected\n");
    //call cond to start the game
    pthread_mutex_lock(game->mutex);
    
    //Read sentences from file 
    //TODO: optimize to load only one sentence
    int lineCount;
    char** sentences = readFileLines(fileName, &lineCount);
    int sentenceIndex = rand() % lineCount;

    game->socketsData->sentence = malloc(strlen(sentences[sentenceIndex]) * sizeof(char));
    printf("sentence: %s\n", sentences[sentenceIndex]);
    strcpy(game->socketsData->sentence, sentences[sentenceIndex]);
    //rozdel sentence na slova a pridaj slova
    game->finishedPlayers = 0;
    printf("Broadcast is going to send\n");
    pthread_cond_broadcast(game->startCond);
    printf("Broadcast send\n");
    pthread_mutex_unlock(game->mutex);

    for (int i = 0; i < maxPlayers; i++) {
        pthread_mutex_lock(game->mutex);
        pthread_join(game->threads[i], NULL);
        pthread_mutex_unlock(game->mutex);
    }

    return 0;
}

int Refresh(Game* game, int* playerIndex) {
    pthread_mutex_lock(game->mutex);
    int* progress = game->refreshingData->progress;
    progress[*playerIndex] += 1;
    pthread_mutex_unlock(game->mutex);
    return 0;
}

void* handleClient(void* arg) {
    ThreadData* d = (ThreadData*)arg;
    Game* game = d->game;
    int playerIndex = d->playerIndex;

    pthread_mutex_lock(game->mutex);
    SocketsData* data = game->socketsData;
    int clientSocket = game->players[playerIndex]->activeSocket;
    while (game->actualPlayerCount < game->maxPlayerCount) {
        printf("Waiting for players to connect...\n");
        pthread_cond_wait(game->startCond, game->mutex);
    }
    printf("Player: %s\n", game->players[playerIndex]->name);

    pthread_mutex_unlock(d->game->mutex);

    // Send the sentence to the client
    data->sentence = "Ahoj ako sa mas?";
    if (send(clientSocket, data->sentence, strlen(data->sentence), 0) < 0) {
        perror("Failed to send sentence to client");
        return NULL;
    }

    pthread_mutex_lock(d->game->mutex);
    // Game loop
    while (game->finishedPlayers < game->maxPlayerCount) {
        pthread_mutex_unlock(game->mutex);

        char buffer[20] = "Ahoj ako sa mas?";
        int bytes_received = recv(clientSocket, buffer, strlen(buffer), 0);
        if (bytes_received == 0) {
            printf("Client with socket %d disconnected.\n", clientSocket);
            active_socket_destroy(clientSocket);
            break;
        } else {
            printf("Received: %s\n", buffer);
            if (Refresh(game, &playerIndex) == -1) {
                perror("Failed to refresh");
                break;
            }
            // pthread_mutex_lock(game->mutex);
            // game->players[playerIndex]->currentIndex += 1;//checkInput(buffer, game);
            // if (game->players[playerIndex]->currentIndex == 5) {
            //     game->players[playerIndex]->finished = 1;
            // }
            // if (game->players[playerIndex]->finished == 1) {
            //     break;
            // }
        }
        pthread_mutex_lock(game->mutex);
    }
    pthread_mutex_unlock(game->mutex);
    return NULL;
}

int checkInput(char* buffer, Game* game) {
    return 1;
}

int gameDestroy(Game* game) {
    for (int i = 0; i < game->maxPlayerCount; i++) {
        free(game->players[i]->name);
        free(game->players[i]);
    }
    free(game->players);

    pthread_mutex_destroy(game->mutex);
    pthread_cond_destroy(game->startCond);
    return 0;
}
