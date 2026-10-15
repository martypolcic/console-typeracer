#include "client.h"

void gameLoop(Client *client)
{
    clearScreen();
    data *d = malloc(sizeof(data));
    if (d == NULL)
    {
        return;
    }
    d->progress = malloc(client->playerCount * sizeof(int));
    if (d->progress == NULL)
    {
        free(d);
        return;
    }
    for (int i = 0; i < client->playerCount; i++) d->progress[i] = 0;
    d->client = client;
    d->serverDisconnected = false;

    // Initialize the mutex
    pthread_t refreshScreenThread;
    pthread_mutex_t mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

    d->mutex = &mutex;

    pthread_create(&refreshScreenThread, NULL, refreshScreen, d);

    bool localFinished = false;

    while (true)
    {
        pthread_mutex_lock(&mutex);
        if (d->serverDisconnected || client->hasFinalTimes)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }

        if (localFinished)
        {
            pthread_mutex_unlock(&mutex);
            usleep(50000); // 50ms pause while waiting for others to finish
            continue;
        }

        showSentence(client);
        pthread_mutex_unlock(&mutex);

        // Wait for keyboard input with a 50ms timeout to avoid hanging on getchar()
        // when race completes or server disconnects
        fd_set inputFds;
        FD_ZERO(&inputFds);
        FD_SET(STDIN_FILENO, &inputFds);
        struct timeval inputTv = {0, 50000}; // 50ms
        int sel = select(STDIN_FILENO + 1, &inputFds, NULL, NULL, &inputTv);
        if (sel <= 0)
        {
            continue;
        }

        int playerInput = getchar();
        if (playerInput == EOF || feof(stdin))
        {
            break;
        }

        pthread_mutex_lock(&mutex);
        if (d->serverDisconnected || client->hasFinalTimes)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }

        if (playerInput == 27)
        {
            // Escape sequence (e.g. arrow keys) - consume trailing bytes
            struct timeval tv = {0, 5000};
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            while (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0)
            {
                getchar();
                FD_ZERO(&fds);
                FD_SET(STDIN_FILENO, &fds);
                tv.tv_sec = 0;
                tv.tv_usec = 5000;
            }
            pthread_mutex_unlock(&mutex);
            continue;
        }

        if (playerInput == 127 || playerInput == '\b')
        {
            if (strlen(client->playerWord) > 0)
            {
                client->charsDone--;
                client->playerWord = deleteChar(client->playerWord);
            }
            pthread_mutex_unlock(&mutex);
            continue;
        }
        if (!isprint((unsigned char)playerInput))
        {
            pthread_mutex_unlock(&mutex);
            continue;
        }

        if (client->words == NULL || client->currentWordIndex >= client->wordCount)
        {
            pthread_mutex_unlock(&mutex);
            continue;
        }

        if (playerInput == ' ' && strcmp(client->playerWord, client->words[client->currentWordIndex]) == 0)
        {
            client->currentWordIndex++;
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
            // playerWord is already the length to the end of the sentence, do not append
            if (strlen(client->playerWord) >= (size_t)getRemainingChars(client->words, client->currentWordIndex, client->wordCount))
            {
                pthread_mutex_unlock(&mutex);
                continue;
            }

            size_t prevWordLen = strlen(client->playerWord);
            client->playerWord = appendChar(client->playerWord, (char)playerInput);
            if (strlen(client->playerWord) > prevWordLen)
            {
                client->charsDone++;
            }
            if (client->currentWordIndex == client->wordCount - 1 && strcmp(client->playerWord, client->words[client->currentWordIndex]) == 0)
            {
                char sentenceFinish = '2';
                if (send(client->activeSocket, &sentenceFinish, 1, 0) < 0)
                {
                    printf("Failed to send finish event.\n");
                }

                localFinished = true;
                clearScreen();
                printf("\n\n\tWaiting for race to finish...\n\n");
                fflush(stdout);
            }
        }
        pthread_mutex_unlock(&mutex);
    }
    clearScreen();
    if (client->activeSocket >= 0)
    {
        shutdown(client->activeSocket, SHUT_RDWR);
    }
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
    printf("\033[s"); // Save typing cursor position
    setCursorVisibility(false);
    moveCursor(1, 1);

    // get terminal size
    struct winsize w;
    int terminalWidth = (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col <= 0) ? 80 : w.ws_col;

    for (int i = 0; i < playerCount; i++)
    {
        const char *name = (playerNames && playerNames[i]) ? playerNames[i] : "Player";
        printf("\033[K%s\n", name);
        // print progress bar with division by zero guard
        int progressWidth = (wordCount > 0) ? ((progress[i] * terminalWidth) / wordCount) : 0;
        if (progressWidth > terminalWidth)
        {
            progressWidth = terminalWidth;
        }
        printf("\033[K\033[42m");
        for (int j = 0; j < progressWidth; j++)
        {
            printf(" ");
        }
        printf("\033[0m\n");
    }
    printf("\033[u"); // Restore typing cursor position
    setCursorVisibility(true);
    fflush(stdout);
}

void showSentence(Client *client)
{
    int cursorRow = 0;
    int cursorCol = 0;
    struct winsize w;
    int terminalWidth = (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col <= 0) ? 80 : w.ws_col;
    moveCursor(1 + (client->playerCount * 2), 1);
    printf("Sentence: ");
    int currentPrintWordIndex = client->currentWordIndex;
    // print correct words in green
    for (int i = 0; i < currentPrintWordIndex && i < client->wordCount; i++)
    {
        printf("\033[32m%s\033[0m ", client->words[i]);
    }

    int coloredChars = strlen(client->playerWord);
    int typedChars = 0;
    if (currentPrintWordIndex < client->wordCount)
    {
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

        // print as many chars from original sentence red as there are coloredChars left
        coloredChars = coloredChars - typedChars;
        for (int i = 0; i < coloredChars; i++)
        {
            if (typedChars >= (int)strlen(client->words[currentPrintWordIndex]))
            {
                if (currentPrintWordIndex < client->wordCount - 1)
                {
                    typedChars = 0;
                    currentPrintWordIndex++;
                    // print space with red background
                    printf("\033[41m \033[0m");
                }
                else
                {
                    break;
                }
            }
            else
            {
                printf("\033[31m%c\033[0m", client->words[currentPrintWordIndex][typedChars]); // red
                typedChars++;
            }
        }

        // print the rest of the current word in white
        for (int i = typedChars; i < (int)strlen(client->words[currentPrintWordIndex]); i++)
        {
            printf("\033[37m%c\033[0m", client->words[currentPrintWordIndex][i]); // white
        }
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
    cursorCol = ((client->charsDone + 9) % terminalWidth) + 1;
    moveCursor(cursorRow, cursorCol);
    setCursorVisibility(true);
}

void showResults(Client *client, Times *timesTaken, char *sentence)
{
    clearScreen();
    printf("--------------------------------\n");
    if (sentence) printf("%s\n", sentence);
    printf("--------------------------------\n");

    if (timesTaken == NULL || timesTaken->times == NULL)
    {
        printf("Results unavailable from server.\n");
        return;
    }

    int *sortedIndexes = malloc(client->playerCount * sizeof(int));
    if (!sortedIndexes)
    {
        return;
    }
    for (int i = 0; i < client->playerCount; i++)
    {
        sortedIndexes[i] = i;
    }
    // Sort: positive finish times ascending; non-positive (DNF/disconnected) to the end
    for (int i = 0; i < client->playerCount; i++)
    {
        for (int j = i + 1; j < client->playerCount; j++)
        {
            double timeA = timesTaken->times[sortedIndexes[i]];
            double timeB = timesTaken->times[sortedIndexes[j]];
            bool swap = false;
            if (timeA <= 0 && timeB > 0)
            {
                swap = true;
            }
            else if (timeA > 0 && timeB > 0 && timeA > timeB)
            {
                swap = true;
            }
            if (swap)
            {
                int temp = sortedIndexes[i];
                sortedIndexes[i] = sortedIndexes[j];
                sortedIndexes[j] = temp;
            }
        }
    }

    printf("Leaderboard:\n");
    int place = 1;
    for (int i = 0; i < client->playerCount; i++)
    {
        int idx = sortedIndexes[i];
        const char *pName = (client->playerNames && client->playerNames[idx]) ? client->playerNames[idx] : "Player";
        if (timesTaken->times[idx] <= 0)
        {
            printf("DNF: %s (Disconnected)\n", pName);
        }
        else
        {
            switch (place)
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
                printf("%dth place: ", place);
                break;
            }
            printf("%s - %.2f seconds\n", pName, timesTaken->times[idx]);
            place++;
        }
    }
    printf("--------------------------------\n");

    printf("Your statistics:\n");
    double authoritativeTime = timesTaken->times[client->playerIndex];
    if (authoritativeTime > 0.0)
    {
        printf("Time: %.2f seconds\n", authoritativeTime);
        printf("Words per minute: %.2f\n", (client->wordCount / authoritativeTime) * 60.0);
    }
    else
    {
        printf("Status: Did not finish (Disconnected)\n");
    }
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
    char accum[4096] = {0};
    int accumLen = 0;

    while (true)
    {
        char temp[256];
        int bytes_received = recv(activeSocket, temp, sizeof(temp) - 1, 0);
        if (bytes_received <= 0)
        {
            pthread_mutex_lock(d->mutex);
            if (!client->hasFinalTimes)
            {
                clearScreen();
                printf("Server disconnected.\n");
            }
            if (client->activeSocket >= 0)
            {
                active_socket_destroy(client->activeSocket);
                client->activeSocket = -1;
            }
            d->serverDisconnected = true;
            pthread_mutex_unlock(d->mutex);
            break;
        }
        temp[bytes_received] = '\0';

        if (accumLen + bytes_received < (int)sizeof(accum) - 1)
        {
            memcpy(accum + accumLen, temp, bytes_received);
            accumLen += bytes_received;
            accum[accumLen] = '\0';
        }
        else
        {
            char *lastBrace = strrchr(accum, '{');
            if (lastBrace && lastBrace > accum)
            {
                int rem = strlen(lastBrace);
                memmove(accum, lastBrace, rem + 1);
                accumLen = rem;
            }
            else
            {
                accumLen = 0;
                accum[0] = '\0';
            }
            if (accumLen + bytes_received < (int)sizeof(accum) - 1)
            {
                memcpy(accum + accumLen, temp, bytes_received);
                accumLen += bytes_received;
                accum[accumLen] = '\0';
            }
        }

        while (true)
        {
            char *start = strchr(accum, '{');
            if (!start)
            {
                accumLen = 0;
                accum[0] = '\0';
                break;
            }

            int depth = 0;
            char *end = NULL;
            for (char *p = start; *p; p++)
            {
                if (*p == '{') depth++;
                else if (*p == '}')
                {
                    depth--;
                    if (depth == 0)
                    {
                        end = p;
                        break;
                    }
                }
            }

            if (!end)
            {
                if (start > accum)
                {
                    memmove(accum, start, strlen(start) + 1);
                    accumLen = strlen(accum);
                }
                break;
            }

            char saved = *(end + 1);
            *(end + 1) = '\0';

            // Check if this JSON object contains the final race results {"times": [...]}
            if (strstr(start, "\"times\"") != NULL)
            {
                Times times = deserializeTimes(start);
                *(end + 1) = saved;
                pthread_mutex_lock(d->mutex);
                client->finalTimes = times;
                client->hasFinalTimes = true;
                d->serverDisconnected = true;
                pthread_mutex_unlock(d->mutex);
                goto all_done;
            }

            RefreshingData refreshingData = deserializeRefreshingData(start);
            *(end + 1) = saved;

            char *rest = end + 1;
            int restLen = strlen(rest);
            memmove(accum, rest, restLen + 1);
            accumLen = restLen;

            if (refreshingData.progress == NULL)
            {
                continue;
            }

            pthread_mutex_lock(d->mutex);
            free(d->progress);
            d->progress = refreshingData.progress;
            showProgress(d->progress, client->playerCount, wordCount, client->playerNames);
            pthread_mutex_unlock(d->mutex);
        }
    }

all_done:
    pthread_mutex_lock(d->mutex);
    d->serverDisconnected = true;
    pthread_mutex_unlock(d->mutex);

    return NULL;
}


// Function to append a character to a string
char *appendChar(char *str, char c)
{
    if (str != NULL)
    {
        int len = strlen(str);
        if (len < 255)
        {
            str[len] = c;
            str[len + 1] = '\0';
        }
    }
    return str;
}

// Function to delete a character from a string
char *deleteChar(char *str)
{
    if (str != NULL)
    {
        int len = strlen(str);
        if (len > 0)
        {
            str[len - 1] = '\0';
        }
    }
    return str;
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
    printf("\033[?1049l");
    fflush(stdout);
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
                putchar('+');
                for (int j = 1; j < terminalWidth - 1; j++)
                {
                    putchar('-');
                }
                if (terminalWidth > 1)
                {
                    putchar('+');
                }
            }
            else if (i == terminalHeight - 2)
            {
                putchar('|');
                int cpStart = (int)floor((terminalWidth / 2.0) - (strlen(copyRight) / 2.0));
                for (int j = 1; j < terminalWidth - 1; j++)
                {
                    if (j == cpStart)
                    {
                        fputs(copyRight, stdout);
                        j += (int)strlen(copyRight) - 1;
                    }
                    else
                    {
                        putchar(' ');
                    }
                }
                if (terminalWidth > 1)
                {
                    putchar('|');
                }
            }
            else
            {
                putchar('|');
                int bannerStart = (int)floor(((terminalWidth - 47) / 2.0));
                int menuStart = (int)floor((terminalWidth / 2.0) - 10);
                for (int j = 1; j < terminalWidth - 1; j++)
                {
                    if (i >= (terminalHeight / 5) && j == bannerStart && menuTextCount < menuTextCountMax)
                    {
                        fputs(menuText[menuTextCount], stdout);
                        menuTextCount++;
                        j += 46;
                    }
                    else if (i >= (terminalHeight / 5) * 3 && j == menuStart && menuItemTextCount < menuItemsCount)
                    {
                        if (selectedMenuItem == menuItemTextCount)
                        {
                            printf("\e[1m>\e[0m %s ", menuItems[menuItemTextCount]);
                        }
                        else
                        {
                            printf("   %s", menuItems[menuItemTextCount]);
                        }
                        j += (int)strlen(menuItems[menuItemTextCount]) + 2;
                        menuItemTextCount++;
                    }
                    else
                    {
                        putchar(' ');
                    }
                }
                if (terminalWidth > 1)
                {
                    putchar('|');
                }
            }
            printf("\n");
        }

        printf("\n   [W/S or Up/Down] Move    [Enter] Select\n");

        int input = getchar();
        if (input == EOF || feof(stdin))
        {
            return 0;
        }
        if (input == 27)
        {
            // Check if trailing escape sequence bytes are available (e.g. arrow keys)
            struct timeval tv = {0, 50000};
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0)
            {
                int c1 = getchar();
                if (c1 == '[')
                {
                    tv.tv_sec = 0;
                    tv.tv_usec = 50000;
                    FD_ZERO(&fds);
                    FD_SET(STDIN_FILENO, &fds);
                    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0)
                    {
                        int c2 = getchar();
                        if (c2 == 'A') // Up
                        {
                            if (selectedMenuItem > 0)
                            {
                                selectedMenuItem--;
                            }
                        }
                        else if (c2 == 'B') // Down
                        {
                            if (selectedMenuItem < menuItemsCount - 1)
                            {
                                selectedMenuItem++;
                            }
                        }
                    }
                }
            }
        }
        else if (input == 's' || input == 'S')
        {
            if (selectedMenuItem < menuItemsCount - 1)
            {
                selectedMenuItem++;
            }
        }
        else if (input == 'w' || input == 'W')
        {
            if (selectedMenuItem > 0)
            {
                selectedMenuItem--;
            }
        }
        else if (input == '\n' || input == '\r')
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


