#pragma once

struct dynamic_buffer {
    char *data;
    size_t size;
    size_t capacity;
};

struct string_pool {
    char *pool;
    size_t used;
    size_t capacity;
    char **string_refs;
    size_t ref_count;
    size_t ref_capacity;
};

struct memory_arena {
    void *memory;
    size_t used;
    size_t capacity;
};

struct compiler_memory {
    string_pool strings;
    memory_arena ast_arena;
    memory_arena temp_arena;
    dynamic_buffer output_buffer;
};

// Initialize memory systems
bool init_compiler_memory();
void deinit_compiler_memory();

// String pool functions
char *pool_string(string_pool *pool, const char *str, size_t len);
char *pool_string_copy(string_pool *pool, const char *str);
void reset_string_pool(string_pool *pool);

// Memory arena functions
void *arena_alloc(memory_arena *arena, size_t size, size_t align);
void arena_reset(memory_arena *arena);

// Dynamic buffer functions
bool buffer_ensure_capacity(dynamic_buffer *buf, size_t needed);
bool buffer_append(dynamic_buffer *buf, const void *data, size_t size);
bool buffer_append_string(dynamic_buffer *buf, const char *str);
void buffer_reset(dynamic_buffer *buf);

extern compiler_memory *memory;
