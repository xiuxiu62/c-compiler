#include "compiler.hpp"
#include "stdio.h"
#include "stdlib.h"

int main(int argc, char *argv[]) {
    printf("🔧 Modern C Compiler v1.0\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    struct compile_options opts = parse_arguments(argc, argv);

    if (opts.input_count == 0) {
        fprintf(stderr, "❌ Error: No input files specified\n");
        return 1;
    }

    // Display compilation info
    printf("📁 Input files: %d\n", opts.input_count);
    for (int i = 0; i < opts.input_count; i++) {
        printf("   • %s\n", opts.input_files[i]);
    }

    if (opts.output_file) {
        printf("📦 Output: %s\n", opts.output_file);
    }

    printf("🎯 Target: %s, Optimization: %s\n",
           opts.arch == ARCH_X86_64  ? "x86_64"
           : opts.arch == ARCH_ARM64 ? "arm64"
                                     : "riscv64",
           opts.opt_level == OPT_NONE    ? "none"
           : opts.opt_level == OPT_SIZE  ? "size"
           : opts.opt_level == OPT_SPEED ? "speed"
                                         : "debug");

    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    // Allocate arrays for file management
    char **obj_files = (char **)malloc(opts.input_count * sizeof(char *));
    char **temp_files = (char **)malloc(opts.input_count * 2 * sizeof(char *));
    int obj_count = 0;
    int temp_count = 0;
    bool success = true;

    // Compile each input file
    for (int i = 0; i < opts.input_count; i++) {
        printf("🔨 Compiling %s...\n", opts.input_files[i]);

        if (!compile_file(opts.input_files[i], &opts)) {
            fprintf(stderr, "❌ Failed to compile %s\n", opts.input_files[i]);
            success = false;
            break;
        }

        // Generate object file name and track it
        char *obj_file = change_extension(opts.input_files[i], ".o");
        if (obj_file) {
            obj_files[obj_count++] = obj_file;
            temp_files[temp_count++] = obj_file;
        }

        // Track assembly file for cleanup if not keeping
        if (!opts.keep_asm) {
            char *asm_file = change_extension(opts.input_files[i], ".s");
            if (asm_file) {
                temp_files[temp_count++] = asm_file;
            }
        }

        printf("✅ Generated %s\n", obj_file ? obj_file : "object file");
    }

    // Link or create library if compilation succeeded and not compile-only
    if (success && !opts.compile_only) {
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

        if (opts.create_library) {
            // Create static library
            const char *lib_file = opts.output_file ? opts.output_file : "liboutput.a";
            printf("📚 Creating library %s...\n", lib_file);

            if (create_static_library(obj_files, obj_count, lib_file)) {
                printf("✅ Library created successfully!\n");
            } else {
                fprintf(stderr, "❌ Failed to create library\n");
                success = false;
            }
        } else {
            // Link executable
            const char *exe_file = opts.output_file ? opts.output_file : "a.out";
            printf("🔗 Linking %s...\n", exe_file);

            if (link_files(obj_files, obj_count, exe_file, &opts)) {
                printf("✅ Executable created successfully!\n");
                printf("🚀 Run with: ./%s\n", exe_file);
            } else {
                fprintf(stderr, "❌ Failed to link executable\n");
                success = false;
            }
        }
    }

    // Cleanup temporary files if not in compile-only mode and not keeping assembly
    if (!opts.compile_only && !opts.keep_asm) {
        cleanup_temp_files(temp_files, temp_count);
    }

    // Free allocated memory
    for (int i = 0; i < obj_count; i++) {
        free(obj_files[i]);
    }
    free(obj_files);

    for (int i = 0; i < temp_count; i++) {
        free(temp_files[i]);
    }
    free(temp_files);

    for (int i = 0; i < opts.input_count; i++) {
        free(opts.input_files[i]);
    }
    free(opts.input_files);

    if (opts.output_file) {
        free(opts.output_file);
    }

    // Final status
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    if (success) {
        printf("🎉 Compilation completed successfully!\n");
        return 0;
    } else {
        printf("💥 Compilation failed!\n");
        return 1;
    }
}
