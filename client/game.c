#include "game.h"

// Typing game function
void startGame() {
    // struct winsize ws;
    // if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
    //     perror("ioctl");
    //     exit(EXIT_FAILURE);
    // }
    // int tWidth = ws.ws_col;
    // printf("Terminal size: %d rows, %d columns\n", ws.ws_row, ws.ws_col);

    // Read sentences from file
    char* sentencesFile = "../input.txt";
    int lineCount;
    char** sentences = readFileLines(sentencesFile, &lineCount);
    if (sentences == NULL) {
        printf("Failed to read file or file is empty.\n");
        return;
    }

    // Choose a random sentence
    srand(time(NULL));
    int randomIndex = rand() % lineCount;
    char *randomSentence = sentences[randomIndex];

    char *sentence = strdup(randomSentence);
    int sentenceLength = strlen(sentence);
    int wordCount = 0;
    int playerWordCount = 0;
    bool wrong = false;

    // Count words in the sentence
    for (char *tmp = sentence; *tmp; tmp++) {
        if (*tmp == ' ') wordCount++;
    }
    wordCount++; // For the last word

    char **words = malloc(wordCount * sizeof(char *));
    char **input = malloc(wordCount * sizeof(char *));
    char *word = strtok(sentence, " ");
    for (int i = 0; word != NULL; i++) {
        words[i] = strdup(word);
        input[i] = calloc(strlen(word) + 1, sizeof(char));
        word = strtok(NULL, " ");
    }

    struct timeval startTime, endTime;

    clearScreen();
    printf("Type the following sentence as fast as you can:\n");
    printf("------------------------------------------------\n%s\n------------------------------------------------\n\n", randomSentence);
    printf("Press Enter to start...");
    getchar();

    gettimeofday(&startTime, NULL);
    disableBufferedInput();
    setCursorVisibility(false);

    int running = 1;
    int currentIndex = 0;
    int inputIndex = 0;
    int charsDone = 0;
    int wrongIndex = 0;

    clearScreen();
    while (running) {
        printf("Sentence: ");
        for (int i = 0; i < wordCount; i++) {
            if (i < currentIndex) {
                printf("\033[32m%s\033[0m ", words[i]);
            } else if (i == currentIndex) {
                for (int j = 0; j < strlen(words[i]); j++) {
                    if (input[i][j] == '\0') {
                        printf("%c", words[i][j]);
                    } else if (!wrong || j < wrongIndex) {
                        printf("\033[32m%c\033[0m", words[i][j]);
                    } else {
                        printf("\033[31m%c\033[0m", words[i][j]);
                    }
                }
                printf(" ");
            } else {
                printf("%s ", words[i]);
            }
        }
        printf("\n");
        moveCursor(1, charsDone + currentIndex + 11);
        setCursorVisibility(true);

        if (currentIndex < wordCount) {
            char c = getchar();
            if (c != '\n') {
                if (c == ' ' && !wrong) {
                    currentIndex++;
                    inputIndex = 0;
                } else if (c == 127 || c == '\b') {
                    if (inputIndex > 0) {
                        input[currentIndex][--inputIndex] = '\0';
                        if (inputIndex == wrongIndex) {
                            wrongIndex = 0;
                            wrong = false;
                        }
                        charsDone--;
                    }
                } else if (isalpha(c)) {
                    if (inputIndex < strlen(words[currentIndex])) {
                        input[currentIndex][inputIndex] = c;
                        if (input[currentIndex][inputIndex] != words[currentIndex][inputIndex] && !wrong) {
                            wrongIndex = inputIndex;
                            wrong = true;
                        }
                        inputIndex++;
                        charsDone++;
                        if (!wrong && currentIndex == wordCount - 1 && inputIndex == strlen(words[currentIndex])) {
                            currentIndex++;
                        }
                    }
                }
            }
            clearScreen();
        } else {
            running = 0;
        }
    }

    gettimeofday(&endTime, NULL);
    restoreBufferedInput();
    setCursorVisibility(true);

    double timeTaken = calculateElapsedTime(startTime, endTime);
    results(&randomSentence, &timeTaken);
    gameDestroy(&wordCount, &words, &input, &sentence);
}

void results(char** s, double* timeTaken) {
    clearScreen();
    printf("Results:\n");
    printf("Time Taken: %.2f seconds\n", *timeTaken);
    printf("Words per minute: %.2f\n", (strlen(*s) / 5.0) / (*timeTaken / 60.0));
}

void gameDestroy(int* wordCount, char*** words, char*** input, char** sentence) {
    for (int i = 0; i < *wordCount; i++) {
        free(*words[i]);
        free(*input[i]);
    }
    free(*words);
    free(*input);
    free(*sentence);
}

// Function to calculate elapsed time in seconds
double calculateElapsedTime(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
}
