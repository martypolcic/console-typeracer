#include "jsonHandler.h"

char *serializeRefreshingData(const RefreshingData *data, int playerCount)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *progressArray = cJSON_CreateIntArray(data->progress, playerCount);
    cJSON_AddItemToObject(json, "progress", progressArray);
    char *jsonString = cJSON_Print(json);
    cJSON_Delete(json);
    return jsonString;
}

RefreshingData deserializeRefreshingData(const char *jsonString)
{
    RefreshingData data;
    cJSON *json = cJSON_Parse(jsonString);
    cJSON *progressArray = cJSON_GetObjectItem(json, "progress");
    int progressArraySize = cJSON_GetArraySize(progressArray);
    data.progress = malloc(progressArraySize * sizeof(int));
    for (int i = 0; i < progressArraySize; i++)
    {
        cJSON *progressItem = cJSON_GetArrayItem(progressArray, i);
        data.progress[i] = progressItem->valueint;
    }
    cJSON_Delete(json);
    return data;
}

char *serializeSocketsData(const SocketsData *data, const int playerIndex)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *namesArray = cJSON_CreateStringArray((const char *const *)data->names, data->playerCount);
    cJSON_AddItemToObject(json, "names", namesArray);
    cJSON_AddStringToObject(json, "sentence", data->sentence);
    cJSON_AddNumberToObject(json, "playerCount", data->playerCount);
    cJSON_AddNumberToObject(json, "playerIndex", playerIndex);
    char *jsonString = cJSON_Print(json);
    cJSON_Delete(json);
    return jsonString;
}

SocketsData deserializeSocketsData(const char *jsonString)
{
    SocketsData data;
    cJSON *json = cJSON_Parse(jsonString);
    cJSON *namesArray = cJSON_GetObjectItem(json, "names");
    if (namesArray != NULL)
    {
        int namesArraySize = cJSON_GetArraySize(namesArray);
        data.names = malloc(namesArraySize * sizeof(char *));
        for (int i = 0; i < namesArraySize; i++)
        {
            cJSON *nameItem = cJSON_GetArrayItem(namesArray, i);
            data.names[i] = strdup(nameItem->valuestring);
        }
    }
    cJSON *sentenceItem = cJSON_GetObjectItem(json, "sentence");
    if (sentenceItem != NULL)
    {
        data.sentence = strdup(sentenceItem->valuestring);
    }
    cJSON *playerCountItem = cJSON_GetObjectItem(json, "playerCount");
    data.playerCount = playerCountItem->valueint;
    cJSON *playerIndexItem = cJSON_GetObjectItem(json, "playerIndex");
    data.playerIndex = playerIndexItem->valueint;
    cJSON_Delete(json);
    return data;
}

int calculateRefreshingDataSize(int playerCount, int wordCount)
{
    int *progress = calloc(playerCount, sizeof(int));
    for (int i = 0; i < playerCount; i++)
    {
        progress[i] = wordCount;
    }

    RefreshingData data;
    data.progress = progress;

    char *jsonString = serializeRefreshingData(&data, playerCount);
    int size = strlen(jsonString);
    free(progress);
    free(jsonString);
    return size;
}

char *serializeTimes(const Times *data, int playerCount)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *timesArray = cJSON_CreateDoubleArray(data->times, playerCount);
    cJSON_AddItemToObject(json, "times", timesArray);
    char *jsonString = cJSON_Print(json);
    cJSON_Delete(json);
    return jsonString;
}

Times deserializeTimes(const char *jsonString)
{
    Times data;
    cJSON *json = cJSON_Parse(jsonString);
    cJSON *timesArray = cJSON_GetObjectItem(json, "times");
    int timesArraySize = cJSON_GetArraySize(timesArray);
    data.times = malloc(timesArraySize * sizeof(double));
    for (int i = 0; i < timesArraySize; i++)
    {
        cJSON *timeItem = cJSON_GetArrayItem(timesArray, i);
        data.times[i] = timeItem->valuedouble;
    }
    cJSON_Delete(json);
    return data;
}