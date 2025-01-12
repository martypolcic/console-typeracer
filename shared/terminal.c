#include "terminal.h"
// Terminal control functions

void disableBufferedInput() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

void restoreBufferedInput() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= (ICANON | ECHO); // Enable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

// Function to move cursor to a specific position
void moveCursor(int x, int y) {
    printf("\033[%d;%dH", x, y);
}

// Function to hide/show the cursor
void setCursorVisibility(bool visible) {
    printf("\033[?25%c", visible ? 'h' : 'l');
}

void clearScreen() {
    printf("\033[2J\033[H"); // Clear screen and move cursor to top-left corner
}

void clearSentence(int startRow) {
    moveCursor(1 + startRow, 0);
    printf("\033[K");
}