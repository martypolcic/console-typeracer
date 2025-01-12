#include "server.h"

int main(int argc, char **argv)
{
    srand(time(NULL));
    printf("argc: %d\n", argc);
    if (argc < 3)
    {
        printf("Usage: %s <max-players> <filename> <logging_enabled{0,1}>\n", argv[0]);
        return -1;
    }
    startupServer(argv);
    return 0;
}
