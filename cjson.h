#ifndef CJSON_H
#define CJSON_H

#include <stddef.h>

typedef struct cJSON {
    char *key;
    char *valuestring;
    struct cJSON *next;
} cJSON;

cJSON *cJSON_CreateObject(void);
void cJSON_Delete(cJSON *item);
cJSON *cJSON_AddStringToObject(cJSON *object, const char *name, const char *value);
char *cJSON_PrintUnformatted(const cJSON *item);

#endif // CJSON_H
