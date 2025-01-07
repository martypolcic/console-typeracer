#include "jsonHandler.h"

char* serializeRefreshingData(const RefreshingData* data, int playerCount) {
    cJSON* json = cJSON_CreateObject();
    cJSON* progressArray = cJSON_CreateIntArray(data->progress, playerCount);
    cJSON_AddItemToObject(json, "progress", progressArray);
    char* jsonString = cJSON_Print(json);
    cJSON_Delete(json);
    return jsonString;
}

RefreshingData deserializeRefreshingData(const char* jsonString) {
    RefreshingData data;
    cJSON* json = cJSON_Parse(jsonString);
    cJSON* progressArray = cJSON_GetObjectItem(json, "progress");
    int progressArraySize = cJSON_GetArraySize(progressArray);
    data.progress = malloc(progressArraySize * sizeof(int));
    for (int i = 0; i < progressArraySize; i++) {
        cJSON* progressItem = cJSON_GetArrayItem(progressArray, i);
        data.progress[i] = progressItem->valueint;
    }
    cJSON_Delete(json);
    return data;
}

char* serializeSocketsData(const SocketsData* data) {
    cJSON* json = cJSON_CreateObject();
    cJSON* namesArray = cJSON_CreateStringArray((const char* const*)data->names, data->playerCount);
    cJSON_AddItemToObject(json, "names", namesArray);
    cJSON_AddStringToObject(json, "sentence", data->sentence);
    cJSON_AddNumberToObject(json, "wordsCount", data->wordsCount);
    cJSON_AddNumberToObject(json, "playerCount", data->playerCount);
    char* jsonString = cJSON_Print(json);
    cJSON_Delete(json);
    return jsonString;
}

SocketsData deserializeSocketsData(const char* jsonString) {
    SocketsData data;
    cJSON* json = cJSON_Parse(jsonString);
    cJSON* namesArray = cJSON_GetObjectItem(json, "names");
    if (namesArray != NULL) {
        int namesArraySize = cJSON_GetArraySize(namesArray);
        data.names = malloc(namesArraySize * sizeof(char*));
        for (int i = 0; i < namesArraySize; i++) {
            cJSON* nameItem = cJSON_GetArrayItem(namesArray, i);
            data.names[i] = strdup(nameItem->valuestring);
        }
    }
    cJSON* sentenceItem = cJSON_GetObjectItem(json, "sentence");
    if (sentenceItem != NULL) {
        data.sentence = strdup(sentenceItem->valuestring);
    }
    cJSON* wordsCountItem = cJSON_GetObjectItem(json, "wordsCount");
    data.wordsCount = wordsCountItem->valueint;
    cJSON* playerCountItem = cJSON_GetObjectItem(json, "playerCount");
    data.playerCount = playerCountItem->valueint;
    cJSON_Delete(json);
    return data;
}