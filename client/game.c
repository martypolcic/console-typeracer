#include "game.h"

// Function to append a character to a string
char* appendChar(char* str, char c) {
    int len = strlen(str);
    char *newStr = malloc(len + 2);
    strcpy(newStr, str);
    newStr[len] = c;
    newStr[len + 1] = '\0';
    free(str);
    return newStr;
}

//Function to delete a character from a string
char* deleteChar(char* str) {
    int len = strlen(str);
    char *newStr = malloc(len);
    strcpy(newStr, str);
    newStr[len - 1] = '\0';
    free(str);
    return newStr;
}

void startGame() {
    // Read sentences from file
    char* sentencesFile = "../input.txt";
    int sentenceCount;
    char** sentences = readFileLines(sentencesFile, &sentenceCount);
    if (sentences == NULL) {
        printf("Failed to read file or file is empty.\n");
        return;
    }

    // Choose a random sentence
    srand(time(NULL));
    int randomIndex = rand() % sentenceCount;
    char *randomSentence = sentences[randomIndex];
    int sentenceLength = strlen(randomSentence);
    // free sentences
    for (int i = 0; i < sentenceCount; i++) {
        free(sentences[i]);
    }
    free(sentences);
    // parse sentence into words
    char** words = NULL;
    int wordCount;
    words = parseSentence(randomSentence, &wordCount);

    struct timeval startTime, endTime;
    // variables
    char* playerWord = malloc(1);
    playerWord[0] = '\0';
    int charsDone = 0;
    int currentWordIndex = 0;

    clearScreen();
    printf("Type the following sentence as fast as you can:\n");
    printf("------------------------------------------------\n%s\n------------------------------------------------\n\n", randomSentence);
    printf("Press Enter to start...");
    getchar();

    gettimeofday(&startTime, NULL);
    disableBufferedInput();
    setCursorVisibility(false);

    while (true) {
        clearScreen();
        printf("Sentence: ");

        int currentPrintWordIndex = currentWordIndex;
        // print correct words in green
        for (int i = 0; i < currentPrintWordIndex; i++) {
            printf("\033[32m%s\033[0m ", words[i]);
        }

        int coloredChars = strlen(playerWord);
        int typedChars = 0;
        for (int i = 0; i < coloredChars; i++) {
            if (playerWord[i] == words[currentPrintWordIndex][i]) {
                printf("\033[32m%c\033[0m", playerWord[i]); // green
                typedChars++;
            } else {
                break;
            }
        }

        // print as many chars from original sentence red as there are coloredChars left, if any overflows, print chars from another word red
        coloredChars = coloredChars - typedChars;
        for (int i = 0; i < coloredChars; i++) {
            if (typedChars >= strlen(words[currentPrintWordIndex])) {
                typedChars = 0;
                currentPrintWordIndex++;
                //print space with red background
                printf("\033[41m \033[0m");
            } else {
                printf("\033[31m%c\033[0m", words[currentPrintWordIndex][typedChars]); // red
                typedChars++;
            }
        }

        // print the rest of the words in white
        for (int i = typedChars; i < strlen(words[currentPrintWordIndex]); i++) {
            printf("\033[37m%c\033[0m", words[currentPrintWordIndex][i]); // white
        }

        // print the rest of the words in white
        for (int i = currentPrintWordIndex + 1; i < wordCount; i++) {
            printf(" %s", words[i]);
        }

        printf("\n");
        printf("Your input: %s\n", playerWord);
        printf("words: %d/%d\n", currentWordIndex, wordCount);
        moveCursor(1, charsDone + currentWordIndex + 11 - 1); // 11 is the length of "Sentence: " string 
        setCursorVisibility(true);

        if (currentWordIndex < wordCount) {
            char playerInput = getchar();
            
            if (playerInput == 127 || playerInput == '\b') {
                if (strlen(playerWord) <= 0) {
                    continue;
                }
                
                playerWord = deleteChar(playerWord);
                charsDone--;
                continue;
            } 
            if (!isCharacterKey(playerInput)) {
                continue;
            }

            
            if (playerInput == ' ' && strcmp(playerWord, words[currentWordIndex]) == 0) {
                currentWordIndex++;
                free(playerWord);
                playerWord = malloc(1);
                playerWord[0] = '\0';
            } else {
                // append character to playerWord
                if (currentPrintWordIndex == wordCount -1 && typedChars == strlen(words[currentPrintWordIndex])) {
                    continue;
                }
                playerWord = appendChar(playerWord, playerInput);
                charsDone++;
                if (currentWordIndex == wordCount -1 && strcmp(playerWord, words[currentWordIndex]) == 0) {
                    break;
                }
            }
        } else {
            break;
        }
    }

    gettimeofday(&endTime, NULL);
    restoreBufferedInput();
    setCursorVisibility(true);

    double timeTaken = calculateElapsedTime(startTime, endTime);
    results(&randomSentence, &timeTaken);
    gameDestroy(&wordCount, &words, &playerWord);
}

void results(char** s, double* timeTaken) {
    clearScreen();
    printf("Results:\n");
    printf("Time Taken: %.2f seconds\n", *timeTaken);
    printf("Words per minute: %.2f\n", (strlen(*s) / 5.0) / (*timeTaken / 60.0));
}

void gameDestroy(int* wordCount, char*** words, char** playerWord) {
    for (int i = 0; i < *wordCount; i++) {
        free((*words)[i]);
    }
    free(*words);
    *words = NULL;
    *wordCount = 0;
    free(*playerWord);
}

// Function to calculate elapsed time in seconds
double calculateElapsedTime(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
}
