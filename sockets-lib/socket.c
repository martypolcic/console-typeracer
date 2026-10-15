#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "socket.h"

int passive_socket_init(const int port) {
  int passSock;
  struct sockaddr_in servAddr;
  passSock = socket(AF_INET, SOCK_STREAM, 0);
  if (passSock < 0) {
    perror("Failed to create socket");
    return -1;
  }
  int opt = 1;
  setsockopt(passSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  memset((char*)&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = INADDR_ANY;
  servAddr.sin_port = htons(port);
  if (bind(passSock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
    perror("Failed to bind socket");
    close(passSock);
    return -1;
  }
  if (listen(passSock, 128) < 0) {
    perror("Failed to listen on socket");
    close(passSock);
    return -1;
  }
  return passSock;
}

int passive_socket_wait_for_client(int passiveSocket) {
  struct sockaddr_in clAddr;
  socklen_t clSize = sizeof(clAddr);
  int actSock = accept(passiveSocket, (struct sockaddr*)&clAddr, &clSize);
  if (actSock < 0) {
    perror("Failed to accept socket connection");
    return -1;
  }
  return actSock;
}

void passive_socket_destroy(int socket) {
  if (socket >= 0) {
    close(socket);
  }
}
 
int connect_to_server(const char * name, const int port) {
  struct addrinfo * server;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  char portText[10] = {0};
  snprintf(portText, sizeof(portText), "%d", port);
  int s = getaddrinfo(name, portText, &hints, &server);
  if (s != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    return -1;
  }
  for (struct addrinfo * rp = server; rp != NULL; rp = rp->ai_next) {
    int actSock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (actSock < 0) {
      continue;
    }
    if(connect(actSock, rp->ai_addr, rp->ai_addrlen) < 0) {
      close(actSock);
    } else {
      freeaddrinfo(server); 
      return actSock;
    }
  }
  perror("Failed to connect to server");
  freeaddrinfo(server); 
  return -1;
}

void active_socket_destroy(int socket) {
  if (socket >= 0) {
    close(socket);
  }
}

int send_all(int socket, const void *buffer, size_t length) {
  const char *ptr = (const char *)buffer;
  size_t totalSent = 0;
  while (totalSent < length) {
    ssize_t sent = send(socket, ptr + totalSent, length - totalSent, 0);
    if (sent < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (sent == 0) {
      return -1;
    }
    totalSent += sent;
  }
  return 0;
}

int recv_all(int socket, void *buffer, size_t length) {
  char *ptr = (char *)buffer;
  size_t totalReceived = 0;
  while (totalReceived < length) {
    ssize_t received = recv(socket, ptr + totalReceived, length - totalReceived, 0);
    if (received < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    if (received == 0) {
      return -1;
    }
    totalReceived += received;
  }
  return 0;
}
