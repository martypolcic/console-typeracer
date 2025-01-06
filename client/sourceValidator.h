#ifndef SOURCEVALIDATOR_H
#define SOURCEVALIDATOR_H

char** readFileLines(const char* fileName, int* lineCount);
int validateLine(const char* line);

#endif // SOURCEVALIDATOR_H