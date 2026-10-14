#include <signal.h>
#include "server.h"

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    srand(time(NULL));
    if (argc < 3)
    {
        printf("Usage: %s <max-players> <filename> [logging_enabled{0,1}]\n", argv[0]);
        return -1;
    }
    char *safeArgv[5];
    safeArgv[0] = argv[0];
    safeArgv[1] = argv[1];
    safeArgv[2] = argv[2];
    safeArgv[3] = (argc >= 4 && argv[3]) ? argv[3] : "0";
    safeArgv[4] = NULL;

    startupServer(safeArgv);
    return 0;
}
