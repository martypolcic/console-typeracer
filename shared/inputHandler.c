#include "inputHandler.h"

int isCharacterKey(int key) {
    return isprint(key);
}

int compareInput(char correctChar, char playerInput) {
    return correctChar == playerInput;
}