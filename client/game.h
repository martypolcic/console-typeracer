#ifndef GAME_H
#define GAME_H

#include "terminal.h"
#include "sourceValidator.h"
#include "inputHandler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

void startGame(const char* inputFilePath);
void results();
void gameDestroy();
double calculateElapsedTime(struct timeval start, struct timeval end);


#endif // GAME_H