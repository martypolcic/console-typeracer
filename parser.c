#include "parser.h"
#include <string.h>

int parseSentence(const char *sentence, char words[MAX_WORDS][MAX_WORD_LENGTH]) {
    int wordCount = 0;
    char *token;
    char sentenceCopy[MAX_WORDS * MAX_WORD_LENGTH];

    strncpy(sentenceCopy, sentence, sizeof(sentenceCopy));
    sentenceCopy[sizeof(sentenceCopy) - 1] = '\0';

    token = strtok(sentenceCopy, " ");
    while (token != NULL && wordCount < MAX_WORDS) {
        strncpy(words[wordCount], token, MAX_WORD_LENGTH);
        words[wordCount][MAX_WORD_LENGTH - 1] = '\0';
        wordCount++;
        token = strtok(NULL, " ");
    }

    return wordCount;
}