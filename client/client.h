#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <math.h>
#include <signal.h>

#include "terminal.h"
#include "sourceValidator.h"
#include "inputHandler.h"
#include "typeracerStructures.h"
#include "../sockets-lib/socket.h"
#include "jsonHandler.h"
#include "server.h"

void gameLoop(Client *client);
int menu();
void *refreshScreen(void *arg);                                                       // done
char *appendChar(char *str, char c);                                                  // done
char *deleteChar(char *str);                                                          // done
void gameDestroy(Client *client, SocketsData *socketsData, Times *times);             // done
void showProgress(int *progress, int playerCount, int wordCount, char **playerNames); // done
void showSentence(Client *client);                                                    // done
void showResults(Client *client, Times *timesTaken, char *sentence);                  // done
int getRemainingChars(char **words, int currentWordIndex, int wordCount);             // done
// Function to calculate elapsed time in seconds
double calculateElapsedTime(struct timeval start, struct timeval end);