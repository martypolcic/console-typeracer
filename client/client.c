#include "client.h"

void gameLoop(Client *client)
{
    clearScreen();
    data *d = malloc(sizeof(data));
    d->progress = malloc(client->playerCount * sizeof(int));
    d->client = client;
    d->serverDisconnected = false;

    // Initialize the mutex
    pthread_t refreshScreenThread;
    pthread_mutex_t mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t startCond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    pthread_cond_t finishCond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

    d->mutex = &mutex;
    d->wait = &startCond;
    d->refresh = &finishCond;

    pthread_create(&refreshScreenThread, NULL, refreshScreen, d);
    pthread_mutex_lock(&mutex);
    while (d->serverDisconnected == false)
    {
        showSentence(client);
        pthread_mutex_unlock(&mutex);

        char playerInput = getchar();

        if (playerInput == 127 || playerInput == '\b')
        {
            if (strlen(client->playerWord) <= 0)
            {
                pthread_mutex_lock(&mutex);
                continue;
            }
            client->charsDone--;
            client->playerWord = deleteChar(client->playerWord);
            pthread_mutex_lock(&mutex);
            continue;
        }
        if (!isCharacterKey(playerInput))
        {
            pthread_mutex_lock(&mutex);
            continue;
        }

        if (playerInput == ' ' && strcmp(client->playerWord, client->words[client->currentWordIndex]) == 0)
        {
            client->currentWordIndex++;
            free(client->playerWord);
            client->playerWord = malloc(1);
            client->playerWord[0] = '\0';
            client->charsDone++;

            char wordDone = '3';
            if (send(client->activeSocket, &wordDone, 1, 0) < 0)
            {
                printf("Failed to send word count.\n");
            }
        }
        else
        {
            // playerWord is already the lenght to the end of the sentence, do not append
            if (strlen(client->playerWord) >= getRemainingChars(client->words, client->currentWordIndex, client->wordCount))
            {
                pthread_mutex_lock(&mutex);
                continue;
            }

            client->charsDone++;
            client->playerWord = appendChar(client->playerWord, playerInput);
            if (client->currentWordIndex == client->wordCount - 1 && strcmp(client->playerWord, client->words[client->currentWordIndex]) == 0)
            {
                char sentenceFinish = '2';
                if (send(client->activeSocket, &sentenceFinish, 1, 0) < 0)
                {
                    printf("Failed to send word count.\n");
                }
                gettimeofday(&client->endTime, NULL);
                pthread_mutex_lock(&mutex);
                break;
            }
        }
        pthread_mutex_lock(&mutex);
    }
    pthread_mutex_unlock(&mutex);
    clearScreen();
    pthread_join(refreshScreenThread, NULL);
    if (d->progress != NULL)
    {
        free(d->progress);
        d->progress = NULL;
    }
    free(d);
    d = NULL;
}

void showProgress(int *progress, int playerCount, int wordCount, char **playerNames)
{
    // clearScreen();
    moveCursor(0, 0);
    setCursorVisibility(false);
    // get terminal size
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
    {
        perror("ioctl");
        free(progress);
        progress = NULL;
        return;
    }
    int terminalWidth = w.ws_col;
    for (int i = 0; i < playerCount; i++)
    {
        printf("%s\n", playerNames[i]);
        // print progress bar
        int progressWidth = (progress[i] * terminalWidth) / wordCount;
        printf("\033[42m");
        for (int j = 0; j < progressWidth; j++)
        {
            printf(" ");
        }
        printf("\033[0m");
        printf("\n");
    }
}

void showSentence(Client *client)
{
    int cursorRow = 0;
    int cursorCol = 0;
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
    {
        perror("ioctl");
        return;
    }
    int terminalWidth = w.ws_col;
    cursorRow = (((client->charsDone + 8) / (terminalWidth)) + (client->playerCount * 2)) + 1;
    cursorCol = ((client->charsDone + 8) % terminalWidth);
    moveCursor(1 + (client->playerCount * 2), 0);
    printf("Sentence: ");
    int currentPrintWordIndex = client->currentWordIndex;
    // print correct words in green
    for (int i = 0; i < currentPrintWordIndex; i++)
    {
        printf("\033[32m%s\033[0m ", client->words[i]);
    }

    int coloredChars = strlen(client->playerWord);
    int typedChars = 0;
    for (int i = 0; i < coloredChars; i++)
    {
        if (client->playerWord[i] == client->words[currentPrintWordIndex][i])
        {
            printf("\033[32m%c\033[0m", client->playerWord[i]); // green
            typedChars++;
        }
        else
        {
            break;
        }
    }

    // print as many chars from original sentence red as there are coloredChars left, if any overflows, print chars from another word red
    coloredChars = coloredChars - typedChars;
    for (int i = 0; i < coloredChars; i++)
    {
        if (typedChars >= strlen(client->words[currentPrintWordIndex]))
        {
            typedChars = 0;
            currentPrintWordIndex++;
            // print space with red background
            printf("\033[41m \033[0m");
        }
        else
        {
            printf("\033[31m%c\033[0m", client->words[currentPrintWordIndex][typedChars]); // red
            typedChars++;
        }
    }

    // print the rest of the words in white
    for (int i = typedChars; i < strlen(client->words[currentPrintWordIndex]); i++)
    {
        printf("\033[37m%c\033[0m", client->words[currentPrintWordIndex][i]); // white
    }

    // print the rest of the words in white
    for (int i = currentPrintWordIndex + 1; i < client->wordCount; i++)
    {
        printf(" %s", client->words[i]);
    }

    printf("\n");
    printf("\033[K");
    printf("Your input: %s\n", client->playerWord);
    printf("words: %d/%d\n", client->currentWordIndex, client->wordCount);
    cursorRow = (((client->charsDone + 9) / (terminalWidth)) + (client->playerCount * 2)) + 1;
    cursorCol = ((client->charsDone + 9) % terminalWidth);
    moveCursor(cursorRow, cursorCol);
    setCursorVisibility(true);
}

void showResults(Client *client, Times *timesTaken, char *sentence)
{
    clearScreen();
    printf("--------------------------------\n");
    printf("%s\n", sentence);
    printf("--------------------------------\n");

    int *sortedIndexes = malloc(client->playerCount * sizeof(int));
    for (int i = 0; i < client->playerCount; i++)
    {
        sortedIndexes[i] = i;
    }
    for (int i = 0; i < client->playerCount; i++)
    {
        for (int j = i + 1; j < client->playerCount; j++)
        {
            if (timesTaken->times[sortedIndexes[i]] > timesTaken->times[sortedIndexes[j]])
            {
                int temp = sortedIndexes[i];
                sortedIndexes[i] = sortedIndexes[j];
                sortedIndexes[j] = temp;
            }
        }
    }

    printf("Leaderboard:\n");
    for (int i = 0; i < client->playerCount; i++)
    {
        switch (i + 1)
        {
        case 1:
            printf("1st place: ");
            break;
        case 2:
            printf("2nd place: ");
            break;
        case 3:
            printf("3rd place: ");
            break;
        default:
            printf("%dth place: ", i + 1);
            break;
        }
        printf("%s - %.2f seconds\n", client->playerNames[sortedIndexes[i]], timesTaken->times[sortedIndexes[i]]);
    }
    printf("--------------------------------\n");

    printf("Your statistics:\n");
    printf("Time: %.2f seconds\n", timesTaken->times[client->playerIndex]);
    printf("Words per minute: %.2f\n", (client->wordCount / calculateElapsedTime(client->startTime, client->endTime)) * 60);
    printf("--------------------------------\n");

    free(sortedIndexes);
    sortedIndexes = NULL;
}

void *refreshScreen(void *arg)
{
    data *d = (data *)arg;
    Client *client = d->client;
    int wordCount = client->wordCount;
    int activeSocket = client->activeSocket;

    while (true)
    {
        int bufferSize = calculateRefreshingDataSize(client->playerCount, wordCount);
        char buffer[bufferSize];
        int bytes_received = recv(activeSocket, buffer, bufferSize, 0);
        if (bytes_received == 0)
        {
            clearScreen();
            printf("Server disconnected.\n");
            pthread_mutex_lock(d->mutex);
            active_socket_destroy(client->activeSocket);
            client->activeSocket = -1;
            free(d->progress);
            d->progress = NULL;
            pthread_mutex_unlock(d->mutex);
            break;
        }
        pthread_mutex_lock(d->mutex);
        free(d->progress);
        d->progress = NULL;
        pthread_mutex_unlock(d->mutex);

        RefreshingData refreshingData = deserializeRefreshingData(buffer);
        int count = 0;
        for (int i = 0; i < client->playerCount; i++)
        {
            if (refreshingData.progress[i] == wordCount)
            {
                count++;
            }
        }
        if (count == client->playerCount)
        {
            free(refreshingData.progress);
            refreshingData.progress = NULL;
            break;
        }
        pthread_mutex_lock(d->mutex);
        showProgress(refreshingData.progress, client->playerCount, wordCount, client->playerNames);
        pthread_mutex_unlock(d->mutex);
        if (refreshingData.progress == NULL)
        {
            break;
        }
        pthread_mutex_lock(d->mutex);
        d->progress = refreshingData.progress;
        pthread_mutex_unlock(d->mutex);
        // showSentence(d->client);
    }
    pthread_mutex_lock(d->mutex);
    d->serverDisconnected = true;
    pthread_mutex_unlock(d->mutex);

    return NULL;
}

// Function to append a character to a string
char *appendChar(char *str, char c)
{
    int len = strlen(str);
    char *newStr = malloc(len + 2);
    strcpy(newStr, str);
    newStr[len] = c;
    newStr[len + 1] = '\0';
    free(str);
    return newStr;
}

// Function to delete a character from a string
char *deleteChar(char *str)
{
    int len = strlen(str);
    char *newStr = malloc(len);
    strcpy(newStr, str);
    newStr[len - 1] = '\0';
    free(str);
    return newStr;
}

void gameDestroy(Client *client, SocketsData *socketsData, Times *times)
{
    if (socketsData != NULL)
    {
        for (int i = 0; i < socketsData->playerCount; i++)
        {
            if (socketsData->names[i] != NULL)
            {
                free(socketsData->names[i]);
                socketsData->names[i] = NULL;
            }
        }
        if (socketsData->names != NULL)
        {
            free(socketsData->names);
            socketsData->names = NULL;
            if (client != NULL)
            {
                client->playerNames = NULL;
            }
        }
        if (socketsData->sentence != NULL)
        {
            free(socketsData->sentence);
            socketsData->sentence = NULL;
        }
    }
    if (times != NULL)
    {
        if (times->times != NULL)
        {
            free(times->times);
            times->times = NULL;
        }
    }
    if (client != NULL)
    {
        if (client->playerNames != NULL)
        {
            for (int i = 0; i < client->playerCount; i++)
            {
                if (client->playerNames[i] != NULL)
                {
                    free(client->playerNames[i]);
                    client->playerNames[i] = NULL;
                }
            }
            free(client->playerNames);
            client->playerNames = NULL;
        }
        if (client->words != NULL)
        {
            for (int i = 0; i < client->wordCount; i++)
            {
                if (client->words[i] != NULL)
                {
                    free(client->words[i]);
                    client->words[i] = NULL;
                }
            }
            free(client->words);
            client->words = NULL;
        }
        if (client->playerWord != NULL)
        {
            free(client->playerWord);
            client->playerWord = NULL;
        }
        if (client->activeSocket != -1)
        {
            active_socket_destroy(client->activeSocket);
            client->activeSocket = -1;
        }
        free(client);
        client = NULL;
    }
    restoreBufferedInput();
    setCursorVisibility(true);
}

int getRemainingChars(char **words, int currentWordIndex, int wordCount)
{
    int remainingChars = 0;
    for (int i = currentWordIndex; i < wordCount; i++)
    {
        remainingChars += strlen(words[i]);
        if (i != wordCount - 1)
        {
            remainingChars++;
        }
    }
    return remainingChars;
}

int menu()
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
    {
        perror("ioctl");
        return -1;
    }
    int terminalWidth = w.ws_col;
    int terminalHeight = w.ws_row - 1;
    int cursorRow = terminalHeight / 2;
    int cursorCol = terminalWidth / 2;

    char *menuItems[] = {"Create lobby", "Join lobby", "Exit"};
    int menuItemsCount = sizeof(menuItems) / sizeof(menuItems[0]);

    char *copyRight = "© 2024 Samuel Fabry & Martin Polcic";

    char *menuText[6] = {
        " _____                                         ",
        "|_   _|   _ _ __   ___ _ __ __ _  ___ ___ _ __ ",
        "  | || | | | '_ \\ / _ \\ '__/ _` |/ __/ _ \\ '__|",
        "  | || |_| | |_) |  __/ | | (_| | (_|  __/ |   ",
        "  |_| \\__, | .__/ \\___|_|  \\__,_|\\___\\___|_|   ",
        "      |___/|_|                                 "};
    int menuTextCountMax = sizeof(menuText) / sizeof(menuText[0]);
    int selectedMenuItem = 0;
    while (true)
    {
        clearScreen();
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
        {
            perror("ioctl");
            return -1;
        }
        terminalWidth = w.ws_col;
        terminalHeight = w.ws_row - 1;
        int menuTextCount = 0;
        int menuItemTextCount = 0;
        for (int i = 0; i < terminalHeight; i++)
        {
            if (i == 0 || i == terminalHeight - 1)
            {
                for (int j = 0; j < terminalWidth; j++)
                {
                    if (j == 0 || j == terminalWidth - 1)
                    {
                        printf("+");
                    }
                    else
                    {
                        printf("-");
                    }
                }
            }
            else if (i == terminalHeight - 2)
            {
                for (int j = 0; j < terminalWidth; j++)
                {
                    if (j == 0 || j == terminalWidth - 1)
                    {
                        printf("|");
                    }
                    else
                    {
                        if (j == floor((terminalWidth / 2) - (strlen(copyRight) / 2)))
                        {
                            printf("%s ", copyRight);
                            j += strlen(copyRight) - 1;
                        }
                        else
                        {
                            printf(" ");
                        }
                    }
                }
            }
            else
            {
                for (int j = 0; j < terminalWidth; j++)
                {
                    if (j == 0 || j == terminalWidth - 1)
                    {
                        printf("|");
                    }
                    else
                    {
                        if ((i >= (terminalHeight / 5) && j == floor(((terminalWidth - 47) / 2))) && menuTextCount < menuTextCountMax)
                        {
                            printf("%s", menuText[menuTextCount]);
                            menuTextCount++;
                            j += 46;
                            continue;
                        }
                        else if (i >= (terminalHeight / 5) * 3 && j == floor((terminalWidth / 2) - 10) && menuItemTextCount < menuItemsCount)
                        {
                            if (selectedMenuItem == menuItemTextCount)
                            {
                                printf("\e[1m>\e[0m %s ", menuItems[menuItemTextCount]);
                            }
                            else
                            {
                                printf("   %s", menuItems[menuItemTextCount]);
                            }
                            j += strlen(menuItems[menuItemTextCount]) + 2;
                            menuItemTextCount++;
                        }
                        else
                        {
                            printf(" ");
                        }
                    }
                }
            }
            printf("\n");
        }

        char input = getchar();
        if (input == 's')
        {
            if (selectedMenuItem < menuItemsCount - 1)
            {
                selectedMenuItem++;
            }
        }
        else if (input == 'w')
        {
            if (selectedMenuItem > 0)
            {
                selectedMenuItem--;
            }
        }
        else if (input == '\n')
        {
            if (selectedMenuItem == 0)
            {
                return 1;
            }
            else if (selectedMenuItem == 1)
            {
                return 2;
            }
            else if (selectedMenuItem == 2)
            {
                return 0;
            }
        }
    }
}

// Function to calculate elapsed time in seconds
double calculateElapsedTime(struct timeval start, struct timeval end)
{
    return (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
}
