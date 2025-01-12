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
    char *filePath = "test.txt";
    signal(SIGINT, handle_sigint);
    disableBufferedInput();
    setCursorVisibility(false);
    int port = 54897;
    // int port = 33827;
    int mode = menu();
    restoreBufferedInput();
    setCursorVisibility(true);
    pthread_t *serverThread;
    char **args = malloc(4 * sizeof(char *));
    args[0] = NULL;
    args[1] = malloc(2 * sizeof(char));
    args[2] = filePath;
    args[3] = malloc(2 * sizeof(char));
    if (mode == 0)
    {
        return 0;
    }
    else if (mode == 1)
    {
        clearScreen();
        printf("How many players do you want to play with?: ");
        int playerCount;
        scanf("%d", &playerCount);
        clear_buffer();
        if (playerCount < 2)
        {
            printf("Please provide a number greater than 1.\n");
            return 1;
        }
        int logging = 0;
        snprintf(args[1], 2, "%d", playerCount);
        snprintf(args[3], 2, "%d", logging);
        serverThread = (pthread_t *)malloc(sizeof(pthread_t));
        pthread_create(serverThread, NULL, startupServer, args);
        sleep(1);
    }
    int activeSocket = connect_to_server("localhost", port);
    if (activeSocket < 0)
    {
        printf("Failed to connect to server.\n");
        return 1;
    }
    clearScreen();
    char name[20 * sizeof(char)];
    printf("Enter your name: ");
    fgets(name, 20, stdin);
    if (strlen(name) == 0)
    {
        printf("Please provide your name as an argument.\n");
        active_socket_destroy(activeSocket);
        return 1;
    }

    if (send(activeSocket, name, 19, 0) < 0)
    {
        printf("Failed to send name to server.\n");
        active_socket_destroy(activeSocket);
        return 1;
    }

    clearScreen();
    disableBufferedInput();
    setCursorVisibility(false);

    printf("Waiting for other players to join\n");
    printf("------------------------------------------------\n");

    char jsonSocketDataLength[10];
    int bytes_received = recv(activeSocket, jsonSocketDataLength, 10, 0);
    if (bytes_received == 0)
    {
        printf("Json data length went wrong.\n");
        active_socket_destroy(activeSocket);
        return 1;
    }

    int socketDataLength = atoi(jsonSocketDataLength);
    char jsonSocketData[socketDataLength];
    bytes_received = 0;
    bytes_received = recv(activeSocket, jsonSocketData, socketDataLength, 0);
    if (bytes_received == 0)
    {
        printf("Getting JSON error\n");
        active_socket_destroy(activeSocket);
        return 1;
    }

    SocketsData socketsData;
    socketsData = deserializeSocketsData(jsonSocketData);

    Client *client = malloc(sizeof(Client));
    client->activeSocket = activeSocket;
    client->playerIndex = socketsData.playerIndex;
    client->currentWordIndex = 0;
    client->playerNames = socketsData.names;
    client->charsDone = 2;
    client->playerCount = socketsData.playerCount;
    client->playerWord = malloc(1);
    client->playerWord[0] = '\0';
    client->words = parseSentence(socketsData.sentence, &client->wordCount);

    gettimeofday(&client->startTime, NULL);
    gameLoop(client);
    double timeTaken = calculateElapsedTime(client->startTime, client->endTime);

    char timeTakenString[15];
    snprintf(timeTakenString, 15, "%.2f", timeTaken);

    if (send(activeSocket, timeTakenString, 50, 0) < 0)
    {
        printf("Failed to send time taken to server. 1\n");
        gameDestroy(client, &socketsData, NULL);
        return 1;
    }

    char timeResultLength[10];
    bytes_received = 0;
    bytes_received = recv(activeSocket, timeResultLength, 10, 0);
    if (bytes_received == 0)
    {
        printf("Failed to receive results from server. 2\n");
        gameDestroy(client, &socketsData, NULL);
        return 1;
    }

    int jsonResultsLength = atoi(timeResultLength);
    char jsonResults[jsonResultsLength];
    bytes_received = 0;
    bytes_received = recv(activeSocket, jsonResults, jsonResultsLength, 0);
    if (bytes_received == 0)
    {
        printf("Failed to receive results from server. 3\n");
        gameDestroy(client, &socketsData, NULL);
        return 1;
    }

    Times times = deserializeTimes(jsonResults);

    showResults(client, &times, socketsData.sentence);
    gameDestroy(client, &socketsData, &times);
    if (mode == 1)
    {
        pthread_join(*serverThread, NULL);
        free(serverThread);
    }
    free(args[1]);
    free(args[3]);
    free(args);
    return 0;
}
