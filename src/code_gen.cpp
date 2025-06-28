#include "ast.hpp"
#include "code_gen.hpp"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "util.hpp"

// Core code generation functions
code_generator create_code_generator(enum target_arch arch, enum optimization_level opt) {
    code_generator gen;

    // Initialize output buffer
    gen.capacity = 64 * 1024;
    gen.output = (char *)malloc(gen.capacity);
    gen.length = 0;
    gen.output[0] = '\0';

    // Initialize string literals
    gen.strings = (char **)malloc(sizeof(char *) * 16);
    gen.string_count = 0;
    gen.strings_capacity = 16;

    // Initialize symbol table
    gen.symbols.variables = (struct variable_info *)malloc(sizeof(struct variable_info) * 32);
    gen.symbols.var_count = 0;
    gen.symbols.var_capacity = 32;
    gen.symbols.functions = (struct function_info *)malloc(sizeof(struct function_info) * 16);
    gen.symbols.func_count = 0;
    gen.symbols.func_capacity = 16;
    gen.symbols.current_stack_offset = 0;
    gen.symbols.scope_level = 0;

    // Initialize state
    gen.label_counter = 0;
    gen.temp_counter = 0;
    gen.in_function = false;
    gen.current_function = NULL;

    // Set target configuration
    gen.arch = arch;
    gen.opt_level = opt;
    gen.debug_info = (opt == OPT_DEBUG);

    // Initialize sections
    gen.data_section = (char *)malloc(16 * 1024);
    gen.text_section = (char *)malloc(32 * 1024);
    gen.bss_section = (char *)malloc(8 * 1024);
    gen.data_section[0] = '\0';
    gen.text_section[0] = '\0';
    gen.bss_section[0] = '\0';

    // Initialize error handling
    gen.error_count = 0;
    gen.errors = (char **)malloc(sizeof(char *) * 16);

    return gen;
}

void destroy_code_generator(code_generator *gen) {
    free(gen->output);

    // Free string literals
    for (int i = 0; i < gen->string_count; i++) {
        free(gen->strings[i]);
    }
    free(gen->strings);

    // Free symbol table
    for (int i = 0; i < gen->symbols.var_count; i++) {
        free(gen->symbols.variables[i].name);
        free(gen->symbols.variables[i].type);
    }
    free(gen->symbols.variables);

    for (int i = 0; i < gen->symbols.func_count; i++) {
        free(gen->symbols.functions[i].name);
        free(gen->symbols.functions[i].return_type);
    }
    free(gen->symbols.functions);

    // Free sections
    free(gen->data_section);
    free(gen->text_section);
    free(gen->bss_section);

    // Free errors
    for (int i = 0; i < gen->error_count; i++) {
        free(gen->errors[i]);
    }
    free(gen->errors);

    free(gen->current_function);
}

// Assembly output helpers
void append_string(code_generator *gen, const char *str) {
    int str_len = strlen(str);
    while (gen->length + str_len >= gen->capacity) {
        gen->capacity *= 2;
        gen->output = (char *)realloc(gen->output, gen->capacity);
    }
    strcpy(gen->output + gen->length, str);
    gen->length += str_len;
}

void append_instruction(code_generator *gen, const char *mnemonic, const char *operands) {
    append_string(gen, "    ");
    append_string(gen, mnemonic);
    if (operands && strlen(operands) > 0) {
        append_string(gen, " ");
        append_string(gen, operands);
    }
    append_string(gen, "\n");
}

void append_label(code_generator *gen, const char *label) {
    append_string(gen, label);
    append_string(gen, ":\n");
}

void append_comment(code_generator *gen, const char *comment) {
    if (gen->debug_info) {
        append_string(gen, "    # ");
        append_string(gen, comment);
        append_string(gen, "\n");
    }
}

// String literal management
void add_string_literal(code_generator *gen, const char *str) {
    if (gen->string_count >= gen->strings_capacity) {
        gen->strings_capacity *= 2;
        gen->strings = (char **)realloc(gen->strings, sizeof(char *) * gen->strings_capacity);
    }
    gen->strings[gen->string_count] = string_duplicate(str, strlen(str));
    gen->string_count++;
}

int find_string_index(code_generator *gen, const char *str) {
    for (int i = 0; i < gen->string_count; i++) {
        if (strcmp(gen->strings[i], str) == 0) {
            return i;
        }
    }
    add_string_literal(gen, str);
    return gen->string_count - 1;
}

// Symbol table management
void enter_scope(code_generator *gen) {
    gen->symbols.scope_level++;
}

void exit_scope(code_generator *gen) {
    // Remove variables from current scope
    int i = gen->symbols.var_count - 1;
    while (i >= 0 && gen->symbols.variables[i].size == gen->symbols.scope_level) {
        free(gen->symbols.variables[i].name);
        free(gen->symbols.variables[i].type);
        i--;
        gen->symbols.var_count--;
    }
    gen->symbols.scope_level--;
}

void add_variable(code_generator *gen, const char *name, const char *type, int size, bool is_param) {
    if (gen->symbols.var_count >= gen->symbols.var_capacity) {
        gen->symbols.var_capacity *= 2;
        gen->symbols.variables = (struct variable_info *)realloc(gen->symbols.variables, sizeof(struct variable_info) *
                                                                                             gen->symbols.var_capacity);
    }

    struct variable_info *var = &gen->symbols.variables[gen->symbols.var_count];
    var->name = string_duplicate(name, strlen(name));
    var->type = string_duplicate(type, strlen(type));
    var->size = size;
    var->is_parameter = is_param;
    var->is_global = (gen->symbols.scope_level == 0);

    if (is_param) {
        // Parameters are at positive offsets from rbp
        var->stack_offset = 16 + (gen->symbols.var_count * 8); // Simplified
    } else {
        // Local variables are at negative offsets from rbp
        gen->symbols.current_stack_offset -= size;
        var->stack_offset = gen->symbols.current_stack_offset;
    }

    gen->symbols.var_count++;
}

void add_function(code_generator *gen, const char *name, const char *return_type) {
    if (gen->symbols.func_count >= gen->symbols.func_capacity) {
        gen->symbols.func_capacity *= 2;
        gen->symbols.functions = (struct function_info *)realloc(
            gen->symbols.functions, sizeof(struct function_info) * gen->symbols.func_capacity);
    }

    struct function_info *func = &gen->symbols.functions[gen->symbols.func_count];
    func->name = string_duplicate(name, strlen(name));
    func->return_type = string_duplicate(return_type, strlen(return_type));
    func->stack_size = 0;
    func->param_count = 0;
    func->is_main = (strcmp(name, "main") == 0);

    gen->symbols.func_count++;
}

struct variable_info *find_variable(code_generator *gen, const char *name) {
    for (int i = gen->symbols.var_count - 1; i >= 0; i--) {
        if (strcmp(gen->symbols.variables[i].name, name) == 0) {
            return &gen->symbols.variables[i];
        }
    }
    return NULL;
}

struct function_info *find_function(code_generator *gen, const char *name) {
    for (int i = 0; i < gen->symbols.func_count; i++) {
        if (strcmp(gen->symbols.functions[i].name, name) == 0) {
            return &gen->symbols.functions[i];
        }
    }
    return NULL;
}

// Label and temporary generation
char *generate_label(code_generator *gen, const char *prefix) {
    char *label = (char *)malloc(64);
    snprintf(label, 64, "%s%d", prefix, gen->label_counter++);
    return label;
}

char *generate_temp(code_generator *gen) {
    char *temp = (char *)malloc(32);
    snprintf(temp, 32, "tmp%d", gen->temp_counter++);
    return temp;
}

// Type system support
int get_type_size(const char *type) {
    if (strcmp(type, "i8") == 0 || strcmp(type, "u8") == 0) return 1;
    if (strcmp(type, "i16") == 0 || strcmp(type, "u16") == 0) return 2;
    if (strcmp(type, "i32") == 0 || strcmp(type, "u32") == 0 || strcmp(type, "f32") == 0) return 4;
    if (strcmp(type, "i64") == 0 || strcmp(type, "u64") == 0 || strcmp(type, "f64") == 0) return 8;
    if (strcmp(type, "bool") == 0) return 1;
    if (strstr(type, "*") != NULL) return 8; // Pointer size
    return 8;                                // Default to 64-bit
}

const char *get_type_suffix(const char *type) {
    int size = get_type_size(type);
    switch (size) {
    case 1:
        return "b";
    case 2:
        return "w";
    case 4:
        return "l";
    case 8:
        return "q";
    default:
        return "q";
    }
}

bool is_floating_type(const char *type) {
    return strcmp(type, "f32") == 0 || strcmp(type, "f64") == 0;
}

bool is_signed_type(const char *type) {
    return strncmp(type, "i", 1) == 0 || strcmp(type, "f32") == 0 || strcmp(type, "f64") == 0;
}

// Node generation dispatch
void generate_node(code_generator *gen, ast_node *node) {
    if (!node) return;

    if (gen->debug_info) {
        char comment[128];
        snprintf(comment, sizeof(comment), "Node: %s", node_type_name(node->type));
        append_comment(gen, comment);
    }

    switch (node->type) {
    case NODE_PROGRAM:
        generate_program(gen, node);
        break;
    case NODE_FUNCTION:
        generate_function(gen, node);
        break;
    case NODE_VARIABLE_DECLARATION:
        generate_variable_declaration(gen, node);
        break;
    case NODE_BLOCK:
        generate_block(gen, node);
        break;
    case NODE_IF_STATEMENT:
        generate_if_statement(gen, node);
        break;
    case NODE_WHILE_STATEMENT:
        generate_while_statement(gen, node);
        break;
    case NODE_FOR_STATEMENT:
        generate_for_statement(gen, node);
        break;
    case NODE_RETURN_STATEMENT:
        generate_return_statement(gen, node);
        break;
    case NODE_EXPRESSION_STATEMENT:
        if (node->child_count > 0) {
            generate_node(gen, node->children[0]);
        }
        break;
    case NODE_ASSIGNMENT:
        generate_assignment(gen, node);
        break;
    case NODE_BINARY_OP:
        generate_binary_op(gen, node);
        break;
    case NODE_UNARY_OP:
        generate_unary_op(gen, node);
        break;
    case NODE_FUNCTION_CALL:
        generate_function_call(gen, node);
        break;
    case NODE_ARRAY_ACCESS:
        generate_array_access(gen, node);
        break;
    case NODE_MEMBER_ACCESS:
        generate_member_access(gen, node);
        break;
    case NODE_TERNARY:
        generate_ternary(gen, node);
        break;
    case NODE_NUMBER_LITERAL:
        generate_number_literal(gen, node);
        break;
    case NODE_FLOAT_LITERAL:
        generate_float_literal(gen, node);
        break;
    case NODE_STRING_LITERAL:
        generate_string_literal(gen, node);
        break;
    case NODE_CHAR_LITERAL:
        generate_char_literal(gen, node);
        break;
    case NODE_BOOL_LITERAL:
        generate_bool_literal(gen, node);
        break;
    case NODE_IDENTIFIER:
        generate_identifier(gen, node);
        break;
    default:
        append_comment(gen, "Unsupported node type");
        break;
    }
}

// Declaration generation
void generate_program(code_generator *gen, ast_node *node) {
    append_comment(gen, "Program start");

    for (int i = 0; i < node->child_count; i++) {
        generate_node(gen, node->children[i]);
    }
}

void generate_function(code_generator *gen, ast_node *node) {
    if (node->child_count < 2) return;

    const char *func_name = node->value;
    ast_node *return_type = node->children[0];
    ast_node *params = (node->child_count > 2) ? node->children[1] : NULL;
    ast_node *body = node->children[node->child_count - 1];

    // Add function to symbol table
    add_function(gen, func_name, return_type->value);

    // Set current function state
    gen->in_function = true;
    gen->current_function = string_duplicate(func_name, strlen(func_name));

    // Enter function scope
    enter_scope(gen);

    // Generate function label
    append_label(gen, func_name);

    // Generate function prologue
    generate_function_prologue(gen, func_name, 64); // Simplified stack size

    // Process parameters
    if (params && params->type == NODE_PARAMETER_LIST) {
        for (int i = 0; i < params->child_count; i++) {
            ast_node *param = params->children[i];
            if (param->type == NODE_PARAMETER && param->child_count > 0) {
                ast_node *param_type = param->children[0];
                const char *param_name = param->value ? param->value : "unnamed";
                int param_size = get_type_size(param_type->value);
                add_variable(gen, param_name, param_type->value, param_size, true);
            }
        }
    }

    // Generate function body
    generate_node(gen, body);

    // Generate epilogue if no explicit return
    generate_function_epilogue(gen);

    // Exit function scope
    exit_scope(gen);
    gen->in_function = false;
    free(gen->current_function);
    gen->current_function = NULL;
}

void generate_variable_declaration(code_generator *gen, ast_node *node) {
    if (node->child_count < 1) return;

    const char *var_name = node->value;
    ast_node *type_node = node->children[0];
    const char *type_name = type_node->value;
    int var_size = get_type_size(type_name);

    // Add to symbol table
    add_variable(gen, var_name, type_name, var_size, false);

    // Generate initialization if present
    if (node->child_count > 1) {
        ast_node *init = node->children[1];
        generate_node(gen, init);

        // Store result in variable
        struct variable_info *var = find_variable(gen, var_name);
        if (var && gen->in_function) {
            char operand[64];
            snprintf(operand, sizeof(operand), "%%rax, %d(%%rbp)", var->stack_offset);
            append_instruction(gen, "mov", operand);
        }
    }
}

// Statement generation
void generate_block(code_generator *gen, ast_node *node) {
    enter_scope(gen);

    for (int i = 0; i < node->child_count; i++) {
        generate_node(gen, node->children[i]);
    }

    exit_scope(gen);
}

void generate_if_statement(code_generator *gen, ast_node *node) {
    if (node->child_count < 2) return;

    ast_node *condition = node->children[0];
    ast_node *then_stmt = node->children[1];
    ast_node *else_stmt = (node->child_count > 2) ? node->children[2] : NULL;

    char *else_label = generate_label(gen, "else_");
    char *end_label = generate_label(gen, "endif_");

    // Generate condition
    generate_node(gen, condition);

    // Test result and jump
    append_instruction(gen, "test", "%rax, %rax");
    append_instruction(gen, "je", else_label);

    // Generate then branch
    generate_node(gen, then_stmt);
    append_instruction(gen, "jmp", end_label);

    // Generate else branch
    append_label(gen, else_label);
    if (else_stmt) {
        generate_node(gen, else_stmt);
    }

    append_label(gen, end_label);

    free(else_label);
    free(end_label);
}

void generate_while_statement(code_generator *gen, ast_node *node) {
    if (node->child_count < 2) return;

    ast_node *condition = node->children[0];
    ast_node *body = node->children[1];

    char *loop_label = generate_label(gen, "loop_");
    char *end_label = generate_label(gen, "endloop_");

    append_label(gen, loop_label);

    // Generate condition
    generate_node(gen, condition);

    // Test and exit if false
    append_instruction(gen, "test", "%rax, %rax");
    append_instruction(gen, "je", end_label);

    // Generate body
    generate_node(gen, body);

    // Jump back to condition
    append_instruction(gen, "jmp", loop_label);

    append_label(gen, end_label);

    free(loop_label);
    free(end_label);
}

void generate_for_statement(code_generator *gen, ast_node *node) {
    if (node->child_count < 3) return;

    ast_node *init = node->children[0];
    ast_node *condition = node->children[1];
    ast_node *update = node->children[2];
    ast_node *body = (node->child_count > 3) ? node->children[3] : NULL;

    char *loop_label = generate_label(gen, "for_loop_");
    char *condition_label = generate_label(gen, "for_condition_");
    char *end_label = generate_label(gen, "for_end_");

    // Generate initialization
    if (init) {
        generate_node(gen, init);
    }

    // Jump to condition check
    append_instruction(gen, "jmp", condition_label);

    // Loop body label
    append_label(gen, loop_label);

    // Generate body
    if (body) {
        generate_node(gen, body);
    }

    // Generate update expression
    if (update) {
        generate_node(gen, update);
    }

    // Condition check
    append_label(gen, condition_label);
    if (condition) {
        generate_node(gen, condition);
        append_instruction(gen, "test", "%rax, %rax");
        append_instruction(gen, "jne", loop_label);
    } else {
        // No condition means infinite loop (for(;;))
        append_instruction(gen, "jmp", loop_label);
    }

    // End label
    append_label(gen, end_label);

    free(loop_label);
    free(condition_label);
    free(end_label);
}

void generate_switch_statement(code_generator *gen, ast_node *node) {
    if (node->child_count < 2) return;

    ast_node *expr = node->children[0];
    ast_node *body = node->children[1];

    char *end_label = generate_label(gen, "switch_end_");
    char *default_label = generate_label(gen, "switch_default_");

    // Generate switch expression
    generate_node(gen, expr);
    append_instruction(gen, "push", "%rax"); // Save switch value

    // Generate case comparisons
    bool has_default = false;
    for (int i = 0; i < body->child_count; i++) {
        ast_node *case_node = body->children[i];

        if (case_node->type == NODE_CASE_STATEMENT) {
            if (case_node->child_count > 0) {
                ast_node *case_value = case_node->children[0];
                char *case_label = generate_label(gen, "case_");

                // Compare switch value with case value
                append_instruction(gen, "mov", "(%rsp), %rax"); // Load switch value
                generate_node(gen, case_value);
                append_instruction(gen, "mov", "%rax, %rbx");
                append_instruction(gen, "mov", "(%rsp), %rax");
                append_instruction(gen, "cmp", "%rbx, %rax");
                append_instruction(gen, "je", case_label);

                free(case_label);
            }
        } else if (case_node->type == NODE_DEFAULT_STATEMENT) {
            has_default = true;
        }
    }

    // If no case matched, jump to default or end
    if (has_default) {
        append_instruction(gen, "jmp", default_label);
    } else {
        append_instruction(gen, "jmp", end_label);
    }

    // Generate case bodies
    for (int i = 0; i < body->child_count; i++) {
        ast_node *case_node = body->children[i];

        if (case_node->type == NODE_CASE_STATEMENT) {
            if (case_node->child_count > 0) {
                ast_node *case_value = case_node->children[0];
                char *case_label = generate_label(gen, "case_");

                append_label(gen, case_label);

                // Generate case body (statements after the case value)
                for (int j = 1; j < case_node->child_count; j++) {
                    generate_node(gen, case_node->children[j]);
                }

                free(case_label);
            }
        } else if (case_node->type == NODE_DEFAULT_STATEMENT) {
            append_label(gen, default_label);

            // Generate default body
            for (int j = 0; j < case_node->child_count; j++) {
                generate_node(gen, case_node->children[j]);
            }
        }
    }

    // End label
    append_label(gen, end_label);
    append_instruction(gen, "add", "$8, %rsp"); // Clean up switch value from stack

    free(end_label);
    free(default_label);
}

void generate_return_statement(code_generator *gen, ast_node *node) {
    if (node->child_count > 0) {
        // Generate return value
        generate_node(gen, node->children[0]);
        // Result is already in %rax
    } else {
        // Return 0 by default
        append_instruction(gen, "mov", "$0, %rax");
    }

    // Jump to function epilogue
    generate_function_epilogue(gen);
}

void generate_break_statement(code_generator *gen, ast_node *node) {
    // For break statements, we need to jump to the nearest loop/switch end
    // This is a simplified version - a real implementation would need
    // to track the current loop/switch context
    append_comment(gen, "break statement");

    // In a more complete implementation, you'd maintain a stack of
    // break/continue labels and jump to the appropriate one
    // For now, this is a placeholder that would need context tracking
    append_instruction(gen, "# break", "- needs context tracking");
}

void generate_continue_statement(code_generator *gen, ast_node *node) {
    // For continue statements, we need to jump to the nearest loop condition
    // This is a simplified version - a real implementation would need
    // to track the current loop context
    append_comment(gen, "continue statement");

    // In a more complete implementation, you'd maintain a stack of
    // break/continue labels and jump to the appropriate one
    // For now, this is a placeholder that would need context tracking
    append_instruction(gen, "# continue", "- needs context tracking");
}

// Expression generation
void generate_binary_op(code_generator *gen, ast_node *node) {
    if (node->child_count != 2) return;

    ast_node *left = node->children[0];
    ast_node *right = node->children[1];
    const char *op = node->value;

    // Generate left operand
    generate_node(gen, left);
    append_instruction(gen, "push", "%rax");

    // Generate right operand
    generate_node(gen, right);
    append_instruction(gen, "mov", "%rax, %rbx");
    append_instruction(gen, "pop", "%rax");

    // Perform operation
    if (strcmp(op, "+") == 0) {
        append_instruction(gen, "add", "%rbx, %rax");
    } else if (strcmp(op, "-") == 0) {
        append_instruction(gen, "sub", "%rbx, %rax");
    } else if (strcmp(op, "*") == 0) {
        append_instruction(gen, "imul", "%rbx, %rax");
    } else if (strcmp(op, "/") == 0) {
        append_instruction(gen, "cqo", "");
        append_instruction(gen, "idiv", "%rbx");
    } else if (strcmp(op, "%") == 0) {
        append_instruction(gen, "cqo", "");
        append_instruction(gen, "idiv", "%rbx");
        append_instruction(gen, "mov", "%rdx, %rax");
    } else if (strcmp(op, "==") == 0) {
        append_instruction(gen, "cmp", "%rbx, %rax");
        append_instruction(gen, "sete", "%al");
        append_instruction(gen, "movzb", "%al, %rax");
    } else if (strcmp(op, "!=") == 0) {
        append_instruction(gen, "cmp", "%rbx, %rax");
        append_instruction(gen, "setne", "%al");
        append_instruction(gen, "movzb", "%al, %rax");
    } else if (strcmp(op, "<") == 0) {
        append_instruction(gen, "cmp", "%rbx, %rax");
        append_instruction(gen, "setl", "%al");
        append_instruction(gen, "movzb", "%al, %rax");
    } else if (strcmp(op, ">") == 0) {
        append_instruction(gen, "cmp", "%rbx, %rax");
        append_instruction(gen, "setg", "%al");
        append_instruction(gen, "movzb", "%al, %rax");
    } else if (strcmp(op, "<=") == 0) {
        append_instruction(gen, "cmp", "%rbx, %rax");
        append_instruction(gen, "setle", "%al");
        append_instruction(gen, "movzb", "%al, %rax");
    } else if (strcmp(op, ">=") == 0) {
        append_instruction(gen, "cmp", "%rbx, %rax");
        append_instruction(gen, "setge", "%al");
        append_instruction(gen, "movzb", "%al, %rax");
    }
}

void generate_unary_op(code_generator *gen, ast_node *node) {
    if (node->child_count != 1) return;

    ast_node *operand = node->children[0];
    const char *op = node->value;

    if (strcmp(op, "-") == 0) {
        generate_node(gen, operand);
        append_instruction(gen, "neg", "%rax");
    } else if (strcmp(op, "!") == 0) {
        generate_node(gen, operand);
        append_instruction(gen, "test", "%rax, %rax");
        append_instruction(gen, "sete", "%al");
        append_instruction(gen, "movzb", "%al, %rax");
    } else if (strcmp(op, "~") == 0) {
        generate_node(gen, operand);
        append_instruction(gen, "not", "%rax");
    } else if (strcmp(op, "&") == 0) {
        // Address-of operator
        if (operand->type == NODE_IDENTIFIER) {
            struct variable_info *var = find_variable(gen, operand->value);
            if (var && gen->in_function) {
                char operand_str[64];
                snprintf(operand_str, sizeof(operand_str), "$%d, %%rax", var->stack_offset);
                append_instruction(gen, "lea", operand_str);
                append_instruction(gen, "add", "%rbp, %rax");
            }
        }
    } else if (strcmp(op, "*") == 0) {
        // Dereference operator
        generate_node(gen, operand);
        append_instruction(gen, "mov", "(%rax), %rax");
    }
}

void generate_assignment(code_generator *gen, ast_node *node) {
    if (node->child_count != 2) return;

    ast_node *target = node->children[0];
    ast_node *value = node->children[1];
    const char *op = node->value;

    // Generate value
    generate_node(gen, value);

    // Handle different assignment operators
    if (strcmp(op, "=") == 0) {
        // Simple assignment - value is already in %rax
    } else if (strcmp(op, "+=") == 0) {
        append_instruction(gen, "push", "%rax");
        generate_node(gen, target);
        append_instruction(gen, "pop", "%rbx");
        append_instruction(gen, "add", "%rbx, %rax");
    } else if (strcmp(op, "-=") == 0) {
        append_instruction(gen, "push", "%rax");
        generate_node(gen, target);
        append_instruction(gen, "pop", "%rbx");
        append_instruction(gen, "sub", "%rbx, %rax");
    } else if (strcmp(op, "*=") == 0) {
        append_instruction(gen, "push", "%rax");
        generate_node(gen, target);
        append_instruction(gen, "pop", "%rbx");
        append_instruction(gen, "imul", "%rbx, %rax");
    }

    // Store result
    if (target->type == NODE_IDENTIFIER) {
        struct variable_info *var = find_variable(gen, target->value);
        if (var && gen->in_function) {
            char operand_str[64];
            snprintf(operand_str, sizeof(operand_str), "%%rax, %d(%%rbp)", var->stack_offset);
            append_instruction(gen, "mov", operand_str);
        }
    }
}

void generate_function_call(code_generator *gen, ast_node *node) {
    const char *func_name = node->value;

    // Handle built-in functions
    if (strcmp(func_name, "printf") == 0) {
        generate_printf(gen, node);
        return;
    }

    // Generate arguments in reverse order (right to left)
    for (int i = node->child_count - 1; i >= 0; i--) {
        generate_node(gen, node->children[i]);
        append_instruction(gen, "push", "%rax");
    }

    // Call function
    generate_call_instruction(gen, func_name);

    // Clean up stack (simplified - assumes all args are 8 bytes)
    if (node->child_count > 0) {
        char cleanup[32];
        snprintf(cleanup, sizeof(cleanup), "$%d, %%rsp", node->child_count * 8);
        append_instruction(gen, "add", cleanup);
    }
}

void generate_array_access(code_generator *gen, ast_node *node) {
    if (node->child_count != 2) return;

    ast_node *array = node->children[0];
    ast_node *index = node->children[1];

    // Generate array address
    generate_node(gen, array);
    append_instruction(gen, "push", "%rax");

    // Generate index
    generate_node(gen, index);
    append_instruction(gen, "imul", "$8, %rax"); // Assume 8-byte elements
    append_instruction(gen, "pop", "%rbx");
    append_instruction(gen, "add", "%rbx, %rax");
    append_instruction(gen, "mov", "(%rax), %rax");
}

void generate_member_access(code_generator *gen, ast_node *node) {
    if (node->child_count != 2) return;

    ast_node *object = node->children[0];
    ast_node *member = node->children[1];
    const char *access_op = node->value;

    // Generate object
    generate_node(gen, object);

    // For now, simplified member access
    // In a real implementation, you'd look up member offsets in struct definitions
    if (strcmp(access_op, ".") == 0) {
        // Direct member access
        append_instruction(gen, "add", "$0, %rax"); // Add member offset
    } else if (strcmp(access_op, "->") == 0) {
        // Pointer member access
        append_instruction(gen, "mov", "(%rax), %rax");
        append_instruction(gen, "add", "$0, %rax"); // Add member offset
    }
}

void generate_ternary(code_generator *gen, ast_node *node) {
    if (node->child_count != 3) return;

    ast_node *condition = node->children[0];
    ast_node *true_expr = node->children[1];
    ast_node *false_expr = node->children[2];

    char *false_label = generate_label(gen, "ternary_false_");
    char *end_label = generate_label(gen, "ternary_end_");

    // Generate condition
    generate_node(gen, condition);
    append_instruction(gen, "test", "%rax, %rax");
    append_instruction(gen, "je", false_label);

    // True branch
    generate_node(gen, true_expr);
    append_instruction(gen, "jmp", end_label);

    // False branch
    append_label(gen, false_label);
    generate_node(gen, false_expr);

    append_label(gen, end_label);

    free(false_label);
    free(end_label);
}

// Literal generation
void generate_number_literal(code_generator *gen, ast_node *node) {
    char instruction[64];
    snprintf(instruction, sizeof(instruction), "$%s, %%rax", node->value);
    append_instruction(gen, "mov", instruction);
}

void generate_float_literal(code_generator *gen, ast_node *node) {
    // For floating point, we'd need to handle differently
    // This is a simplified version
    char instruction[64];
    snprintf(instruction, sizeof(instruction), "$%s, %%rax", node->value);
    append_instruction(gen, "mov", instruction);
}

void generate_string_literal(code_generator *gen, ast_node *node) {
    int str_index = find_string_index(gen, node->value);
    char instruction[64];
    snprintf(instruction, sizeof(instruction), "$str%d, %%rax", str_index);
    append_instruction(gen, "mov", instruction);
}

void generate_char_literal(code_generator *gen, ast_node *node) {
    char instruction[64];
    if (node->value && strlen(node->value) > 0) {
        snprintf(instruction, sizeof(instruction), "$%d, %%rax", (int)node->value[0]);
    } else {
        snprintf(instruction, sizeof(instruction), "$0, %%rax");
    }
    append_instruction(gen, "mov", instruction);
}

void generate_bool_literal(code_generator *gen, ast_node *node) {
    if (strcmp(node->value, "true") == 0) {
        append_instruction(gen, "mov", "$1, %rax");
    } else {
        append_instruction(gen, "mov", "$0, %rax");
    }
}

void generate_identifier(code_generator *gen, ast_node *node) {
    struct variable_info *var = find_variable(gen, node->value);
    if (var && gen->in_function) {
        char operand[64];
        snprintf(operand, sizeof(operand), "%d(%%rbp), %%rax", var->stack_offset);
        append_instruction(gen, "mov", operand);
    }
}

// Architecture-specific code generation
void generate_function_prologue(code_generator *gen, const char *func_name, int stack_size) {
    append_instruction(gen, "push", "%rbp");
    append_instruction(gen, "mov", "%rsp, %rbp");

    if (stack_size > 0) {
        char stack_alloc[32];
        snprintf(stack_alloc, sizeof(stack_alloc), "$%d, %%rsp", stack_size);
        append_instruction(gen, "sub", stack_alloc);
    }
}

void generate_function_epilogue(code_generator *gen) {
    append_instruction(gen, "mov", "%rbp, %rsp");
    append_instruction(gen, "pop", "%rbp");
    append_instruction(gen, "ret", "");
}

void generate_call_instruction(code_generator *gen, const char *func_name) {
    append_instruction(gen, "call", func_name);
}

void generate_syscall(code_generator *gen, int syscall_num) {
    char syscall_str[32];
    snprintf(syscall_str, sizeof(syscall_str), "$%d, %%rax", syscall_num);
    append_instruction(gen, "mov", syscall_str);
    append_instruction(gen, "syscall", "");
}

// Built-in function support
void generate_printf(code_generator *gen, ast_node *node) {
    if (node->child_count == 0) return;

    // Handle format string and arguments
    if (node->child_count >= 2) {
        ast_node *format = node->children[0];
        ast_node *arg = node->children[1];

        if (format->type == NODE_STRING_LITERAL && strcmp(format->value, "%d") == 0) {
            // Integer printf
            generate_node(gen, arg);

            // Convert number to string (simplified)
            char *num_str = (char *)malloc(32);
            snprintf(num_str, 32, "%lld", arg->data.literal.int_value);
            int str_index = find_string_index(gen, num_str);

            char mov_instr[64];
            snprintf(mov_instr, sizeof(mov_instr), "$str%d, %%rsi", str_index);
            append_instruction(gen, "mov", mov_instr);

            char len_instr[64];
            snprintf(len_instr, sizeof(len_instr), "$%d, %%rdx", (int)strlen(num_str));
            append_instruction(gen, "mov", len_instr);

            free(num_str);
        }
    } else {
        // Simple string printf
        generate_node(gen, node->children[0]);
        append_instruction(gen, "mov", "%rax, %rsi");

        if (node->children[0]->type == NODE_STRING_LITERAL) {
            char len_instr[64];
            snprintf(len_instr, sizeof(len_instr), "$%d, %%rdx", (int)strlen(node->children[0]->value));
            append_instruction(gen, "mov", len_instr);
        }
    }

    // Write syscall
    append_instruction(gen, "mov", "$1, %rdi"); // stdout
    append_instruction(gen, "mov", "$1, %rax"); // write syscall
    append_instruction(gen, "syscall", "");
}

// Output generation
char *generate(code_generator *gen, ast_node *ast) {
    // Generate program
    generate_node(gen, ast);

    // Build final output
    append_string(gen, ".global _start\n");

    // Data section
    append_string(gen, ".section .data\n");
    for (int i = 0; i < gen->string_count; i++) {
        char str_def[512];
        snprintf(str_def, sizeof(str_def), "str%d: .ascii \"%s\"\n", i, gen->strings[i]);
        append_string(gen, str_def);
    }

    // Text section
    append_string(gen, ".section .text\n");
    append_string(gen, "_start:\n");
    append_instruction(gen, "call", "main");
    append_instruction(gen, "mov", "%rax, %rdi");
    generate_syscall(gen, 60); // exit syscall

    return gen->output;
}

// Error handling
void codegen_error(code_generator *gen, const char *message) {
    if (gen->error_count >= 16) return; // Max errors

    gen->errors[gen->error_count] = string_duplicate(message, strlen(message));
    gen->error_count++;

    fprintf(stderr, "Code generation error: %s\n", message);
}

// Debug functions
void print_symbol_table(code_generator *gen) {
    printf("=== Symbol Table ===\n");
    printf("Variables:\n");
    for (int i = 0; i < gen->symbols.var_count; i++) {
        struct variable_info *var = &gen->symbols.variables[i];
        printf("  %s: %s (offset: %d, size: %d)\n", var->name, var->type, var->stack_offset, var->size);
    }

    printf("Functions:\n");
    for (int i = 0; i < gen->symbols.func_count; i++) {
        struct function_info *func = &gen->symbols.functions[i];
        printf("  %s: %s (stack: %d, params: %d)\n", func->name, func->return_type, func->stack_size,
               func->param_count);
    }
}
