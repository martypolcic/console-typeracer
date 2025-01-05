#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdbool.h>
#include <ctype.h>


#include "terminal.h"
#include "game.h"


#define MAX_SENTENCE_LENGTH 256

void renderTrack(int* playerWordCount, int* sentenceWordCount, int* tWidth) {
    printf("Player 1: ");
    int max = ((*tWidth - 20) * *playerWordCount) / *sentenceWordCount;
    for (int i = 0; i < max; i++) printf("-");
    printf(">\n");
}


int main() {
    printf("Welcome to the TypeRacer game!\n");
    startGame();
    return 0;
}
