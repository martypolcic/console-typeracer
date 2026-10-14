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
    data.progress = NULL;
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
            data.progress = malloc(progressArraySize * sizeof(int));
            if (data.progress != NULL)
            {
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
    cJSON *json = cJSON_CreateObject();
    cJSON *namesArray = cJSON_CreateStringArray((const char *const *)data->names, data->playerCount);
    cJSON_AddItemToObject(json, "names", namesArray);
    cJSON_AddStringToObject(json, "sentence", data->sentence ? data->sentence : "");
    cJSON_AddNumberToObject(json, "playerCount", data->playerCount);
    cJSON_AddNumberToObject(json, "playerIndex", playerIndex);
    char *jsonString = cJSON_Print(json);
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
    cJSON *namesArray = cJSON_GetObjectItem(json, "names");
    if (namesArray != NULL && cJSON_IsArray(namesArray))
    {
        int namesArraySize = cJSON_GetArraySize(namesArray);
        data.names = malloc(namesArraySize * sizeof(char *));
        if (data.names != NULL)
        {
            for (int i = 0; i < namesArraySize; i++)
            {
                cJSON *nameItem = cJSON_GetArrayItem(namesArray, i);
                data.names[i] = (nameItem && nameItem->valuestring) ? strdup(nameItem->valuestring) : strdup("");
            }
        }
    }
    cJSON *sentenceItem = cJSON_GetObjectItem(json, "sentence");
    if (sentenceItem != NULL && sentenceItem->valuestring != NULL)
    {
        data.sentence = strdup(sentenceItem->valuestring);
    }
    cJSON *playerCountItem = cJSON_GetObjectItem(json, "playerCount");
    data.playerCount = (playerCountItem && cJSON_IsNumber(playerCountItem)) ? playerCountItem->valueint : 0;
    cJSON *playerIndexItem = cJSON_GetObjectItem(json, "playerIndex");
    data.playerIndex = (playerIndexItem && cJSON_IsNumber(playerIndexItem)) ? playerIndexItem->valueint : 0;
    cJSON_Delete(json);
    return data;
}

int calculateRefreshingDataSize(int playerCount, int wordCount)
{
    int *progress = calloc(playerCount, sizeof(int));
    if (!progress)
    {
        return 0;
    }
    for (int i = 0; i < playerCount; i++)
    {
        progress[i] = wordCount;
    }

    RefreshingData data;
    data.progress = progress;

    char *jsonString = serializeRefreshingData(&data, playerCount);
    int size = jsonString ? (int)strlen(jsonString) : 0;
    free(progress);
    if (jsonString)
    {
        free(jsonString);
    }
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
    data.times = NULL;
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
            data.times = malloc(timesArraySize * sizeof(double));
            if (data.times != NULL)
            {
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