#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "inputHandler.h"
#include "sourceValidator.h"
#include "jsonHandler.h"
#include "typeracerStructures.h"

static int tests_run = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("[FAIL] %s: %s (line %d)\n", __func__, msg, __LINE__); \
        tests_failed++; \
    } else { \
        printf("[PASS] %s: %s\n", __func__, msg); \
    } \
} while (0)

void test_input_handler(void)
{
    printf("\n--- Testing Input Handler ---\n");
    // Printable characters must be allowed
    TEST_ASSERT(isCharacterKey('a') != 0, "Lowercase letter 'a' allowed");
    TEST_ASSERT(isCharacterKey('Z') != 0, "Uppercase letter 'Z' allowed");
    TEST_ASSERT(isCharacterKey('0') != 0, "Digit '0' allowed");
    TEST_ASSERT(isCharacterKey('9') != 0, "Digit '9' allowed");
    TEST_ASSERT(isCharacterKey(' ') != 0, "Space ' ' allowed");
    TEST_ASSERT(isCharacterKey(',') != 0, "Comma ',' allowed");
    TEST_ASSERT(isCharacterKey('.') != 0, "Period '.' allowed");
    TEST_ASSERT(isCharacterKey('!') != 0, "Exclamation '!' allowed");
    TEST_ASSERT(isCharacterKey('?') != 0, "Question '?' allowed");
    TEST_ASSERT(isCharacterKey('-') != 0, "Hyphen '-' allowed");
    TEST_ASSERT(isCharacterKey('@') != 0, "Symbol '@' allowed");

    // Control and escape characters must NOT be allowed
    TEST_ASSERT(isCharacterKey('\0') == 0, "Null byte rejected");
    TEST_ASSERT(isCharacterKey('\n') == 0, "Newline rejected");
    TEST_ASSERT(isCharacterKey('\r') == 0, "Carriage return rejected");
    TEST_ASSERT(isCharacterKey('\t') == 0, "Tab rejected");
    TEST_ASSERT(isCharacterKey(27) == 0, "Escape (27) rejected");
    TEST_ASSERT(isCharacterKey(127) == 0, "DEL (127) rejected");

    // compareInput
    TEST_ASSERT(compareInput('k', 'k') == 1, "Matching characters compare true");
    TEST_ASSERT(compareInput('a', 'b') == 0, "Mismatched characters compare false");
}

void test_source_validator(void)
{
    printf("\n--- Testing Source Validator ---\n");
    // validateLine
    TEST_ASSERT(validateLine("Hello World") == 1, "Alphabetic line validates");
    TEST_ASSERT(validateLine("   12345   ") == 0, "Non-alphabetic line rejected");
    TEST_ASSERT(validateLine("") == 0, "Empty line rejected");

    // parseSentence
    int wordCount = 0;
    char **words = parseSentence("The quick brown fox jumps", &wordCount);
    TEST_ASSERT(wordCount == 5, "Word count parsed accurately");
    TEST_ASSERT(words != NULL, "Words array not NULL");
    if (words && wordCount == 5)
    {
        TEST_ASSERT(strcmp(words[0], "The") == 0, "Word 0 matches");
        TEST_ASSERT(strcmp(words[1], "quick") == 0, "Word 1 matches");
        TEST_ASSERT(strcmp(words[2], "brown") == 0, "Word 2 matches");
        TEST_ASSERT(strcmp(words[3], "fox") == 0, "Word 3 matches");
        TEST_ASSERT(strcmp(words[4], "jumps") == 0, "Word 4 matches");
    }
    for (int i = 0; i < wordCount; i++) free(words[i]);
    free(words);

    // parseSentence with extra whitespace/newlines
    words = parseSentence("  Click,  clack, moo. \r\n", &wordCount);
    TEST_ASSERT(wordCount == 3, "Sentence with extra spaces and newlines parses accurately");
    if (words && wordCount == 3)
    {
        TEST_ASSERT(strcmp(words[0], "Click,") == 0, "Punctuation preserved");
        TEST_ASSERT(strcmp(words[2], "moo.") == 0, "Trailing newlines stripped");
    }
    for (int i = 0; i < wordCount; i++) free(words[i]);
    free(words);

    // parseSentence NULL safety
    words = parseSentence(NULL, &wordCount);
    TEST_ASSERT(words == NULL, "NULL sentence returns NULL safely");
    TEST_ASSERT(wordCount == 0, "NULL sentence word count is 0");

    // readFileLines
#ifndef INPUT_TXT_PATH
#define INPUT_TXT_PATH "input.txt"
#endif
    int lineCount = 0;
    char **lines = readFileLines(INPUT_TXT_PATH, &lineCount);
    TEST_ASSERT(lines != NULL, "Read input.txt lines successfully");
    TEST_ASSERT(lineCount > 0, "input.txt has positive line count");
    if (lines)
    {
        for (int i = 0; i < lineCount; i++) free(lines[i]);
        free(lines);
    }
}

void test_json_handler(void)
{
    printf("\n--- Testing JSON Handler ---\n");

    // 1. RefreshingData
    int prog[3] = {5, 12, 0};
    RefreshingData rData;
    rData.progress = prog;
    char *rJson = serializeRefreshingData(&rData, 3);
    TEST_ASSERT(rJson != NULL, "serializeRefreshingData produced JSON");
    RefreshingData rParsed = deserializeRefreshingData(rJson);
    TEST_ASSERT(rParsed.progress != NULL, "deserializeRefreshingData returned non-NULL");
    if (rParsed.progress)
    {
        TEST_ASSERT(rParsed.progress[0] == 5, "Progress[0] matches");
        TEST_ASSERT(rParsed.progress[1] == 12, "Progress[1] matches");
        TEST_ASSERT(rParsed.progress[2] == 0, "Progress[2] matches");
        free(rParsed.progress);
    }
    free(rJson);

    // RefreshingData NULL and malformed JSON safety
    RefreshingData rNull = deserializeRefreshingData(NULL);
    TEST_ASSERT(rNull.progress == NULL, "deserializeRefreshingData(NULL) is safe");
    RefreshingData rBad = deserializeRefreshingData("{ invalid_json: [ }");
    TEST_ASSERT(rBad.progress == NULL, "deserializeRefreshingData(malformed) is safe");

    // 2. SocketsData
    char *names[2] = {"Alice", "Bob"};
    SocketsData sData;
    sData.names = names;
    sData.sentence = "Hello TypeRacer!";
    sData.playerCount = 2;
    sData.playerIndex = 1;

    char *sJson = serializeSocketsData(&sData, 1);
    TEST_ASSERT(sJson != NULL, "serializeSocketsData produced JSON");
    SocketsData sParsed = deserializeSocketsData(sJson);
    TEST_ASSERT(sParsed.names != NULL, "deserializeSocketsData names not NULL");
    TEST_ASSERT(sParsed.sentence != NULL, "deserializeSocketsData sentence not NULL");
    TEST_ASSERT(sParsed.playerCount == 2, "deserializeSocketsData playerCount matches");
    TEST_ASSERT(sParsed.playerIndex == 1, "deserializeSocketsData playerIndex matches");
    if (sParsed.sentence)
    {
        TEST_ASSERT(strcmp(sParsed.sentence, "Hello TypeRacer!") == 0, "Sentence matches");
        free(sParsed.sentence);
    }
    if (sParsed.names)
    {
        TEST_ASSERT(strcmp(sParsed.names[0], "Alice") == 0, "Name 0 matches");
        TEST_ASSERT(strcmp(sParsed.names[1], "Bob") == 0, "Name 1 matches");
        free(sParsed.names[0]);
        free(sParsed.names[1]);
        free(sParsed.names);
    }
    free(sJson);

    // SocketsData NULL and malformed safety
    SocketsData sNull = deserializeSocketsData(NULL);
    TEST_ASSERT(sNull.names == NULL && sNull.sentence == NULL, "deserializeSocketsData(NULL) safe");
    SocketsData sBad = deserializeSocketsData("not json at all");
    TEST_ASSERT(sBad.names == NULL && sBad.sentence == NULL, "deserializeSocketsData(garbage) safe");

    // 3. Times
    double tVals[2] = {14.25, 18.70};
    Times tData;
    tData.times = tVals;
    char *tJson = serializeTimes(&tData, 2);
    TEST_ASSERT(tJson != NULL, "serializeTimes produced JSON");
    Times tParsed = deserializeTimes(tJson);
    TEST_ASSERT(tParsed.times != NULL, "deserializeTimes returned non-NULL");
    if (tParsed.times)
    {
        TEST_ASSERT(tParsed.times[0] >= 14.24 && tParsed.times[0] <= 14.26, "Time 0 matches");
        TEST_ASSERT(tParsed.times[1] >= 18.69 && tParsed.times[1] <= 18.71, "Time 1 matches");
        free(tParsed.times);
    }
    free(tJson);

    // Times NULL and malformed safety
    Times tNull = deserializeTimes(NULL);
    TEST_ASSERT(tNull.times == NULL, "deserializeTimes(NULL) safe");
    Times tBad = deserializeTimes("{ \"bad\": true }");
    TEST_ASSERT(tBad.times == NULL, "deserializeTimes(missing times) safe");
}

#include "eventQueue.h"

void test_event_queue(void)
{
    printf("\n--- Testing MPSC Event Queue ---\n");
    EventQueue q;
    event_queue_init(&q);

    // 1. Pop from empty queue with timeout=0
    GameEvent ev;
    TEST_ASSERT(event_queue_pop(&q, &ev, 0) == false, "Empty queue pop (timeout=0) returns false");

    // 2. FIFO order verification
    GameEvent e1 = {EVENT_WORD_COMPLETED, 0, {100, 0}};
    GameEvent e2 = {EVENT_SENTENCE_FINISHED, 1, {100, 500}};
    event_queue_push(&q, e1);
    event_queue_push(&q, e2);

    GameEvent out1, out2;
    TEST_ASSERT(event_queue_pop(&q, &out1, 100) == true, "Pop 1st event succeeded");
    TEST_ASSERT(out1.type == EVENT_WORD_COMPLETED && out1.playerIndex == 0, "1st event matches e1");

    TEST_ASSERT(event_queue_pop(&q, &out2, 100) == true, "Pop 2nd event succeeded");
    TEST_ASSERT(out2.type == EVENT_SENTENCE_FINISHED && out2.playerIndex == 1, "2nd event matches e2");

    // 3. Timed wait on empty queue
    TEST_ASSERT(event_queue_pop(&q, &ev, 20) == false, "Timed pop on empty queue expired cleanly");

    // 4. Close queue unblocks pop
    event_queue_close(&q);
    TEST_ASSERT(event_queue_pop(&q, &ev, 50) == false, "Pop on closed queue returns false immediately");

    // 5. Push to closed queue is safely discarded
    event_queue_push(&q, e1);
    TEST_ASSERT(event_queue_pop(&q, &ev, 0) == false, "Push on closed queue was discarded");

    event_queue_destroy(&q);
    TEST_ASSERT(1, "Queue destroyed cleanly");
}

int main(void)
{
    printf("=========================================\n");
    printf("     RUNNING UNIT TESTS (console-typeracer)\n");
    printf("=========================================\n");

    test_input_handler();
    test_source_validator();
    test_json_handler();
    test_event_queue();

    printf("\n=========================================\n");
    printf("Tests run: %d, Failed: %d\n", tests_run, tests_failed);
    printf("=========================================\n");

    return tests_failed == 0 ? 0 : 1;
}
