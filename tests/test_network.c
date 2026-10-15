#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "socket.h"
#include "server.h"
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

// Helper thread for simulated client in full race test
typedef struct SimulatedClientArgs {
    const char *name;
    int port;
    int wordsToComplete;
    double reportedTime;
    bool finishedSuccessfully;
    double serverRecordedTime;
    bool disconnectMidRace;
    const char *roomCode;
    bool prematureFinish;
} SimulatedClientArgs;

void *simulated_client_thread(void *arg)
{
    SimulatedClientArgs *cargs = (SimulatedClientArgs *)arg;
    cargs->finishedSuccessfully = false;
    cargs->serverRecordedTime = -999.0;

    // Retry connect up to 20 times (1 second total) while server is binding
    int sock = -1;
    for (int retry = 0; retry < 20; retry++)
    {
        sock = connect_to_server("localhost", cargs->port);
        if (sock >= 0)
        {
            break;
        }
        usleep(50000); // 50ms
    }
    if (sock < 0)
    {
        perror("Simulated client failed to connect after retries");
        return NULL;
    }

    // Send handshake or legacy name
    if (cargs->roomCode != NULL)
    {
        char handshake[64] = {0};
        snprintf(handshake, sizeof(handshake), "JOIN %s %s\n", cargs->roomCode, cargs->name);
        if (send_all(sock, handshake, strlen(handshake)) < 0)
        {
            close(sock);
            return NULL;
        }
    }
    else
    {
        char nameBuf[20] = {0};
        strncpy(nameBuf, cargs->name, sizeof(nameBuf) - 1);
        if (send_all(sock, nameBuf, 20) < 0)
        {
            close(sock);
            return NULL;
        }
    }

    // Receive SocketsData length
    char lenBuf[11] = {0};
    if (recv_all(sock, lenBuf, 10) < 0)
    {
        close(sock);
        return NULL;
    }
    int dataLen = atoi(lenBuf);
    if (dataLen <= 0)
    {
        close(sock);
        return NULL;
    }

    // Receive SocketsData JSON
    char *dataJson = calloc(dataLen + 1, 1);
    if (recv_all(sock, dataJson, dataLen) < 0)
    {
        free(dataJson);
        close(sock);
        return NULL;
    }
    SocketsData sData = deserializeSocketsData(dataJson);
    free(dataJson);

    // Receive initial RefreshingData JSON
    char rBuf[256] = {0};
    recv(sock, rBuf, sizeof(rBuf) - 1, 0);

    if (cargs->disconnectMidRace)
    {
        // Simulate typing 1 word, then abrupt disconnect
        char wordDone = '3';
        send_all(sock, &wordDone, 1);
        usleep(20000); // 20ms
        close(sock);
        if (sData.sentence) free(sData.sentence);
        if (sData.names)
        {
            for (int i = 0; i < sData.playerCount; i++) free(sData.names[i]);
            free(sData.names);
        }
        return NULL;
    }

    int totalWords = 0;
    char **parsedWords = parseSentence(sData.sentence, &totalWords);
    if (parsedWords)
    {
        for (int i = 0; i < totalWords; i++) free(parsedWords[i]);
        free(parsedWords);
    }
    int wordsToType = (totalWords > 1) ? totalWords - 1 : 0;
    if (cargs->prematureFinish)
    {
        wordsToType = 0; // Skip intermediate word packets to simulate premature finish attempt
    }

    // Simulate typing words: send '3'
    for (int i = 0; i < wordsToType; i++)
    {
        char wordDone = '3';
        send_all(sock, &wordDone, 1);
        char drain[512];
        recv(sock, drain, sizeof(drain), MSG_DONTWAIT);
        usleep(1000); // 1ms simulate typing delay
    }

    // Finish sentence: send '2'
    char finishByte = '2';
    send_all(sock, &finishByte, 1);

    // Send reported time (legacy/anti-cheat check)
    char timeStr[50] = {0};
    snprintf(timeStr, sizeof(timeStr), "%.2f", cargs->reportedTime);
    send_all(sock, timeStr, sizeof(timeStr));

    // Read until results with "times" arrive
    char accum[4096] = {0};
    int accumLen = 0;
    while (true)
    {
        char temp[512] = {0};
        int b = recv(sock, temp, sizeof(temp) - 1, 0);
        if (b <= 0) break;
        temp[b] = '\0';

        if (accumLen + b < (int)sizeof(accum) - 1)
        {
            memcpy(accum + accumLen, temp, b);
            accumLen += b;
            accum[accumLen] = '\0';
        }
        else
        {
            accumLen = 0;
            accum[0] = '\0';
        }

        char *timesPtr = strstr(accum, "\"times\"");
        if (timesPtr != NULL)
        {
            char *objStart = timesPtr;
            while (objStart > accum && *objStart != '{')
            {
                objStart--;
            }
            if (*objStart == '{')
            {
                Times times = deserializeTimes(objStart);
                if (times.times != NULL)
                {
                    cargs->finishedSuccessfully = true;
                    if (sData.playerIndex >= 0 && sData.playerIndex < sData.playerCount)
                    {
                        cargs->serverRecordedTime = times.times[sData.playerIndex];
                    }
                    free(times.times);
                    break;
                }
            }
        }
        else
        {
            char *lastBrace = strrchr(accum, '{');
            if (lastBrace != NULL && lastBrace > accum)
            {
                int rem = strlen(lastBrace);
                memmove(accum, lastBrace, rem + 1);
                accumLen = rem;
            }
            else if (lastBrace == NULL)
            {
                accumLen = 0;
                accum[0] = '\0';
            }
        }
    }

    if (sData.sentence) free(sData.sentence);
    if (sData.names)
    {
        for (int i = 0; i < sData.playerCount; i++) free(sData.names[i]);
        free(sData.names);
    }
    close(sock);
    return NULL;
}

void test_socket_lifecycle(void)
{
    printf("\n--- Testing Socket Lifecycle & SO_REUSEADDR ---\n");
    int port = 54890;
    int sock1 = passive_socket_init(port);
    TEST_ASSERT(sock1 >= 0, "Passive socket created and bound");
    passive_socket_destroy(sock1);

    // Immediate re-bind on same port (testing SO_REUSEADDR)
    int sock2 = passive_socket_init(port);
    TEST_ASSERT(sock2 >= 0, "Immediate re-bind on same port succeeded (SO_REUSEADDR works)");
    passive_socket_destroy(sock2);
}

typedef struct CoordinatorTestArgs {
    int port;
    const char *fileName;
    int maxPlayers;
} CoordinatorTestArgs;

void *coordinator_thread(void *arg)
{
    CoordinatorTestArgs *args = (CoordinatorTestArgs *)arg;
    startCoordinatorServer(args->port, args->fileName, args->maxPlayers);
    return NULL;
}

void test_full_two_player_race(void)
{
    printf("\n--- Testing Full Simulated 2-Player Race ---\n");
    signal(SIGPIPE, SIG_IGN);

#ifndef INPUT_TXT_PATH
#define INPUT_TXT_PATH "input.txt"
#endif
    int port = 54897;
    CoordinatorTestArgs cargs = {port, INPUT_TXT_PATH, 2};
    pthread_t serverThread;
    int ret = pthread_create(&serverThread, NULL, coordinator_thread, &cargs);
    TEST_ASSERT(ret == 0, "Server thread spawned successfully");
    usleep(100000); // 100ms for coordinator to bind

    // Launch 2 simulated client threads
    SimulatedClientArgs client1 = {"Racer1", port, 3, 10.50, false, 0.0, false, "DEFAULT", false};
    SimulatedClientArgs client2 = {"Racer2", port, 3, 12.80, false, 0.0, false, "DEFAULT", false};

    pthread_t c1Thread, c2Thread;
    pthread_create(&c1Thread, NULL, simulated_client_thread, &client1);
    pthread_create(&c2Thread, NULL, simulated_client_thread, &client2);

    pthread_join(c1Thread, NULL);
    pthread_join(c2Thread, NULL);

    TEST_ASSERT(client1.finishedSuccessfully == true, "Client 1 completed race & received results");
    TEST_ASSERT(client2.finishedSuccessfully == true, "Client 2 completed race & received results");
    TEST_ASSERT(client1.serverRecordedTime > 0.0 && client1.serverRecordedTime < 5.0,
                "Client 1 received server-authoritative monotonic timestamp");

    stopCoordinatorServer();
    pthread_join(serverThread, NULL);
    TEST_ASSERT(1, "Server thread cleanly joined without deadlocking");
}

void test_authoritative_timing_and_disconnect(void)
{
    printf("\n--- Testing Authoritative Timing Anti-Cheat & Mid-Race Disconnect ---\n");
    signal(SIGPIPE, SIG_IGN);
    usleep(100000); // 100ms pause between runs

    int port = 54898;
    CoordinatorTestArgs cargs = {port, INPUT_TXT_PATH, 2};
    pthread_t serverThread;
    int ret = pthread_create(&serverThread, NULL, coordinator_thread, &cargs);
    TEST_ASSERT(ret == 0, "Server spawned for anti-cheat and disconnect test");
    usleep(100000); // 100ms for coordinator to bind

    // Racer1 reports fake 999.99s, Racer2 disconnects abruptly mid-race
    SimulatedClientArgs client1 = {"HonestFinisher", port, 3, 999.99, false, 0.0, false, "DEFAULT", false};
    SimulatedClientArgs client2 = {"AbruptQuitter", port, 1, 0.0, false, 0.0, true, "DEFAULT", false};

    pthread_t c1Thread, c2Thread;
    pthread_create(&c1Thread, NULL, simulated_client_thread, &client1);
    pthread_create(&c2Thread, NULL, simulated_client_thread, &client2);

    pthread_join(c1Thread, NULL);
    pthread_join(c2Thread, NULL);

    TEST_ASSERT(client1.finishedSuccessfully == true, "Finishing client received race results");
    TEST_ASSERT(client1.serverRecordedTime > 0.0 && client1.serverRecordedTime < 5.0,
                "Server ignored fake 999.99s client time and calculated authoritative monotonic time");

    stopCoordinatorServer();
    pthread_join(serverThread, NULL);
    TEST_ASSERT(1, "Server cleanly terminated despite abrupt client disconnect");
}

void test_handshake_parsing(void)
{
    printf("\n--- Testing Client Handshake Parsing ---\n");

    char room[16];
    char name[32];

    // Standard JOIN with room and player name
    parseClientHandshake("JOIN ROOM42 Alice\n", 18, room, sizeof(room), name, sizeof(name));
    TEST_ASSERT(strcmp(room, "ROOM42") == 0, "Handshake parses custom room code");
    TEST_ASSERT(strcmp(name, "Alice") == 0, "Handshake parses player name");

    // Legacy client name without JOIN prefix
    parseClientHandshake("LegacyBob\r\n", 11, room, sizeof(room), name, sizeof(name));
    TEST_ASSERT(strcmp(room, "DEFAULT") == 0, "Legacy format defaults to DEFAULT room");
    TEST_ASSERT(strcmp(name, "LegacyBob") == 0, "Legacy format extracts full name");

    // Whitespace and empty variations
    parseClientHandshake("", 0, room, sizeof(room), name, sizeof(name));
    TEST_ASSERT(strcmp(room, "DEFAULT") == 0, "Empty buffer defaults to DEFAULT room");
    TEST_ASSERT(strcmp(name, "Player") == 0, "Empty buffer defaults to Player name");

    // Extra spaces around JOIN
    parseClientHandshake("JOIN   BETA99   Charlie  \n", 26, room, sizeof(room), name, sizeof(name));
    TEST_ASSERT(strcmp(room, "BETA99") == 0, "Handshake trims extra spaces for room");
    TEST_ASSERT(strcmp(name, "Charlie") == 0, "Handshake trims extra spaces for name");
}

void test_multi_lobby_concurrent_races(void)
{
    printf("\n--- Testing Multi-Lobby Concurrent Coordinator Races ---\n");
    signal(SIGPIPE, SIG_IGN);
    int testPort = 54902;

    CoordinatorTestArgs cArgs = { testPort, INPUT_TXT_PATH, 2 };
    pthread_t cThread;
    int ret = pthread_create(&cThread, NULL, coordinator_thread, &cArgs);
    TEST_ASSERT(ret == 0, "Coordinator daemon spawned on test port");

    usleep(100000); // 100ms for listening socket to open

    // Test INFO command
    int infoSock = connect_to_server("localhost", testPort);
    TEST_ASSERT(infoSock >= 0, "Connected to coordinator to query INFO");
    char infoBuf[1024] = {0};
    send_all(infoSock, "INFO\n", 5);
    int infoBytes = recv(infoSock, infoBuf, sizeof(infoBuf) - 1, 0);
    close(infoSock);
    TEST_ASSERT(infoBytes > 0 && strstr(infoBuf, "healthy") != NULL, "INFO query returned healthy status");

    // Launch 2 concurrent races in 2 distinct lobbies
    SimulatedClientArgs clientA1 = {"Alpha_1", testPort, 3, 5.0, false, 0.0, false, "ROOM_ALPHA", false};
    SimulatedClientArgs clientA2 = {"Alpha_2", testPort, 3, 5.5, false, 0.0, false, "ROOM_ALPHA", false};

    SimulatedClientArgs clientB1 = {"Beta_1", testPort, 3, 6.0, false, 0.0, false, "ROOM_BETA", false};
    SimulatedClientArgs clientB2 = {"Beta_2", testPort, 3, 6.5, false, 0.0, false, "ROOM_BETA", false};

    pthread_t thA1, thA2, thB1, thB2;
    pthread_create(&thA1, NULL, simulated_client_thread, &clientA1);
    pthread_create(&thA2, NULL, simulated_client_thread, &clientA2);
    pthread_create(&thB1, NULL, simulated_client_thread, &clientB1);
    pthread_create(&thB2, NULL, simulated_client_thread, &clientB2);

    pthread_join(thA1, NULL);
    pthread_join(thA2, NULL);
    pthread_join(thB1, NULL);
    pthread_join(thB2, NULL);

    TEST_ASSERT(clientA1.finishedSuccessfully == true, "Room ALPHA Racer 1 finished race");
    TEST_ASSERT(clientA2.finishedSuccessfully == true, "Room ALPHA Racer 2 finished race");
    TEST_ASSERT(clientB1.finishedSuccessfully == true, "Room BETA Racer 1 finished race");
    TEST_ASSERT(clientB2.finishedSuccessfully == true, "Room BETA Racer 2 finished race");

    // Query SUMMARY for ROOM_ALPHA
    int sumSock = connect_to_server("localhost", testPort);
    TEST_ASSERT(sumSock >= 0, "Connected to coordinator for SUMMARY query");
    char sumBuf[2048] = {0};
    send_all(sumSock, "SUMMARY ROOM_ALPHA\n", 19);
    int sumBytes = recv(sumSock, sumBuf, sizeof(sumBuf) - 1, 0);
    close(sumSock);
    TEST_ASSERT(sumBytes > 0 && strstr(sumBuf, "ROOM_ALPHA") != NULL && strstr(sumBuf, "FINISHED") != NULL,
                "Coordinator generated summary for ROOM_ALPHA");

    // Stop coordinator daemon
    stopCoordinatorServer();
    pthread_join(cThread, NULL);
    TEST_ASSERT(1, "Coordinator daemon cleanly shut down and joined");
}

void test_lobby_prestart_disconnect_recovery(void)
{
    printf("\n--- Testing Lobby Pre-Start Disconnect & Slot Recovery ---\n");
    int testPort = 54904;
    CoordinatorTestArgs cargs = {testPort, INPUT_TXT_PATH, 2};
    pthread_t cThread;
    int rc = pthread_create(&cThread, NULL, coordinator_thread, &cargs);
    TEST_ASSERT(rc == 0, "Coordinator spawned for pre-start disconnect recovery test");
    usleep(100000); // 100ms

    // Connect Client 1 then immediately abort before Client 2 joins
    int flakySock = connect_to_server("localhost", testPort);
    TEST_ASSERT(flakySock >= 0, "Flaky client connected to lobby");
    send_all(flakySock, "JOIN ROOM_RECOVER FlakyPlayer\n", 30);
    usleep(150000); // 150ms
    close(flakySock); // Abrupt close in waiting lobby
    usleep(250000); // 250ms allow server to detect disconnect and mark slot available

    // Now connect 2 valid clients to ROOM_RECOVER
    SimulatedClientArgs client1 = {"ValidPlayer1", testPort, 3, 5.0, false, 0.0, false, "ROOM_RECOVER", false};
    SimulatedClientArgs client2 = {"ValidPlayer2", testPort, 3, 5.5, false, 0.0, false, "ROOM_RECOVER", false};

    pthread_t th1, th2;
    pthread_create(&th1, NULL, simulated_client_thread, &client1);
    pthread_create(&th2, NULL, simulated_client_thread, &client2);

    pthread_join(th1, NULL);
    pthread_join(th2, NULL);

    TEST_ASSERT(client1.finishedSuccessfully == true, "Valid Player 1 finished race after flaky disconnect");
    TEST_ASSERT(client2.finishedSuccessfully == true, "Valid Player 2 finished race after flaky disconnect");

    stopCoordinatorServer();
    pthread_join(cThread, NULL);
    TEST_ASSERT(1, "Coordinator cleanly stopped after recovery test");
}

void test_premature_finish_cheat_detection(void)
{
    printf("\n--- Testing Premature Finish Disqualification (Anti-Cheat / DNF) ---\n");
    int testPort = 54905;
    CoordinatorTestArgs cargs = {testPort, INPUT_TXT_PATH, 2};
    pthread_t cThread;
    int rc = pthread_create(&cThread, NULL, coordinator_thread, &cargs);
    TEST_ASSERT(rc == 0, "Coordinator spawned for premature finish test");
    usleep(100000); // 100ms

    // Player 1 sends premature finish ('2' immediately without typing required words)
    // Player 2 completes the race normally
    SimulatedClientArgs cheater = {"Cheater", testPort, 0, 0.1, false, 0.0, false, "ROOM_CHEAT", true};
    SimulatedClientArgs honest = {"HonestPlayer", testPort, 3, 4.0, false, 0.0, false, "ROOM_CHEAT", false};

    pthread_t th1, th2;
    pthread_create(&th1, NULL, simulated_client_thread, &cheater);
    pthread_create(&th2, NULL, simulated_client_thread, &honest);

    pthread_join(th1, NULL);
    pthread_join(th2, NULL);

    TEST_ASSERT(cheater.finishedSuccessfully == true, "Premature finisher received final race packet without hanging");
    TEST_ASSERT(cheater.serverRecordedTime == -1.0, "Premature finisher was disqualified and recorded as DNF (-1.0)");
    TEST_ASSERT(honest.finishedSuccessfully == true, "Honest player finished successfully");
    TEST_ASSERT(honest.serverRecordedTime > 0.0, "Honest player received positive valid completion time");

    stopCoordinatorServer();
    pthread_join(cThread, NULL);
    TEST_ASSERT(1, "Coordinator cleanly stopped after premature finish test");
}

int main(void)
{
    printf("=========================================\n");
    printf("   RUNNING NETWORK TESTS (console-typeracer)\n");
    printf("=========================================\n");

    test_socket_lifecycle();
    test_handshake_parsing();
    test_full_two_player_race();
    test_authoritative_timing_and_disconnect();
    test_multi_lobby_concurrent_races();
    test_lobby_prestart_disconnect_recovery();
    test_premature_finish_cheat_detection();

    printf("\n=========================================\n");
    printf("Tests run: %d, Failed: %d\n", tests_run, tests_failed);
    printf("=========================================\n");

    return tests_failed == 0 ? 0 : 1;
}

