#ifndef JSON_HANDLER_H
#define JSON_HANDLER_H

#include "../cJSON-lib/cJSON.h"
#include "typeracerStructures.h"
#include <stdlib.h>
#include <string.h>

// Function prototypes for RefreshingData
char *serializeRefreshingData(const RefreshingData *data, int playerCount);
RefreshingData deserializeRefreshingData(const char *jsonString);

// Function prototypes for SocketsData
char *serializeSocketsData(const SocketsData *data, const int playerIndex);
SocketsData deserializeSocketsData(const char *jsonString);

// Function prototypes for calculating the size of the RefreshingData
int calculateRefreshingDataSize(int playerCount, int wordCount);

// Function protorypes for Time struct
char *serializeTimes(const Times *data, int playerCount);
Times deserializeTimes(const char *jsonString);

#endif // JSON_HANDLER_H