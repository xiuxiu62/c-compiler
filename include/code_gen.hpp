#pragma once

#include "ast.hpp"

// Target architectures
enum target_arch { ARCH_X86_64, ARCH_ARM64, ARCH_RISCV64 };

// Optimization levels
enum optimization_level { OPT_NONE, OPT_SIZE, OPT_SPEED, OPT_DEBUG };

// Variable storage info
struct variable_info {
    char *name;
    char *type;
    int stack_offset;
    int size;
    bool is_parameter;
    bool is_global;
};

// Function info
struct function_info {
    char *name;
    char *return_type;
    int stack_size;
    int param_count;
    bool is_main;
};

// Symbol table for code generation
struct symbol_table {
    struct variable_info *variables;
    int var_count;
    int var_capacity;

    struct function_info *functions;
    int func_count;
    int func_capacity;

    int current_stack_offset;
    int scope_level;
};

struct code_generator {
    char *output;
    int length;
    int capacity;

    // String literals
    char **strings;
    int string_count;
    int strings_capacity;

    // Symbol management
    struct symbol_table symbols;

    // Code generation state
    int label_counter;
    int temp_counter;
    bool in_function;
    char *current_function;

    // Target configuration
    enum target_arch arch;
    enum optimization_level opt_level;
    bool debug_info;

    // Assembly sections
    char *data_section;
    char *text_section;
    char *bss_section;

    // Error handling
    int error_count;
    char **errors;
};

// Core code generation functions
code_generator create_code_generator(enum target_arch arch, enum optimization_level opt);
void destroy_code_generator(code_generator *gen);

// String literal management
void add_string_literal(code_generator *gen, const char *str);
int find_string_index(code_generator *gen, const char *str);

// Symbol table management
void enter_scope(code_generator *gen);
void exit_scope(code_generator *gen);
void add_variable(code_generator *gen, const char *name, const char *type, int size, bool is_param);
void add_function(code_generator *gen, const char *name, const char *return_type);
struct variable_info *find_variable(code_generator *gen, const char *name);
struct function_info *find_function(code_generator *gen, const char *name);

// Label and temporary generation
char *generate_label(code_generator *gen, const char *prefix);
char *generate_temp(code_generator *gen);

// Assembly output helpers
void append_string(code_generator *gen, const char *str);
void append_instruction(code_generator *gen, const char *mnemonic, const char *operands);
void append_label(code_generator *gen, const char *label);
void append_comment(code_generator *gen, const char *comment);

// Node generation dispatch
void generate_node(code_generator *gen, ast_node *node);

// Declaration generation
void generate_program(code_generator *gen, ast_node *node);
void generate_function(code_generator *gen, ast_node *node);
void generate_variable_declaration(code_generator *gen, ast_node *node);
void generate_struct_declaration(code_generator *gen, ast_node *node);
void generate_enum_declaration(code_generator *gen, ast_node *node);

// Statement generation
void generate_block(code_generator *gen, ast_node *node);
void generate_if_statement(code_generator *gen, ast_node *node);
void generate_while_statement(code_generator *gen, ast_node *node);
void generate_for_statement(code_generator *gen, ast_node *node);
void generate_switch_statement(code_generator *gen, ast_node *node);
void generate_return_statement(code_generator *gen, ast_node *node);
void generate_break_statement(code_generator *gen, ast_node *node);
void generate_continue_statement(code_generator *gen, ast_node *node);

// Expression generation
void generate_binary_op(code_generator *gen, ast_node *node);
void generate_unary_op(code_generator *gen, ast_node *node);
void generate_assignment(code_generator *gen, ast_node *node);
void generate_function_call(code_generator *gen, ast_node *node);
void generate_array_access(code_generator *gen, ast_node *node);
void generate_member_access(code_generator *gen, ast_node *node);
void generate_ternary(code_generator *gen, ast_node *node);

// Literal generation
void generate_number_literal(code_generator *gen, ast_node *node);
void generate_float_literal(code_generator *gen, ast_node *node);
void generate_string_literal(code_generator *gen, ast_node *node);
void generate_char_literal(code_generator *gen, ast_node *node);
void generate_bool_literal(code_generator *gen, ast_node *node);
void generate_identifier(code_generator *gen, ast_node *node);

// Type system support
int get_type_size(const char *type);
const char *get_type_suffix(const char *type);
bool is_floating_type(const char *type);
bool is_signed_type(const char *type);

// Architecture-specific code generation
void generate_function_prologue(code_generator *gen, const char *func_name, int stack_size);
void generate_function_epilogue(code_generator *gen);
void generate_call_instruction(code_generator *gen, const char *func_name);
void generate_move_instruction(code_generator *gen, const char *src, const char *dst, int size);
void generate_syscall(code_generator *gen, int syscall_num);

// Built-in function support
void generate_printf(code_generator *gen, ast_node *node);
void generate_malloc(code_generator *gen, ast_node *node);
void generate_free(code_generator *gen, ast_node *node);

// Optimization passes
void optimize_redundant_moves(code_generator *gen);
void optimize_dead_code(code_generator *gen);
void peephole_optimize(code_generator *gen);

// Output generation
char *generate(code_generator *gen, ast_node *ast);
void output_data_section(code_generator *gen);
void output_text_section(code_generator *gen);
void output_bss_section(code_generator *gen);

// Debug and error handling
void codegen_error(code_generator *gen, const char *message);
void emit_debug_info(code_generator *gen, ast_node *node);
void print_symbol_table(code_generator *gen);

// Module system support
void generate_import(code_generator *gen, ast_node *node);
void generate_export(code_generator *gen, ast_node *node);
void generate_module_header(code_generator *gen, const char *module_name);
