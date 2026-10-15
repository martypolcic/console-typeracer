#include <unistd.h>
#include <termios.h>
#include <time.h>
#include "client.h"

// Signal handler for SIGINT and SIGTERM - async-signal-safe
void handle_sigint(int sig)
{
    (void)sig;
    const char msg[] = "\033[?1049l\033[?25h\nCaught signal. Exiting safely.\n";
    (void)write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    restoreBufferedInput();
    _exit(0);
}

void clear_buffer(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

int main(int argc, char **argv)
{
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    signal(SIGQUIT, handle_sigint);
    signal(SIGPIPE, SIG_IGN);
    srand(time(NULL));

    char host[128] = "localhost";
    int port = 54897;
    char lobbyCode[32] = "DEFAULT";
    char playerName[32] = {0};
    bool autoJoin = false;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--host") == 0 && i + 1 < argc)
        {
            strncpy(host, argv[++i], sizeof(host) - 1);
            autoJoin = true;
        }
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
        {
            port = atoi(argv[++i]);
            autoJoin = true;
        }
        else if ((strcmp(argv[i], "--lobby") == 0 || strcmp(argv[i], "--room") == 0) && i + 1 < argc)
        {
            strncpy(lobbyCode, argv[++i], sizeof(lobbyCode) - 1);
            autoJoin = true;
        }
        else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc)
        {
            strncpy(playerName, argv[++i], sizeof(playerName) - 1);
        }
        else if (argv[i][0] != '-')
        {
            strncpy(playerName, argv[i], sizeof(playerName) - 1);
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            printf("Usage: %s [name] [--name <player_name>] [--host <ip/domain>] [--port <port>] [--lobby <room_code>]\n", argv[0]);
            return 0;
        }
    }

    int mode = 2; // Default to joining lobby if autoJoin is true
    if (!autoJoin)
    {
        disableBufferedInput();
        setCursorVisibility(false);
        mode = menu();
        restoreBufferedInput();
        setCursorVisibility(true);
    }

    if (mode <= 0)
    {
        return (mode == 0) ? 0 : 1;
    }
    else if (mode == 1)
    {
        clearScreen();
        printf("How many players do you want to play with?: ");
        int playerCount = 2;
        if (scanf("%d", &playerCount) != 1)
        {
            playerCount = 2;
        }
        clear_buffer();
        if (playerCount < 1)
        {
            printf("Please provide a number of at least 1.\n");
            return 1;
        }

        // Generate a 6-character room code and register with coordinator
        char randRoom[16] = {0};
        const char codeChars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        for (int i = 0; i < 6; i++)
        {
            randRoom[i] = codeChars[rand() % (sizeof(codeChars) - 1)];
        }

        int createSock = connect_to_server(host, port);
        if (createSock >= 0)
        {
            char createCmd[64];
            snprintf(createCmd, sizeof(createCmd), "CREATE %s %d\n", randRoom, playerCount);
            send_all(createSock, createCmd, strlen(createCmd));
            char resp[128] = {0};
            recv(createSock, resp, sizeof(resp) - 1, 0);
            close(createSock);
            if (strstr(resp, "\"status\":\"ok\"") == NULL)
            {
                resp[strcspn(resp, "\r\n")] = '\0';
                printf("Failed to create room: %s\n", resp[0] ? resp : "No response from server");
                return 1;
            }
            char *maxPtr = strstr(resp, "\"maxPlayers\":");
            if (maxPtr)
            {
                int approved = atoi(maxPtr + 13);
                if (approved > 0)
                {
                    playerCount = approved;
                }
            }
            strncpy(lobbyCode, randRoom, sizeof(lobbyCode) - 1);
            printf("Created room [%s] for %d players on %s:%d\n", lobbyCode, playerCount, host, port);
            sleep(1);
        }
        else
        {
            printf("Failed to connect to TypeRacer coordinator at %s:%d.\n", host, port);
            printf("Please start the daemon first: ./server --daemon\n");
            return 1;
        }
    }
    else if (mode == 2 && !autoJoin)
    {
        clearScreen();
        printf("Enter server host [%s]: ", host);
        char hostBuf[128] = {0};
        if (fgets(hostBuf, sizeof(hostBuf), stdin))
        {
            hostBuf[strcspn(hostBuf, "\r\n")] = '\0';
            if (strlen(hostBuf) > 0)
            {
                strncpy(host, hostBuf, sizeof(host) - 1);
            }
        }

        printf("Enter server port [%d]: ", port);
        char portBuf[32] = {0};
        if (fgets(portBuf, sizeof(portBuf), stdin))
        {
            int p = atoi(portBuf);
            if (p > 0)
            {
                port = p;
            }
        }

        printf("Enter lobby code (or leave empty for default): ");
        char lobbyBuf[32] = {0};
        if (fgets(lobbyBuf, sizeof(lobbyBuf), stdin))
        {
            lobbyBuf[strcspn(lobbyBuf, "\r\n")] = '\0';
            if (strlen(lobbyBuf) > 0)
            {
                strncpy(lobbyCode, lobbyBuf, sizeof(lobbyCode) - 1);
            }
        }
    }

    char name[21] = {0};
    if (playerName[0] != '\0')
    {
        strncpy(name, playerName, sizeof(name) - 1);
    }
    else
    {
        printf("Enter your name: ");
        if (fgets(name, 20, stdin) == NULL || strlen(name) == 0 || name[0] == '\n')
        {
            printf("Please provide a valid name.\n");
            return 1;
        }
        if (strchr(name, '\n') == NULL)
        {
            clear_buffer();
        }
        name[strcspn(name, "\r\n")] = 0;
    }

    int activeSocket = connect_to_server(host, port);
    if (activeSocket < 0)
    {
        printf("Failed to connect to server (%s:%d).\n", host, port);
        return 1;
    }

    char handshake[128] = {0};
    snprintf(handshake, sizeof(handshake), "JOIN %s %s\n", lobbyCode, name);
    if (send_all(activeSocket, handshake, strlen(handshake)) < 0)
    {
        printf("Failed to send handshake to server.\n");
        active_socket_destroy(activeSocket);
        return 1;
    }

    clearScreen();
    printf("Connected to [%s:%d] | Lobby: [%s]\n", host, port, lobbyCode);
    printf("Waiting for other players to join...\n");
    printf("------------------------------------------------\n");

    char jsonSocketDataLength[11] = {0};
    if (recv_all(activeSocket, jsonSocketDataLength, 10) < 0)
    {
        printf("Json data length went wrong.\n");
        active_socket_destroy(activeSocket);
        return 1;
    }
    jsonSocketDataLength[10] = '\0';

    if (strncmp(jsonSocketDataLength, "ROOM_FULL", 9) == 0)
    {
        printf("Cannot join room: Room is full or race has already started.\n");
        active_socket_destroy(activeSocket);
        return 1;
    }
    if (strncmp(jsonSocketDataLength, "ERROR:", 6) == 0)
    {
        char errBuf[128] = {0};
        memcpy(errBuf, jsonSocketDataLength, 10);
        recv(activeSocket, errBuf + 10, sizeof(errBuf) - 11, MSG_DONTWAIT);
        errBuf[strcspn(errBuf, "\r\n")] = '\0';
        printf("%s\n", errBuf);
        active_socket_destroy(activeSocket);
        return 1;
    }
    if (strncmp(jsonSocketDataLength, "SERVER_ERR", 10) == 0)
    {
        printf("Server error occurred while joining room.\n");
        active_socket_destroy(activeSocket);
        return 1;
    }

    int socketDataLength = atoi(jsonSocketDataLength);
    if (socketDataLength <= 0 || socketDataLength > 65536)
    {
        printf("Getting JSON error: invalid length (%d)\n", socketDataLength);
        active_socket_destroy(activeSocket);
        return 1;
    }
    char *jsonSocketData = calloc(socketDataLength + 1, sizeof(char));
    if (!jsonSocketData)
    {
        active_socket_destroy(activeSocket);
        return 1;
    }
    if (recv_all(activeSocket, jsonSocketData, socketDataLength) < 0)
    {
        printf("Getting JSON error\n");
        free(jsonSocketData);
        active_socket_destroy(activeSocket);
        return 1;
    }
    jsonSocketData[socketDataLength] = '\0';

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
    client->playerWord = calloc(256, sizeof(char));
    client->words = parseSentence(socketsData.sentence, &client->wordCount);
    if (!client->words || client->wordCount <= 0)
    {
        printf("Error: Empty or invalid race sentence received from server.\n");
        if (client->words) free(client->words);
        free(client->playerWord);
        free(client);
        for (int i = 0; i < socketsData.playerCount; i++)
        {
            if (socketsData.names && socketsData.names[i]) free(socketsData.names[i]);
        }
        if (socketsData.names) free(socketsData.names);
        if (socketsData.sentence) free(socketsData.sentence);
        active_socket_destroy(activeSocket);
        return 1;
    }
    client->hasFinalTimes = false;
    client->finalTimes.times = NULL;

    // Switch to alternate screen buffer for clean terminal session
    printf("\033[?1049h");
    fflush(stdout);
    clearScreen();
    disableBufferedInput();
    setCursorVisibility(false);

    gameLoop(client);

    // Restore terminal settings and discard leftover keystrokes before waiting for Enter
    restoreBufferedInput();
    setCursorVisibility(true);
    tcflush(STDIN_FILENO, TCIFLUSH);

    if (client->hasFinalTimes)
    {
        showResults(client, &client->finalTimes, socketsData.sentence);
        printf("\nPress Enter to exit...\n");
        getchar();
        gameDestroy(client, &socketsData, &client->finalTimes);
    }
    else
    {
        printf("\nRace ended without results (server disconnected).\n");
        printf("\nPress Enter to exit...\n");
        getchar();
        gameDestroy(client, &socketsData, NULL);
    }
    return 0;
}
