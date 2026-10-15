#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "server.h"

static void handle_server_sigint(int sig)
{
    (void)sig;
    gCoordinatorRunning = 0;
}

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handle_server_sigint);
    signal(SIGTERM, handle_server_sigint);
    srand(time(NULL));

    bool isDaemon = (argc == 1);
    int port = 54897;
    const char *fileName = "input.txt";
    int defaultMaxPlayers = 2;

    if (getenv("ENGINE_LOGGING") && strcmp(getenv("ENGINE_LOGGING"), "1") == 0)
    {
        logging = 1;
    }

    if (argc >= 2)
    {
        if (strcmp(argv[1], "--daemon") == 0 || strcmp(argv[1], "-d") == 0)
        {
            isDaemon = true;
            if (argc >= 3)
            {
                port = atoi(argv[2]);
            }
            if (argc >= 4)
            {
                fileName = argv[3];
            }
            if (argc >= 5)
            {
                defaultMaxPlayers = atoi(argv[4]);
            }
        }
        else if (argc >= 3 && atoi(argv[1]) > 0)
        {
            // Legacy server mode mapped to modern coordinator engine
            defaultMaxPlayers = atoi(argv[1]);
            fileName = argv[2];
            if (argc >= 4 && atoi(argv[3]) == 1)
            {
                logging = 1;
            }
            int rc = startCoordinatorServer(port, fileName, defaultMaxPlayers);
            stopCoordinatorServer();
            return rc;
        }
        else
        {
            printf("Usage:\n");
            printf("  Daemon mode:  %s [--daemon] [port] [filename] [defaultMaxPlayers]\n", argv[0]);
            printf("  Legacy mode:  %s <max-players> <filename> [logging_enabled{0,1}]\n", argv[0]);
            return -1;
        }
    }

    if (isDaemon)
    {
        printf("Starting TypeRacer Coordinator Daemon on port %d...\n", port);
        startCoordinatorServer(port, fileName, defaultMaxPlayers);
        printf("\nShutting down TypeRacer coordinator...\n");
        stopCoordinatorServer();
    }

    return 0;
}
