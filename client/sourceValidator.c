#include "sourceValidator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char** readFileLines(const char* fileName, int* lineCount) {
    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        return NULL;
    }

    char **lines = NULL;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    *lineCount = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        if (validateLine(line)) {
            lines = realloc(lines, sizeof(char*) * (*lineCount + 1));
            lines[*lineCount] = strdup(line);
            (*lineCount)++;
        }
    }

    free(line);
    fclose(file);

    return lines;
}

int validateLine(const char* line) {
    for (int i = 0; line[i] != '\0'; i++) {
        if (isalpha(line[i])) {
            return 1;
        }
    }
    return 0;
}