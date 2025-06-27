#pragma once

#include "stddef.h"

enum node_type {
    // Program structure
    NODE_PROGRAM,
    NODE_MODULE,
    NODE_IMPORT,
    NODE_EXPORT,

    // Declarations
    NODE_FUNCTION,
    NODE_VARIABLE_DECLARATION,
    NODE_STRUCT,
    NODE_ENUM,
    NODE_UNION,
    NODE_PARAMETER,
    NODE_PARAMETER_LIST,

    // Types
    NODE_TYPE,
    NODE_POINTER_TYPE,
    NODE_ARRAY_TYPE,

    // Statements
    NODE_BLOCK,
    NODE_EXPRESSION_STATEMENT,
    NODE_RETURN_STATEMENT,
    NODE_IF_STATEMENT,
    NODE_WHILE_STATEMENT,
    NODE_FOR_STATEMENT,
    NODE_DO_WHILE_STATEMENT,
    NODE_SWITCH_STATEMENT,
    NODE_CASE_STATEMENT,
    NODE_DEFAULT_STATEMENT,
    NODE_BREAK_STATEMENT,
    NODE_CONTINUE_STATEMENT,

    // Expressions
    NODE_ASSIGNMENT,
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_POSTFIX_OP,
    NODE_TERNARY,
    NODE_FUNCTION_CALL,
    NODE_ARRAY_ACCESS,
    NODE_MEMBER_ACCESS,
    NODE_SIZEOF,

    // Literals
    NODE_NUMBER_LITERAL,
    NODE_FLOAT_LITERAL,
    NODE_STRING_LITERAL,
    NODE_CHAR_LITERAL,
    NODE_BOOL_LITERAL,
    NODE_NULL_LITERAL,

    // Identifiers and values
    NODE_IDENTIFIER,
    NODE_ENUM_VALUE,

    // Casts and conversions
    NODE_CAST,
    NODE_TYPE_CONVERSION,
};

struct ast_node {
    node_type type;
    char *value;
    struct ast_node **children;
    int child_count;
    int child_capacity;

    // Additional metadata
    union {
        struct {
            int line;
            int column;
        } location;

        struct {
            long long int_value;
            double float_value;
            bool bool_value;
        } literal;

        struct {
            char *name;
            struct ast_node *type_info;
        } symbol;
    } data;
};

// AST Node functions
ast_node *create_node(node_type type);
ast_node *create_node_with_value(node_type type, const char *value);
ast_node *create_literal_node(node_type type, const char *value, long long int_val, double float_val, bool bool_val);

void add_child(ast_node *parent, ast_node *child);
void insert_child(ast_node *parent, int index, ast_node *child);
void remove_child(ast_node *parent, int index);
ast_node *get_child(ast_node *parent, int index);

void free_node(ast_node *node);
ast_node *copy_node(const ast_node *node);

// AST traversal and manipulation
void visit_node(ast_node *node, void (*visitor)(ast_node *));
void visit_node_with_context(ast_node *node, void (*visitor)(ast_node *, void *), void *context);
ast_node *find_node_by_type(ast_node *root, node_type type);
ast_node *find_node_by_value(ast_node *root, const char *value);

// AST utility functions
const char *node_type_name(node_type type);
bool is_literal_node(node_type type);
bool is_statement_node(node_type type);
bool is_expression_node(node_type type);
bool is_declaration_node(node_type type);

// AST validation
bool validate_ast(ast_node *root);
void print_ast(ast_node *node, int indent);
void print_ast_debug(ast_node *node, int indent);

// Symbol table integration
struct symbol_info {
    char *name;
    node_type type;
    ast_node *declaration;
    int scope_level;
};

// Memory management
void ast_cleanup_all(void);
size_t ast_memory_usage(void);
