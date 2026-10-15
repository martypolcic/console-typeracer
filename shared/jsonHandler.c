#include "jsonHandler.h"

char *serializeRefreshingData(const RefreshingData *data, int playerCount)
{
    if (data == NULL || data->progress == NULL || playerCount <= 0)
    {
        return NULL;
    }
    cJSON *json = cJSON_CreateObject();
    if (json == NULL)
    {
        return NULL;
    }
    cJSON *progressArray = cJSON_CreateIntArray(data->progress, playerCount);
    if (progressArray == NULL)
    {
        cJSON_Delete(json);
        return NULL;
    }
    cJSON_AddItemToObject(json, "progress", progressArray);
    char *jsonString = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return jsonString;
}

RefreshingData deserializeRefreshingData(const char *jsonString)
{
    RefreshingData data;
    data.progress = NULL;
    data.count = 0;
    if (jsonString == NULL)
    {
        return data;
    }
    cJSON *json = cJSON_Parse(jsonString);
    if (json == NULL)
    {
        return data;
    }
    cJSON *progressArray = cJSON_GetObjectItem(json, "progress");
    if (progressArray != NULL && cJSON_IsArray(progressArray))
    {
        int progressArraySize = cJSON_GetArraySize(progressArray);
        if (progressArraySize > 0)
        {
            int allocCount = (progressArraySize < 16) ? 16 : progressArraySize;
            data.progress = calloc(allocCount, sizeof(int));
            if (data.progress != NULL)
            {
                data.count = progressArraySize;
                for (int i = 0; i < progressArraySize; i++)
                {
                    cJSON *progressItem = cJSON_GetArrayItem(progressArray, i);
                    data.progress[i] = (progressItem && cJSON_IsNumber(progressItem)) ? progressItem->valueint : 0;
                }
            }
        }
    }
    cJSON_Delete(json);
    return data;
}

char *serializeSocketsData(const SocketsData *data, const int playerIndex)
{
    if (data == NULL || data->names == NULL || data->playerCount <= 0)
    {
        return NULL;
    }
    cJSON *json = cJSON_CreateObject();
    if (json == NULL)
    {
        return NULL;
    }
    cJSON *namesArray = cJSON_CreateStringArray((const char *const *)data->names, data->playerCount);
    if (namesArray == NULL)
    {
        cJSON_Delete(json);
        return NULL;
    }
    cJSON_AddItemToObject(json, "names", namesArray);
    cJSON_AddStringToObject(json, "sentence", data->sentence ? data->sentence : "");
    cJSON_AddNumberToObject(json, "playerCount", data->playerCount);
    cJSON_AddNumberToObject(json, "playerIndex", playerIndex);
    char *jsonString = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return jsonString;
}

SocketsData deserializeSocketsData(const char *jsonString)
{
    SocketsData data;
    data.names = NULL;
    data.sentence = NULL;
    data.playerCount = 0;
    data.playerIndex = 0;
    if (jsonString == NULL)
    {
        return data;
    }
    cJSON *json = cJSON_Parse(jsonString);
    if (json == NULL)
    {
        return data;
    }
    int namesArraySize = 0;
    cJSON *namesArray = cJSON_GetObjectItem(json, "names");
    if (namesArray != NULL && cJSON_IsArray(namesArray))
    {
        namesArraySize = cJSON_GetArraySize(namesArray);
    }
    cJSON *sentenceItem = cJSON_GetObjectItem(json, "sentence");
    if (sentenceItem != NULL && sentenceItem->valuestring != NULL)
    {
        data.sentence = strdup(sentenceItem->valuestring);
    }
    cJSON *playerCountItem = cJSON_GetObjectItem(json, "playerCount");
    int claimedPlayerCount = (playerCountItem && cJSON_IsNumber(playerCountItem)) ? playerCountItem->valueint : 0;
    int effectiveCount = (claimedPlayerCount > 0 && claimedPlayerCount < namesArraySize) ? claimedPlayerCount : namesArraySize;
    if (effectiveCount > 0 && namesArray != NULL)
    {
        data.names = malloc(effectiveCount * sizeof(char *));
        if (data.names != NULL)
        {
            for (int i = 0; i < effectiveCount; i++)
            {
                cJSON *nameItem = cJSON_GetArrayItem(namesArray, i);
                data.names[i] = (nameItem && nameItem->valuestring) ? strdup(nameItem->valuestring) : strdup("");
            }
        }
    }
    data.playerCount = (data.names != NULL) ? effectiveCount : 0;
    cJSON *playerIndexItem = cJSON_GetObjectItem(json, "playerIndex");
    int rawIdx = (playerIndexItem && cJSON_IsNumber(playerIndexItem)) ? playerIndexItem->valueint : 0;
    if (rawIdx < 0 || (data.playerCount > 0 && rawIdx >= data.playerCount))
    {
        rawIdx = 0;
    }
    data.playerIndex = rawIdx;
    cJSON_Delete(json);
    return data;
}

char *serializeTimes(const Times *data, int playerCount)
{
    if (data == NULL || data->times == NULL || playerCount <= 0)
    {
        return NULL;
    }
    cJSON *json = cJSON_CreateObject();
    if (json == NULL)
    {
        return NULL;
    }
    cJSON *timesArray = cJSON_CreateDoubleArray(data->times, playerCount);
    if (timesArray == NULL)
    {
        cJSON_Delete(json);
        return NULL;
    }
    cJSON_AddItemToObject(json, "times", timesArray);
    char *jsonString = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return jsonString;
}

Times deserializeTimes(const char *jsonString)
{
    Times data;
    data.times = NULL;
    data.count = 0;
    if (jsonString == NULL)
    {
        return data;
    }
    cJSON *json = cJSON_Parse(jsonString);
    if (json == NULL)
    {
        return data;
    }
    cJSON *timesArray = cJSON_GetObjectItem(json, "times");
    if (timesArray != NULL && cJSON_IsArray(timesArray))
    {
        int timesArraySize = cJSON_GetArraySize(timesArray);
        if (timesArraySize > 0)
        {
            int allocCount = (timesArraySize < 16) ? 16 : timesArraySize;
            data.times = malloc(allocCount * sizeof(double));
            if (data.times != NULL)
            {
                data.count = timesArraySize;
                for (int i = 0; i < allocCount; i++)
                {
                    data.times[i] = -1.0;
                }
                for (int i = 0; i < timesArraySize; i++)
                {
                    cJSON *timeItem = cJSON_GetArrayItem(timesArray, i);
                    data.times[i] = (timeItem && cJSON_IsNumber(timeItem)) ? timeItem->valuedouble : -1.0;
                }
            }
        }
    }
    cJSON_Delete(json);
    return data;
}