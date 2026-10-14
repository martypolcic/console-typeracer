#include "client.h"

// Signal handler for SIGINT (Ctrl+C)
void handle_sigint(int sig)
{
    printf("\nCaught signal %d (Ctrl+C). Cleaning up...\n", sig);

    clearScreen();
    restoreBufferedInput();
    setCursorVisibility(true);
    printf("Exiting program safely.\n");

    exit(0); // Exit the program
}

void clear_buffer()
{
    // Clear leftover input buffer (e.g., newline from previous input)
    while (getchar() != '\n' && getchar() != EOF)
        ;
}

int main(int argc, char **argv)
{
    char *serverPath = "./build/server/server";
    char *filePath = "input.txt";
    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN);
    disableBufferedInput();
    setCursorVisibility(false);
    int port = 54897;
    // int port = 33827;
    int mode = menu();
    restoreBufferedInput();
    setCursorVisibility(true);
    pthread_t *serverThread = NULL;
    char **args = malloc(4 * sizeof(char *));
    args[0] = NULL;
    args[1] = malloc(16);
    args[2] = filePath;
    args[3] = malloc(16);
    if (mode == 0)
    {
        free(args[1]);
        free(args[3]);
        free(args);
        return 0;
    }
    else if (mode == 1)
    {
        clearScreen();
        printf("How many players do you want to play with?: ");
        int playerCount;
        if (scanf("%d", &playerCount) != 1)
        {
            playerCount = 0;
        }
        clear_buffer();
        if (playerCount < 2)
        {
            printf("Please provide a number greater than 1.\n");
            free(args[1]);
            free(args[3]);
            free(args);
            return 1;
        }
        int logging = 0;
        snprintf(args[1], 16, "%d", playerCount);
        snprintf(args[3], 16, "%d", logging);
        serverThread = (pthread_t *)malloc(sizeof(pthread_t));
        pthread_create(serverThread, NULL, startupServer, args);
        sleep(1);
    }
    int activeSocket = connect_to_server("localhost", port);
    if (activeSocket < 0)
    {
        printf("Failed to connect to server.\n");
        free(args[1]);
        free(args[3]);
        free(args);
        if (serverThread) free(serverThread);
        return 1;
    }
    clearScreen();
    char name[21] = {0};
    printf("Enter your name: ");
    if (fgets(name, 20, stdin) == NULL || strlen(name) == 0 || name[0] == '\n')
    {
        printf("Please provide a valid name.\n");
        active_socket_destroy(activeSocket);
        free(args[1]);
        free(args[3]);
        free(args);
        if (serverThread) free(serverThread);
        return 1;
    }
    name[strcspn(name, "\r\n")] = 0;

    if (send(activeSocket, name, 20, 0) < 0)
    {
        printf("Failed to send name to server.\n");
        active_socket_destroy(activeSocket);
        free(args[1]);
        free(args[3]);
        free(args);
        if (serverThread) free(serverThread);
        return 1;
    }

    clearScreen();
    disableBufferedInput();
    setCursorVisibility(false);

    printf("Waiting for other players to join\n");
    printf("------------------------------------------------\n");

    char jsonSocketDataLength[11] = {0};
    int bytes_received = recv(activeSocket, jsonSocketDataLength, 10, 0);
    if (bytes_received <= 0)
    {
        printf("Json data length went wrong.\n");
        active_socket_destroy(activeSocket);
        free(args[1]);
        free(args[3]);
        free(args);
        if (serverThread) free(serverThread);
        return 1;
    }
    jsonSocketDataLength[bytes_received] = '\0';

    int socketDataLength = atoi(jsonSocketDataLength);
    if (socketDataLength <= 0)
    {
        printf("Getting JSON error: invalid length\n");
        active_socket_destroy(activeSocket);
        free(args[1]);
        free(args[3]);
        free(args);
        if (serverThread) free(serverThread);
        return 1;
    }
    char *jsonSocketData = calloc(socketDataLength + 1, sizeof(char));
    if (!jsonSocketData)
    {
        active_socket_destroy(activeSocket);
        free(args[1]);
        free(args[3]);
        free(args);
        if (serverThread) free(serverThread);
        return 1;
    }
    bytes_received = recv(activeSocket, jsonSocketData, socketDataLength, 0);
    if (bytes_received <= 0)
    {
        printf("Getting JSON error\n");
        free(jsonSocketData);
        active_socket_destroy(activeSocket);
        free(args[1]);
        free(args[3]);
        free(args);
        if (serverThread) free(serverThread);
        return 1;
    }
    jsonSocketData[bytes_received] = '\0';

    SocketsData socketsData;
    socketsData = deserializeSocketsData(jsonSocketData);
    free(jsonSocketData);

    Client *client = malloc(sizeof(Client));
    client->activeSocket = activeSocket;
    client->playerIndex = socketsData.playerIndex;
    client->currentWordIndex = 0;
    client->playerNames = socketsData.names;
    client->charsDone = 0;
    client->playerCount = socketsData.playerCount;
    client->playerWord = malloc(1);
    client->playerWord[0] = '\0';
    client->words = parseSentence(socketsData.sentence, &client->wordCount);

    gettimeofday(&client->startTime, NULL);
    gameLoop(client);
    double timeTaken = calculateElapsedTime(client->startTime, client->endTime);

    char timeTakenString[50] = {0};
    snprintf(timeTakenString, sizeof(timeTakenString), "%.2f", timeTaken);

    if (send(activeSocket, timeTakenString, sizeof(timeTakenString), 0) < 0)
    {
        printf("Failed to send time taken to server. 1\n");
        gameDestroy(client, &socketsData, NULL);
        free(args[1]);
        free(args[3]);
        free(args);
        if (serverThread) free(serverThread);
        return 1;
    }

    char timeResultLength[11] = {0};
    bytes_received = recv(activeSocket, timeResultLength, 10, 0);
    if (bytes_received <= 0)
    {
        printf("Failed to receive results from server. 2\n");
        gameDestroy(client, &socketsData, NULL);
        free(args[1]);
        free(args[3]);
        free(args);
        if (serverThread) free(serverThread);
        return 1;
    }
    timeResultLength[bytes_received] = '\0';

    int jsonResultsLength = atoi(timeResultLength);
    if (jsonResultsLength <= 0)
    {
        printf("Failed to receive results length from server.\n");
        gameDestroy(client, &socketsData, NULL);
        free(args[1]);
        free(args[3]);
        free(args);
        if (serverThread) free(serverThread);
        return 1;
    }
    char *jsonResults = calloc(jsonResultsLength + 1, sizeof(char));
    if (!jsonResults)
    {
        gameDestroy(client, &socketsData, NULL);
        free(args[1]);
        free(args[3]);
        free(args);
        if (serverThread) free(serverThread);
        return 1;
    }
    bytes_received = recv(activeSocket, jsonResults, jsonResultsLength, 0);
    if (bytes_received <= 0)
    {
        printf("Failed to receive results from server. 3\n");
        free(jsonResults);
        gameDestroy(client, &socketsData, NULL);
        free(args[1]);
        free(args[3]);
        free(args);
        if (serverThread) free(serverThread);
        return 1;
    }
    jsonResults[bytes_received] = '\0';

    Times times = deserializeTimes(jsonResults);
    free(jsonResults);

    showResults(client, &times, socketsData.sentence);
    gameDestroy(client, &socketsData, &times);
    if (mode == 1 && serverThread)
    {
        pthread_join(*serverThread, NULL);
        free(serverThread);
    }
    free(args[1]);
    free(args[3]);
    free(args);
    return 0;
}
