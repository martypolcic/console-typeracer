#ifndef JSON_HANDLER_H
#define JSON_HANDLER_H

#include "cJSON.h"
#include "typeracerStructures.h"

// Function prototypes for RefreshingData
char* serializeRefreshingData(const RefreshingData* data, int playerCount);
RefreshingData deserializeRefreshingData(const char* jsonString);

// Function prototypes for SocketsData
char* serializeSocketsData(const SocketsData* data);
SocketsData deserializeSocketsData(const char* jsonString);

#endif // JSON_HANDLER_H