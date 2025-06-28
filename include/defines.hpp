#pragma once

#include "stdio.h"
#include "stdlib.h"

#define info(fmt, ...) fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__);

#define warn(fmt, ...) fprintf(stdout, "[WARN] " fmt "\n", ##__VA_ARGS__);

#define error(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__);

#define fatal(fmt, ...)                                                                                                \
    do {                                                                                                               \
        fprintf(stderr, "[FATAL] " fmt "\n", ##__VA_ARGS__);                                                           \
        abort();                                                                                                       \
    } while (0)
