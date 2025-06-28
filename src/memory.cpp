#include "defines.hpp"
#include "memory.hpp"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

// Alignment helpers
#define ALIGN_UP(n, align) (((n) + (align)-1) & ~((align)-1))
#define DEFAULT_ALIGN 8

// Initial sizes
#define INITIAL_STRING_POOL_SIZE (512 * 1024)    // 512KB for strings
#define INITIAL_AST_ARENA_SIZE (2 * 1024 * 1024) // 2MB for AST nodes
#define INITIAL_TEMP_ARENA_SIZE (256 * 1024)     // 256KB for temp allocations
#define INITIAL_OUTPUT_BUFFER_SIZE (64 * 1024)   // 64KB for code output
#define INITIAL_STRING_REFS 1024

static compiler_memory *memory = nullptr;

bool init_compiler_memory() {
    if (memory) {
        warn("Compiler memory already initialized");
        return false;
    }

    memset(memory, 0, sizeof(*memory));

    // Initialize string pool
    memory->strings.pool = (char *)malloc(INITIAL_STRING_POOL_SIZE);
    if (!memory->strings.pool) return false;
    memory->strings.capacity = INITIAL_STRING_POOL_SIZE;
    memory->strings.used = 0;

    memory->strings.string_refs = (char **)malloc(INITIAL_STRING_REFS * sizeof(char *));
    if (!memory->strings.string_refs) {
        free(memory->strings.pool);
        return false;
    }
    memory->strings.ref_capacity = INITIAL_STRING_REFS;
    memory->strings.ref_count = 0;

    // Initialize AST arena
    memory->ast_arena.memory = malloc(INITIAL_AST_ARENA_SIZE);
    if (!memory->ast_arena.memory) {
        free(memory->strings.pool);
        free(memory->strings.string_refs);
        return false;
    }
    memory->ast_arena.capacity = INITIAL_AST_ARENA_SIZE;
    memory->ast_arena.used = 0;

    // Initialize temp arena
    memory->temp_arena.memory = malloc(INITIAL_TEMP_ARENA_SIZE);
    if (!memory->temp_arena.memory) {
        free(memory->strings.pool);
        free(memory->strings.string_refs);
        free(memory->ast_arena.memory);
        return false;
    }
    memory->temp_arena.capacity = INITIAL_TEMP_ARENA_SIZE;
    memory->temp_arena.used = 0;

    // Initialize output buffer
    memory->output_buffer.data = (char *)malloc(INITIAL_OUTPUT_BUFFER_SIZE);
    if (!memory->output_buffer.data) {
        free(memory->strings.pool);
        free(memory->strings.string_refs);
        free(memory->ast_arena.memory);
        free(memory->temp_arena.memory);
        return false;
    }
    memory->output_buffer.capacity = INITIAL_OUTPUT_BUFFER_SIZE;
    memory->output_buffer.size = 0;

    return true;
}

void cleanup_compiler_memory() {
    if (!memory) {
        warn("Compiler memory not initialized");
        return;
    }

    free(memory->strings.pool);
    free(memory->strings.string_refs);
    free(memory->ast_arena.memory);
    free(memory->temp_arena.memory);
    free(memory->output_buffer.data);
    memset(memory, 0, sizeof(*memory));
}

char *pool_string(string_pool *pool, const char *str, size_t len) {
    if (!str || len == 0) return NULL;

    // Check if we need to grow the pool
    size_t aligned_len = ALIGN_UP(len + 1, DEFAULT_ALIGN);
    if (pool->used + aligned_len > pool->capacity) {
        size_t new_capacity = pool->capacity * 2;
        while (new_capacity < pool->used + aligned_len) {
            new_capacity *= 2;
        }

        char *new_pool = (char *)realloc(pool->pool, new_capacity);
        if (!new_pool) return NULL;

        // Update all existing references
        ptrdiff_t offset = new_pool - pool->pool;
        if (offset != 0) {
            for (size_t i = 0; i < pool->ref_count; i++) {
                pool->string_refs[i] += offset;
            }
        }

        pool->pool = new_pool;
        pool->capacity = new_capacity;
    }

    // Check if we need to grow the references array
    if (pool->ref_count >= pool->ref_capacity) {
        size_t new_ref_capacity = pool->ref_capacity * 2;
        char **new_refs = (char **)realloc(pool->string_refs, new_ref_capacity * sizeof(char *));
        if (!new_refs) return NULL;
        pool->string_refs = new_refs;
        pool->ref_capacity = new_ref_capacity;
    }

    char *result = pool->pool + pool->used;
    memcpy(result, str, len);
    result[len] = '\0';

    pool->string_refs[pool->ref_count++] = result;
    pool->used += aligned_len;

    return result;
}

char *pool_string_copy(string_pool *pool, const char *str) {
    return str ? pool_string(pool, str, strlen(str)) : nullptr;
}

void reset_string_pool(string_pool *pool) {
    pool->used = 0;
    pool->ref_count = 0;
}

void *arena_alloc(memory_arena *arena, size_t size, size_t align) {
    if (align == 0) align = DEFAULT_ALIGN;

    size_t aligned_used = ALIGN_UP(arena->used, align);
    if (aligned_used + size > arena->capacity) {
        size_t new_capacity = arena->capacity * 2;
        while (new_capacity < aligned_used + size) {
            new_capacity *= 2;
        }

        void *new_memory = realloc(arena->memory, new_capacity);
        if (!new_memory) return NULL;

        arena->memory = new_memory;
        arena->capacity = new_capacity;
    }

    void *result = (char *)arena->memory + aligned_used;
    arena->used = aligned_used + size;
    return result;
}

void arena_reset(memory_arena *arena) {
    arena->used = 0;
}

bool buffer_ensure_capacity(dynamic_buffer *buf, size_t needed) {
    if (buf->capacity >= needed) return true;

    size_t new_capacity = buf->capacity;
    if (new_capacity == 0) new_capacity = 1024;

    while (new_capacity < needed) {
        new_capacity *= 2;
    }

    char *new_data = (char *)realloc(buf->data, new_capacity);
    if (!new_data) return false;

    buf->data = new_data;
    buf->capacity = new_capacity;
    return true;
}

bool buffer_append(dynamic_buffer *buf, const void *data, size_t size) {
    if (!buffer_ensure_capacity(buf, buf->size + size)) {
        return false;
    }

    memcpy(buf->data + buf->size, data, size);
    buf->size += size;
    return true;
}

bool buffer_append_string(dynamic_buffer *buf, const char *str) {
    return str ? buffer_append(buf, str, strlen(str)) : true;
}

void buffer_reset(dynamic_buffer *buf) {
    buf->size = 0;
}
