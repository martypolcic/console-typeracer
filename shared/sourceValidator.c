#include "sourceValidator.h"

char **readFileLines(const char *fileName, int *lineCount)
{
    FILE *file = fopen(fileName, "r");
    if (file == NULL)
    {
        return NULL;
    }

    char **lines = NULL;
    size_t len = 0;
    ssize_t read;
    char *line = NULL;
    *lineCount = 0;

    while ((read = getline(&line, &len, file)) != -1)
    {
        if (validateLine(line))
        {
            // Allocate memory for the new line
            char **temp = realloc(lines, sizeof(char *) * (*lineCount + 1));
            if (temp == NULL)
            {
                perror("Memory allocation failed");
                // Free already allocated memory
                for (int i = 0; i < *lineCount; i++)
                {
                    free(lines[i]);
                }
                free(lines);
                free(line);
                fclose(file);
                return NULL;
            }
            lines = temp;

            // Duplicate and store the line
            lines[*lineCount] = strdup(line);
            if (lines[*lineCount] == NULL)
            {
                perror("Memory allocation failed");
                // Free already allocated memory
                for (int i = 0; i < *lineCount; i++)
                {
                    free(lines[i]);
                }
                free(lines);
                free(line);
                fclose(file);
                return NULL;
            }

            (*lineCount)++;
        }
    }

    free(line);   // Free buffer used by getline
    fclose(file); // Close the file

    return lines;
}

int validateLine(const char *line)
{
    for (int i = 0; line[i] != '\0'; i++)
    {
        if (isalpha(line[i]))
        {
            return 1;
        }
    }
    return 0;
}

char **parseSentence(const char *sentence, int *wordCount)
{
    char *sentenceCopy = strdup(sentence);
    char *word = strtok(sentenceCopy, " ");
    char **words = NULL;
    *wordCount = 0;

    while (word != NULL)
    {
        words = realloc(words, sizeof(char *) * (*wordCount + 1));
        words[*wordCount] = strdup(word);
        (*wordCount)++;
        word = strtok(NULL, " ");
    }

    free(sentenceCopy);

    return words;
}
