#ifndef PARSER_H
#define PARSER_H

#define MAX_WORDS 50
#define MAX_WORD_LENGTH 50

int parseSentence(const char *sentence, char words[MAX_WORDS][MAX_WORD_LENGTH]);

#endif // PARSER_H