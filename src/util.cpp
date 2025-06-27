#include "util.hpp"
#include "stdlib.h"
#include "string.h"

char *string_duplicate(const char *str, int len) {
    char *result = (char *)malloc(len + 1);
    if (!result) return NULL;
    strncpy(result, str, len);
    result[len] = '\0';
    return result;
}
