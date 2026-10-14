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

    // Send name
    char nameBuf[20] = {0};
    strncpy(nameBuf, cargs->name, sizeof(nameBuf) - 1);
    if (send_all(sock, nameBuf, 20) < 0)
    {
        close(sock);
        return NULL;
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

    // Simulate typing words: send '3'
    for (int i = 0; i < cargs->wordsToComplete; i++)
    {
        char wordDone = '3';
        send_all(sock, &wordDone, 1);
        usleep(10000); // 10ms simulate typing delay
    }

    // Finish sentence: send '2'
    char finishByte = '2';
    send_all(sock, &finishByte, 1);

    // Send reported time (legacy/anti-cheat check)
    char timeStr[50] = {0};
    snprintf(timeStr, sizeof(timeStr), "%.2f", cargs->reportedTime);
    send_all(sock, timeStr, sizeof(timeStr));

    // Read until results with "times" arrive
    char streamBuf[2048] = {0};
    int totalBytes = 0;
    while (totalBytes < (int)sizeof(streamBuf) - 1)
    {
        int b = recv(sock, streamBuf + totalBytes, sizeof(streamBuf) - 1 - totalBytes, 0);
        if (b <= 0) break;
        totalBytes += b;
        streamBuf[totalBytes] = '\0';

        char *timesPtr = memmem(streamBuf, totalBytes, "times", 5);
        if (timesPtr != NULL)
        {
            char *objStart = timesPtr;
            while (objStart > streamBuf && *objStart != '{')
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
    }
    if (!cargs->finishedSuccessfully)
    {
        printf("[%s] DEBUG: totalBytes=%d streamBuf='%s'\n", cargs->name, totalBytes, streamBuf);
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

void test_full_two_player_race(void)
{
    printf("\n--- Testing Full Simulated 2-Player Race ---\n");
    signal(SIGPIPE, SIG_IGN);

#ifndef INPUT_TXT_PATH
#define INPUT_TXT_PATH "input.txt"
#endif
    // Prepare arguments for server thread
    char *serverArgs[5];
    serverArgs[0] = "server";
    serverArgs[1] = "2";         // 2 players
    serverArgs[2] = INPUT_TXT_PATH;  // file
    serverArgs[3] = "1";         // logging enabled
    serverArgs[4] = NULL;

    pthread_t serverThread;
    int ret = pthread_create(&serverThread, NULL, startupServer, serverArgs);
    TEST_ASSERT(ret == 0, "Server thread spawned successfully");

    // Launch 2 simulated client threads
    SimulatedClientArgs client1 = {"Racer1", 54897, 3, 10.50, false, 0.0, false};
    SimulatedClientArgs client2 = {"Racer2", 54897, 3, 12.80, false, 0.0, false};

    pthread_t c1Thread, c2Thread;
    pthread_create(&c1Thread, NULL, simulated_client_thread, &client1);
    pthread_create(&c2Thread, NULL, simulated_client_thread, &client2);

    pthread_join(c1Thread, NULL);
    pthread_join(c2Thread, NULL);

    TEST_ASSERT(client1.finishedSuccessfully == true, "Client 1 completed race & received results");
    TEST_ASSERT(client2.finishedSuccessfully == true, "Client 2 completed race & received results");
    TEST_ASSERT(client1.serverRecordedTime > 0.0 && client1.serverRecordedTime < 5.0,
                "Client 1 received server-authoritative monotonic timestamp");

    // Wait for server thread to cleanly terminate
    pthread_join(serverThread, NULL);
    TEST_ASSERT(1, "Server thread cleanly joined without deadlocking");
}

void test_authoritative_timing_and_disconnect(void)
{
    printf("\n--- Testing Authoritative Timing Anti-Cheat & Mid-Race Disconnect ---\n");
    signal(SIGPIPE, SIG_IGN);
    usleep(100000); // 100ms pause between runs

    char *serverArgs[5];
    serverArgs[0] = "server";
    serverArgs[1] = "2";
    serverArgs[2] = INPUT_TXT_PATH;
    serverArgs[3] = "1";
    serverArgs[4] = NULL;

    pthread_t serverThread;
    int ret = pthread_create(&serverThread, NULL, startupServer, serverArgs);
    TEST_ASSERT(ret == 0, "Server spawned for anti-cheat and disconnect test");

    // Racer1 reports fake 999.99s, Racer2 disconnects abruptly mid-race
    SimulatedClientArgs client1 = {"HonestFinisher", 54897, 3, 999.99, false, 0.0, false};
    SimulatedClientArgs client2 = {"AbruptQuitter", 54897, 1, 0.0, false, 0.0, true};

    pthread_t c1Thread, c2Thread;
    pthread_create(&c1Thread, NULL, simulated_client_thread, &client1);
    pthread_create(&c2Thread, NULL, simulated_client_thread, &client2);

    pthread_join(c1Thread, NULL);
    pthread_join(c2Thread, NULL);

    TEST_ASSERT(client1.finishedSuccessfully == true, "Finishing client received race results");
    TEST_ASSERT(client1.serverRecordedTime > 0.0 && client1.serverRecordedTime < 5.0,
                "Server ignored fake 999.99s client time and calculated authoritative monotonic time");

    pthread_join(serverThread, NULL);
    TEST_ASSERT(1, "Server cleanly terminated despite abrupt client disconnect");
}

int main(void)
{
    printf("=========================================\n");
    printf("   RUNNING NETWORK TESTS (console-typeracer)\n");
    printf("=========================================\n");

    test_socket_lifecycle();
    test_full_two_player_race();
    test_authoritative_timing_and_disconnect();

    printf("\n=========================================\n");
    printf("Tests run: %d, Failed: %d\n", tests_run, tests_failed);
    printf("=========================================\n");

    return tests_failed == 0 ? 0 : 1;
}
