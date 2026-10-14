#ifndef SOCKET
#define SOCKET
#include <stddef.h>
#include <sys/types.h>

int passive_socket_init(const int port);
int passive_socket_wait_for_client(int passiveSocket);
void passive_socket_destroy(int socket);
 
int connect_to_server(const char * name, const int port);
void active_socket_destroy(int socket);

int send_all(int socket, const void *buffer, size_t length);
int recv_all(int socket, void *buffer, size_t length);

#endif // !SOCKET
