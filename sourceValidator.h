
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

// Function to validate a line
bool isValidLine(const char *line, int *length) {
    *length = 0;

    for (int i = 0; line[i] != '\0'; i++) {
        // Check if the character is printable or a newline
        if (!isprint(line[i]) && line[i] != '\n') {
            return false;
        }
        (*length)++;
    }

    // Check if the line exceeds the maximum allowed length
    return *length <= MAX_LINE_LENGTH;
}

int main() {
    const char *filename = "file.txt";  // Replace with your file's name
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    char line[MAX_LINE_LENGTH + 2];  // Buffer to hold lines (+1 for newline, +1 for null terminator)
    int lineNumber = 0;
    bool isValid = true;

    while (fgets(line, sizeof(line), file)) {
        lineNumber++;
        int length = 0;

        if (!isValidLine(line, &length)) {
            printf("Invalid line %d: Exceeds length or contains non-printable characters\n", lineNumber);
            isValid = false;
        } else {
            printf("Line %d is valid (Length: %d): %s", lineNumber, length, line);
        }
    }

    fclose(file);

    if (isValid) {
        printf("The file is valid!\n");
    } else {
        printf("The file contains invalid lines!\n");
    }

    return EXIT_SUCCESS;
}
