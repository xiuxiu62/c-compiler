#pragma once

#include "code_gen.hpp"

// Compilation options
struct compile_options {
    char **input_files;
    int input_count;
    char *output_file;
    enum target_arch arch;
    enum optimization_level opt_level;
    bool debug_info;
    bool verbose;
    bool compile_only;   // -c flag
    bool link_only;      // -l flag
    bool create_library; // -lib flag
    bool print_ast;
    bool print_tokens;
    bool keep_asm;
    char *lib_paths[16];
    int lib_path_count;
    char *libraries[16];
    int library_count;
};

// Forward declarations
void print_usage(const char *program_name);
void print_version(void);
struct compile_options parse_arguments(int argc, char *argv[]);
char *read_file(const char *filename);
bool file_exists(const char *filename);
char *get_file_extension(const char *filename);
char *change_extension(const char *filename, const char *new_ext);
bool compile_file(const char *input_file, struct compile_options *opts);
bool assemble_file(const char *asm_file, const char *obj_file);
bool link_files(char **obj_files, int obj_count, const char *output_file, struct compile_options *opts);
bool create_static_library(char **obj_files, int obj_count, const char *lib_file);
void cleanup_temp_files(char **temp_files, int count);
