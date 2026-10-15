#include "sourceValidator.h"

char **readFileLines(const char *fileName, int *lineCount)
{
    if (lineCount != NULL)
    {
        *lineCount = 0;
    }
    if (fileName == NULL || lineCount == NULL)
    {
        return NULL;
    }

    FILE *file = fopen(fileName, "r");
    if (file == NULL)
    {
        return NULL;
    }

    char **lines = NULL;
    int capacity = 0;
    size_t len = 0;
    ssize_t read;
    char *line = NULL;

    while ((read = getline(&line, &len, file)) != -1)
    {
        if (validateLine(line))
        {
            if (*lineCount >= capacity)
            {
                int newCapacity = capacity == 0 ? 16 : capacity * 2;
                char **temp = realloc(lines, sizeof(char *) * newCapacity);
                if (temp == NULL)
                {
                    perror("Memory allocation failed");
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
                capacity = newCapacity;
            }

            // Duplicate and store the line
            lines[*lineCount] = strdup(line);
            if (lines[*lineCount] == NULL)
            {
                perror("Memory allocation failed");
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
    if (line == NULL)
    {
        return 0;
    }
    for (int i = 0; line[i] != '\0'; i++)
    {
        if (isalpha((unsigned char)line[i]))
        {
            return 1;
        }
    }
    return 0;
}

char **parseSentence(const char *sentence, int *wordCount)
{
    if (wordCount != NULL)
    {
        *wordCount = 0;
    }
    if (sentence == NULL || wordCount == NULL)
    {
        return NULL;
    }
    char *sentenceCopy = strdup(sentence);
    if (!sentenceCopy)
    {
        return NULL;
    }
    char *saveptr = NULL;
    char *word = strtok_r(sentenceCopy, " \r\n\t", &saveptr);
    char **words = NULL;
    int capacity = 0;

    while (word != NULL)
    {
        if (*wordCount >= capacity)
        {
            int newCap = (capacity == 0) ? 16 : capacity * 2;
            char **temp = realloc(words, sizeof(char *) * newCap);
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
            capacity = newCap;
        }
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
        word = strtok_r(NULL, " \r\n\t", &saveptr);
    }

    free(sentenceCopy);
    return words;
}

int countWords(const char *sentence)
{
    if (sentence == NULL)
    {
        return 0;
    }
    int count = 0;
    int inWord = 0;
    for (int i = 0; sentence[i] != '\0'; i++)
    {
        unsigned char c = (unsigned char)sentence[i];
        if (c != ' ' && c != '\r' && c != '\n' && c != '\t')
        {
            if (!inWord)
            {
                inWord = 1;
                count++;
            }
        }
        else
        {
            inWord = 0;
        }
    }
    return count;
}

