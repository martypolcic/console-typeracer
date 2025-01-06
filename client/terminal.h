#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>

void disableBufferedInput();
void restoreBufferedInput();
void moveCursor(int x, int y);
void setCursorVisibility(bool visible);
void clearScreen();

#endif