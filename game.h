#ifndef GAME_H
#define GAME_H

#include "game.h"
#include "terminal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/time.h>
#include <unistd.h>

void startGame();
void results();
void gameDestroy();
double calculateElapsedTime(struct timeval start, struct timeval end);


#endif // GAME_H