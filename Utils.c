#include "Utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define _CRT_SECURE_NO_WARNINGS

char* sliceCharStr(char string[], int start, int end)
{
    int len = end - start;
    char* result = (char*)malloc(len + 1);

    if (result) {
        strncpy(result, string + start, len);
        result[len] = '\0';
    }

    return result;
}

char** splitCharStr(const char* string, char splitter) {
    char* strCopy = _strdup(string);
    if (!strCopy) return NULL;

    char** result = malloc(10 * sizeof(char*));
    if (!result) { free(strCopy); return NULL; }

    char delim[2] = { splitter, '\0' };
    char* token = strtok(strCopy, delim);
    int row = 0;

    while (token != NULL && row < 10) {
        result[row] = _strdup(token);
        row++;
        token = strtok(NULL, delim);
    }

    if (row < 10) result[row] = NULL;

    free(strCopy);
    return result;
}