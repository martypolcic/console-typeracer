#ifndef SOURCEVALIDATOR_H
#define SOURCEVALIDATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char **readFileLines(const char *fileName, int *lineCount);
int validateLine(const char *line);
char **parseSentence(const char *sentence, int *wordCount);

#endif // SOURCEVALIDATOR_H