#include "cjson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *duplicate_string(const char *input) {
    if (!input) {
        return NULL;
    }
    size_t len = strlen(input);
    char *copy = (char *)malloc(len + 1);
    if (copy) {
        memcpy(copy, input, len + 1);
    }
    return copy;
}

cJSON *cJSON_CreateObject(void) {
    cJSON *object = (cJSON *)calloc(1, sizeof(cJSON));
    return object;
}

void cJSON_Delete(cJSON *item) {
    while (item) {
        cJSON *next = item->next;
        free(item->key);
        free(item->valuestring);
        free(item);
        item = next;
    }
}

static char *escape_json_string(const char *input) {
    size_t extra = 0;
    for (const char *c = input; *c; ++c) {
        if (*c == '\\' || *c == '"' || *c == '\n' || *c == '\r' || *c == '\t') {
            extra++;
        }
    }

    size_t len = strlen(input);
    char *output = (char *)malloc(len + extra + 1);
    if (!output) {
        return NULL;
    }

    char *out = output;
    for (const char *c = input; *c; ++c) {
        switch (*c) {
            case '\\':
                *out++ = '\\';
                *out++ = '\\';
                break;
            case '"':
                *out++ = '\\';
                *out++ = '"';
                break;
            case '\n':
                *out++ = '\\';
                *out++ = 'n';
                break;
            case '\r':
                *out++ = '\\';
                *out++ = 'r';
                break;
            case '\t':
                *out++ = '\\';
                *out++ = 't';
                break;
            default:
                *out++ = *c;
                break;
        }
    }
    *out = '\0';
    return output;
}

cJSON *cJSON_AddStringToObject(cJSON *object, const char *name, const char *value) {
    if (!object || !name || !value) {
        return NULL;
    }

    cJSON *node = (cJSON *)calloc(1, sizeof(cJSON));
    if (!node) {
        return NULL;
    }

    node->key = duplicate_string(name);
    node->valuestring = duplicate_string(value);
    if (!node->key || !node->valuestring) {
        free(node->key);
        free(node->valuestring);
        free(node);
        return NULL;
    }

    /* append at end of list */
    cJSON *cursor = object;
    while (cursor->next) {
        cursor = cursor->next;
    }
    cursor->next = node;
    return node;
}

char *cJSON_PrintUnformatted(const cJSON *item) {
    if (!item) {
        return NULL;
    }

    /* count properties */
    size_t property_count = 0;
    for (const cJSON *cursor = item->next; cursor; cursor = cursor->next) {
        property_count++;
    }

    char **keys = NULL;
    char **values = NULL;
    if (property_count > 0) {
        keys = (char **)calloc(property_count, sizeof(char *));
        values = (char **)calloc(property_count, sizeof(char *));
        if (!keys || !values) {
            free(keys);
            free(values);
            return NULL;
        }
    }

    size_t index = 0;
    size_t total_length = 2; /* braces */
    for (const cJSON *cursor = item->next; cursor; cursor = cursor->next, ++index) {
        keys[index] = escape_json_string(cursor->key);
        values[index] = escape_json_string(cursor->valuestring);
        if (!keys[index] || !values[index]) {
            for (size_t j = 0; j <= index; ++j) {
                free(keys[j]);
                free(values[j]);
            }
            free(keys);
            free(values);
            return NULL;
        }

        total_length += strlen(keys[index]) + strlen(values[index]) + 5; /* quotes and colon */
        if (cursor->next) {
            total_length += 1; /* comma */
        }
    }

    char *output = (char *)malloc(total_length + 1);
    if (!output) {
        for (size_t j = 0; j < property_count; ++j) {
            free(keys[j]);
            free(values[j]);
        }
        free(keys);
        free(values);
        return NULL;
    }

    char *out = output;
    *out++ = '{';

    for (size_t i = 0; i < property_count; ++i) {
        *out++ = '"';
        size_t key_len = strlen(keys[i]);
        memcpy(out, keys[i], key_len);
        out += key_len;
        *out++ = '"';
        *out++ = ':';
        *out++ = '"';
        size_t value_len = strlen(values[i]);
        memcpy(out, values[i], value_len);
        out += value_len;
        *out++ = '"';
        if (i + 1 < property_count) {
            *out++ = ',';
        }
        free(keys[i]);
        free(values[i]);
    }

    *out++ = '}';
    *out = '\0';

    free(keys);
    free(values);
    return output;
}
