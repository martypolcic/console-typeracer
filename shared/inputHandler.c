#include "inputHandler.h"

int isCharacterKey(int key) {
    return isalpha(key) || key == ',' || key == '.' || key == '?' || key == '!' || key == '\'' || key == ' ' || key == '-' || key == ':' || key == ';' || key == '"';
}

int compareInput(char correctChar, char playerInput) {
    return correctChar == playerInput;
}