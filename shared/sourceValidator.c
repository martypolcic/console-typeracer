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
    *wordCount = 0;
    if (sentence == NULL)
    {
        return NULL;
    }
    char *sentenceCopy = strdup(sentence);
    if (!sentenceCopy)
    {
        return NULL;
    }
    char *word = strtok(sentenceCopy, " \r\n\t");
    char **words = NULL;

    while (word != NULL)
    {
        char **temp = realloc(words, sizeof(char *) * (*wordCount + 1));
        if (!temp)
        {
            for (int i = 0; i < *wordCount; i++)
            {
                free(words[i]);
            }
            free(words);
            free(sentenceCopy);
            *wordCount = 0;
            return NULL;
        }
        words = temp;
        words[*wordCount] = strdup(word);
        if (!words[*wordCount])
        {
            for (int i = 0; i < *wordCount; i++)
            {
                free(words[i]);
            }
            free(words);
            free(sentenceCopy);
            *wordCount = 0;
            return NULL;
        }
        (*wordCount)++;
        word = strtok(NULL, " \r\n\t");
    }

    free(sentenceCopy);
    return words;
}
