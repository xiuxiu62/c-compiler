#include "compiler.hpp"
#include "ast.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/stat.h"
#include "unistd.h"

void print_usage(const char *program_name) {
    printf("Usage: %s [options] <input-files>\n\n", program_name);
    printf("📋 Options:\n");
    printf("  -o <file>         Output file name\n");
    printf("  -c                Compile only (don't link)\n");
    printf("  -lib              Create static library (.a)\n");
    printf("  -O0, -O1, -O2, -Os Optimization level (0=none, 1=speed, 2=more speed, s=size)\n");
    printf("  -g                Include debug information\n");
    printf("  -v, --verbose     Verbose output\n");
    printf("  -S                Keep assembly files\n");
    printf("  --print-ast       Print Abstract Syntax Tree\n");
    printf("  --print-tokens    Print token stream\n");
    printf("  --target <arch>   Target architecture (x86_64, arm64, riscv64)\n");
    printf("  -L <path>         Add library search path\n");
    printf("  -l <library>      Link with library\n");
    printf("  -h, --help        Show this help\n");
    printf("  --version         Show version information\n\n");
    printf("📂 Examples:\n");
    printf("  %s main.c                    # Compile to a.out\n", program_name);
    printf("  %s -o myapp main.c util.c    # Compile multiple files\n", program_name);
    printf("  %s -c main.c                 # Compile to object file only\n", program_name);
    printf("  %s -lib -o libutil.a util.c  # Create static library\n", program_name);
    printf("  %s -O2 -g main.c             # Optimized build with debug info\n", program_name);
}

void print_version(void) {
    printf("Modern C Compiler v1.0\n");
    printf("Built with enhanced lexer, parser, and code generator\n");
    printf("Supports: i8, i16, i32, i64, u8, u16, u32, u64, f32, f64, bool\n");
    printf("Target architectures: x86_64, ARM64, RISC-V64\n");
}

struct compile_options parse_arguments(int argc, char *argv[]) {
    struct compile_options opts = {0};

    // Initialize defaults
    opts.arch = ARCH_X86_64;
    opts.opt_level = OPT_NONE;
    opts.debug_info = false;
    opts.verbose = false;
    opts.compile_only = false;
    opts.create_library = false;
    opts.print_ast = false;
    opts.print_tokens = false;
    opts.keep_asm = false;
    opts.input_files = (char **)malloc(argc * sizeof(char *));
    opts.input_count = 0;
    opts.lib_path_count = 0;
    opts.library_count = 0;

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(arg, "--version") == 0) {
            print_version();
            exit(0);
        } else if (strcmp(arg, "-o") == 0) {
            if (++i < argc) {
                opts.output_file = strdup(argv[i]);
            }
        } else if (strcmp(arg, "-c") == 0) {
            opts.compile_only = true;
        } else if (strcmp(arg, "-lib") == 0) {
            opts.create_library = true;
        } else if (strcmp(arg, "-O0") == 0) {
            opts.opt_level = OPT_NONE;
        } else if (strcmp(arg, "-O1") == 0) {
            opts.opt_level = OPT_SPEED;
        } else if (strcmp(arg, "-O2") == 0) {
            opts.opt_level = OPT_SPEED;
        } else if (strcmp(arg, "-Os") == 0) {
            opts.opt_level = OPT_SIZE;
        } else if (strcmp(arg, "-g") == 0) {
            opts.debug_info = true;
            opts.opt_level = OPT_DEBUG;
        } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            opts.verbose = true;
        } else if (strcmp(arg, "-S") == 0) {
            opts.keep_asm = true;
        } else if (strcmp(arg, "--print-ast") == 0) {
            opts.print_ast = true;
        } else if (strcmp(arg, "--print-tokens") == 0) {
            opts.print_tokens = true;
        } else if (strcmp(arg, "--target") == 0) {
            if (++i < argc) {
                if (strcmp(argv[i], "x86_64") == 0) {
                    opts.arch = ARCH_X86_64;
                } else if (strcmp(argv[i], "arm64") == 0) {
                    opts.arch = ARCH_ARM64;
                } else if (strcmp(argv[i], "riscv64") == 0) {
                    opts.arch = ARCH_RISCV64;
                } else {
                    fprintf(stderr, "❌ Unknown target architecture: %s\n", argv[i]);
                    exit(1);
                }
            }
        } else if (strcmp(arg, "-L") == 0) {
            if (++i < argc && opts.lib_path_count < 16) {
                opts.lib_paths[opts.lib_path_count++] = argv[i];
            }
        } else if (strncmp(arg, "-l", 2) == 0) {
            if (opts.library_count < 16) {
                opts.libraries[opts.library_count++] = arg + 2;
            }
        } else if (arg[0] != '-') {
            // Input file
            opts.input_files[opts.input_count++] = strdup(arg);
        } else {
            fprintf(stderr, "❌ Unknown option: %s\n", arg);
            exit(1);
        }
    }

    return opts;
}

char *read_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = (char *)malloc(size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(content, 1, size, file);
    content[read_size] = '\0';

    fclose(file);
    return content;
}

bool file_exists(const char *filename) {
    struct stat st;
    return stat(filename, &st) == 0;
}

char *get_file_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    return dot ? strdup(dot) : strdup("");
}

char *change_extension(const char *filename, const char *new_ext) {
    const char *dot = strrchr(filename, '.');
    size_t base_len = dot ? (size_t)(dot - filename) : strlen(filename);

    char *new_filename = (char *)malloc(base_len + strlen(new_ext) + 1);
    if (!new_filename) {
        return NULL;
    }

    strncpy(new_filename, filename, base_len);
    strcpy(new_filename + base_len, new_ext);

    return new_filename;
}

bool compile_file(const char *input_file, struct compile_options *opts) {
    // Check if input file exists
    if (!file_exists(input_file)) {
        fprintf(stderr, "❌ Error: File not found: %s\n", input_file);
        return false;
    }

    // Read source file
    char *source = read_file(input_file);
    if (!source) {
        fprintf(stderr, "❌ Error: Cannot read file %s\n", input_file);
        return false;
    }

    if (opts->verbose) {
        printf("📖 Read %zu bytes from %s\n", strlen(source), input_file);
    }

    // Lexical analysis
    struct lexer lexer = create_lexer(source);

    if (opts->print_tokens) {
        printf("🔤 Tokens for %s:\n", input_file);
        struct lexer temp_lexer = lexer;
        while (true) {
            struct token token = next_token(&temp_lexer);
            if (token.type == TOKEN_EOF) {
                free_token(&token);
                break;
            }
            printf("  %s: '%s'\n", token_type_name(token.type), token.value);
            free_token(&token);
        }
        printf("\n");
    }

    // Reset lexer for parsing
    lexer = create_lexer(source);

    // Parsing
    struct parser parser = create_parser(&lexer);
    ast_node *ast = parse(&parser);

    if (!ast || parser.error_count > 0) {
        fprintf(stderr, "❌ Parse error in %s (%d errors)\n", input_file, parser.error_count);
        free(source);
        free_token(&parser.current_token);
        return false;
    }

    if (opts->print_ast) {
        printf("🌳 AST for %s:\n", input_file);
        print_ast(ast, 0);
        printf("\n");
    }

    // Code generation
    code_generator codegen = create_code_generator(opts->arch, opts->opt_level);
    char *assembly = generate(&codegen, ast);

    if (codegen.error_count > 0) {
        fprintf(stderr, "❌ Code generation error in %s (%d errors)\n", input_file, codegen.error_count);
        destroy_code_generator(&codegen);
        free_node(ast);
        free(source);
        free_token(&parser.current_token);
        return false;
    }

    // Write assembly file
    char *asm_file = change_extension(input_file, ".s");
    if (!asm_file) {
        fprintf(stderr, "❌ Error: Memory allocation failed\n");
        destroy_code_generator(&codegen);
        free_node(ast);
        free(source);
        free_token(&parser.current_token);
        return false;
    }

    FILE *output_file = fopen(asm_file, "w");
    if (!output_file) {
        fprintf(stderr, "❌ Error: Cannot write to %s\n", asm_file);
        free(asm_file);
        destroy_code_generator(&codegen);
        free_node(ast);
        free(source);
        free_token(&parser.current_token);
        return false;
    }

    fprintf(output_file, "%s", assembly);
    fclose(output_file);

    if (opts->verbose) {
        printf("📝 Generated assembly: %s\n", asm_file);
    }

    // Assemble to object file
    char *obj_file = change_extension(input_file, ".o");
    bool assemble_success = false;

    if (obj_file) {
        assemble_success = assemble_file(asm_file, obj_file);

        if (assemble_success && opts->verbose) {
            printf("🔧 Generated object file: %s\n", obj_file);
        }
    }

    // Cleanup
    free(asm_file);
    free(obj_file);
    destroy_code_generator(&codegen);
    free_node(ast);
    free(source);
    free_token(&parser.current_token);

    return assemble_success;
}

bool assemble_file(const char *asm_file, const char *obj_file) {
    char command[1024];
    snprintf(command, sizeof(command), "as -64 %s -o %s 2>/dev/null", asm_file, obj_file);

    int result = system(command);
    if (result != 0) {
        fprintf(stderr, "❌ Assembly failed for %s\n", asm_file);
        return false;
    }

    return true;
}

bool link_files(char **obj_files, int obj_count, const char *output_file, struct compile_options *opts) {
    if (obj_count == 0) {
        fprintf(stderr, "❌ No object files to link\n");
        return false;
    }

    char command[4096];
    int len = snprintf(command, sizeof(command), "ld");

    // Add object files
    for (int i = 0; i < obj_count; i++) {
        if (len < sizeof(command) - 100) {
            len += snprintf(command + len, sizeof(command) - len, " %s", obj_files[i]);
        }
    }

    // Add library paths
    for (int i = 0; i < opts->lib_path_count; i++) {
        if (len < sizeof(command) - 100) {
            len += snprintf(command + len, sizeof(command) - len, " -L%s", opts->lib_paths[i]);
        }
    }

    // Add libraries
    for (int i = 0; i < opts->library_count; i++) {
        if (len < sizeof(command) - 100) {
            len += snprintf(command + len, sizeof(command) - len, " -l%s", opts->libraries[i]);
        }
    }

    // Add output file
    if (len < sizeof(command) - 100) {
        len += snprintf(command + len, sizeof(command) - len, " -o %s", output_file);
    }

    // Suppress linker warnings for basic functionality
    len += snprintf(command + len, sizeof(command) - len, " 2>/dev/null");

    if (opts->verbose) {
        printf("🔗 Link command: %s\n", command);
    }

    int result = system(command);
    if (result != 0) {
        fprintf(stderr, "❌ Linking failed\n");
        return false;
    }

    return true;
}

bool create_static_library(char **obj_files, int obj_count, const char *lib_file) {
    if (obj_count == 0) {
        fprintf(stderr, "❌ No object files for library\n");
        return false;
    }

    char command[4096];
    int len = snprintf(command, sizeof(command), "ar rcs %s", lib_file);

    for (int i = 0; i < obj_count; i++) {
        if (len < sizeof(command) - 100) {
            len += snprintf(command + len, sizeof(command) - len, " %s", obj_files[i]);
        }
    }

    int result = system(command);
    if (result != 0) {
        fprintf(stderr, "❌ Library creation failed\n");
        return false;
    }

    return true;
}

void cleanup_temp_files(char **temp_files, int count) {
    for (int i = 0; i < count; i++) {
        if (temp_files[i] && file_exists(temp_files[i])) {
            if (unlink(temp_files[i]) != 0) {
                // Silently ignore cleanup failures
            }
        }
    }
}
