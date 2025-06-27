#include "ast.hpp"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "util.hpp"

// Global memory tracking
static size_t total_nodes_allocated = 0;
static size_t total_memory_used = 0;

// Core AST Node functions
ast_node *create_node(node_type type) {
    ast_node *node = (ast_node *)malloc(sizeof(ast_node));
    if (!node) return NULL;

    node->type = type;
    node->value = NULL;
    node->children = (ast_node **)malloc(sizeof(ast_node *) * 4);
    node->child_count = 0;
    node->child_capacity = 4;

    // Initialize metadata
    memset(&node->data, 0, sizeof(node->data));

    // Update memory tracking
    total_nodes_allocated++;
    total_memory_used += sizeof(ast_node) + sizeof(ast_node *) * 4;

    return node;
}

ast_node *create_node_with_value(node_type type, const char *value) {
    ast_node *node = create_node(type);
    if (!node) return NULL;

    if (value) {
        node->value = string_duplicate(value, strlen(value));
        total_memory_used += strlen(value) + 1;
    }

    return node;
}

ast_node *create_literal_node(node_type type, const char *value, long long int_val, double float_val, bool bool_val) {
    ast_node *node = create_node_with_value(type, value);
    if (!node) return NULL;

    node->data.literal.int_value = int_val;
    node->data.literal.float_value = float_val;
    node->data.literal.bool_value = bool_val;

    return node;
}

void add_child(ast_node *parent, ast_node *child) {
    if (!parent || !child) return;

    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity *= 2;
        size_t old_size = sizeof(ast_node *) * (parent->child_capacity / 2);
        size_t new_size = sizeof(ast_node *) * parent->child_capacity;

        parent->children = (ast_node **)realloc(parent->children, new_size);
        total_memory_used += (new_size - old_size);
    }
    parent->children[parent->child_count++] = child;
}

void insert_child(ast_node *parent, int index, ast_node *child) {
    if (!parent || !child || index < 0 || index > parent->child_count) return;

    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity *= 2;
        parent->children = (ast_node **)realloc(parent->children, sizeof(ast_node *) * parent->child_capacity);
    }

    // Shift existing children to the right
    for (int i = parent->child_count; i > index; i--) {
        parent->children[i] = parent->children[i - 1];
    }

    parent->children[index] = child;
    parent->child_count++;
}

void remove_child(ast_node *parent, int index) {
    if (!parent || index < 0 || index >= parent->child_count) return;

    // Shift children to the left
    for (int i = index; i < parent->child_count - 1; i++) {
        parent->children[i] = parent->children[i + 1];
    }

    parent->child_count--;
}

ast_node *get_child(ast_node *parent, int index) {
    if (!parent || index < 0 || index >= parent->child_count) return NULL;
    return parent->children[index];
}

void free_node(ast_node *node) {
    if (!node) return;

    // Free all children recursively
    for (int i = 0; i < node->child_count; i++) {
        free_node(node->children[i]);
    }

    // Free node data
    free(node->children);
    free(node->value);

    // Update memory tracking
    total_nodes_allocated--;
    total_memory_used -= sizeof(ast_node);
    if (node->value) {
        total_memory_used -= strlen(node->value) + 1;
    }
    total_memory_used -= sizeof(ast_node *) * node->child_capacity;

    free(node);
}

ast_node *copy_node(const ast_node *node) {
    if (!node) return NULL;

    ast_node *copy = create_node_with_value(node->type, node->value);
    if (!copy) return NULL;

    // Copy metadata
    copy->data = node->data;

    // Copy children
    for (int i = 0; i < node->child_count; i++) {
        ast_node *child_copy = copy_node(node->children[i]);
        if (child_copy) {
            add_child(copy, child_copy);
        }
    }

    return copy;
}

// AST traversal functions
void visit_node(ast_node *node, void (*visitor)(ast_node *)) {
    if (!node || !visitor) return;

    visitor(node);

    for (int i = 0; i < node->child_count; i++) {
        visit_node(node->children[i], visitor);
    }
}

void visit_node_with_context(ast_node *node, void (*visitor)(ast_node *, void *), void *context) {
    if (!node || !visitor) return;

    visitor(node, context);

    for (int i = 0; i < node->child_count; i++) {
        visit_node_with_context(node->children[i], visitor, context);
    }
}

ast_node *find_node_by_type(ast_node *root, node_type type) {
    if (!root) return NULL;

    if (root->type == type) {
        return root;
    }

    for (int i = 0; i < root->child_count; i++) {
        ast_node *found = find_node_by_type(root->children[i], type);
        if (found) {
            return found;
        }
    }

    return NULL;
}

ast_node *find_node_by_value(ast_node *root, const char *value) {
    if (!root || !value) return NULL;

    if (root->value && strcmp(root->value, value) == 0) {
        return root;
    }

    for (int i = 0; i < root->child_count; i++) {
        ast_node *found = find_node_by_value(root->children[i], value);
        if (found) {
            return found;
        }
    }

    return NULL;
}

// Utility functions
const char *node_type_name(node_type type) {
    switch (type) {
    // Program structure
    case NODE_PROGRAM:
        return "PROGRAM";
    case NODE_MODULE:
        return "MODULE";
    case NODE_IMPORT:
        return "IMPORT";
    case NODE_EXPORT:
        return "EXPORT";

    // Declarations
    case NODE_FUNCTION:
        return "FUNCTION";
    case NODE_VARIABLE_DECLARATION:
        return "VARIABLE_DECLARATION";
    case NODE_STRUCT:
        return "STRUCT";
    case NODE_ENUM:
        return "ENUM";
    case NODE_UNION:
        return "UNION";
    case NODE_PARAMETER:
        return "PARAMETER";
    case NODE_PARAMETER_LIST:
        return "PARAMETER_LIST";

    // Types
    case NODE_TYPE:
        return "TYPE";
    case NODE_POINTER_TYPE:
        return "POINTER_TYPE";
    case NODE_ARRAY_TYPE:
        return "ARRAY_TYPE";

    // Statements
    case NODE_BLOCK:
        return "BLOCK";
    case NODE_EXPRESSION_STATEMENT:
        return "EXPRESSION_STATEMENT";
    case NODE_RETURN_STATEMENT:
        return "RETURN_STATEMENT";
    case NODE_IF_STATEMENT:
        return "IF_STATEMENT";
    case NODE_WHILE_STATEMENT:
        return "WHILE_STATEMENT";
    case NODE_FOR_STATEMENT:
        return "FOR_STATEMENT";
    case NODE_DO_WHILE_STATEMENT:
        return "DO_WHILE_STATEMENT";
    case NODE_SWITCH_STATEMENT:
        return "SWITCH_STATEMENT";
    case NODE_CASE_STATEMENT:
        return "CASE_STATEMENT";
    case NODE_DEFAULT_STATEMENT:
        return "DEFAULT_STATEMENT";
    case NODE_BREAK_STATEMENT:
        return "BREAK_STATEMENT";
    case NODE_CONTINUE_STATEMENT:
        return "CONTINUE_STATEMENT";

    // Expressions
    case NODE_ASSIGNMENT:
        return "ASSIGNMENT";
    case NODE_BINARY_OP:
        return "BINARY_OP";
    case NODE_UNARY_OP:
        return "UNARY_OP";
    case NODE_POSTFIX_OP:
        return "POSTFIX_OP";
    case NODE_TERNARY:
        return "TERNARY";
    case NODE_FUNCTION_CALL:
        return "FUNCTION_CALL";
    case NODE_ARRAY_ACCESS:
        return "ARRAY_ACCESS";
    case NODE_MEMBER_ACCESS:
        return "MEMBER_ACCESS";
    case NODE_SIZEOF:
        return "SIZEOF";

    // Literals
    case NODE_NUMBER_LITERAL:
        return "NUMBER_LITERAL";
    case NODE_FLOAT_LITERAL:
        return "FLOAT_LITERAL";
    case NODE_STRING_LITERAL:
        return "STRING_LITERAL";
    case NODE_CHAR_LITERAL:
        return "CHAR_LITERAL";
    case NODE_BOOL_LITERAL:
        return "BOOL_LITERAL";
    case NODE_NULL_LITERAL:
        return "NULL_LITERAL";

    // Identifiers and values
    case NODE_IDENTIFIER:
        return "IDENTIFIER";
    case NODE_ENUM_VALUE:
        return "ENUM_VALUE";

    // Casts and conversions
    case NODE_CAST:
        return "CAST";
    case NODE_TYPE_CONVERSION:
        return "TYPE_CONVERSION";

    default:
        return "UNKNOWN";
    }
}

bool is_literal_node(node_type type) {
    return type >= NODE_NUMBER_LITERAL && type <= NODE_NULL_LITERAL;
}

bool is_statement_node(node_type type) {
    return type >= NODE_BLOCK && type <= NODE_CONTINUE_STATEMENT;
}

bool is_expression_node(node_type type) {
    return (type >= NODE_ASSIGNMENT && type <= NODE_SIZEOF) || is_literal_node(type) || type == NODE_IDENTIFIER;
}

bool is_declaration_node(node_type type) {
    return type >= NODE_FUNCTION && type <= NODE_PARAMETER_LIST;
}

// AST validation
bool validate_ast(ast_node *root) {
    if (!root) return false;

    // Basic validation - ensure children are valid
    for (int i = 0; i < root->child_count; i++) {
        if (!validate_ast(root->children[i])) {
            return false;
        }
    }

    // Type-specific validation
    switch (root->type) {
    case NODE_FUNCTION:
        // Function should have at least return type and body
        return root->child_count >= 2;

    case NODE_BINARY_OP:
        // Binary operations should have exactly 2 children
        return root->child_count == 2;

    case NODE_UNARY_OP:
        // Unary operations should have exactly 1 child
        return root->child_count == 1;

    case NODE_IF_STATEMENT:
        // If statement should have condition and then branch (else is optional)
        return root->child_count >= 2 && root->child_count <= 3;

    case NODE_WHILE_STATEMENT:
        return root->child_count == 2;

    case NODE_FOR_STATEMENT:
        // For loop can have 3-4 children (init, condition, increment, body)
        return root->child_count >= 3 && root->child_count <= 4;

    default:
        return true; // Default to valid for other types
    }
}

// Debug printing
void print_ast(ast_node *node, int indent) {
    if (!node) return;

    for (int i = 0; i < indent; i++) {
        printf("  ");
    }

    printf("%s", node_type_name(node->type));
    if (node->value) {
        printf(": %s", node->value);
    }

    // Print literal values if available
    if (is_literal_node(node->type)) {
        switch (node->type) {
        case NODE_NUMBER_LITERAL:
            printf(" (%lld)", node->data.literal.int_value);
            break;
        case NODE_FLOAT_LITERAL:
            printf(" (%f)", node->data.literal.float_value);
            break;
        case NODE_BOOL_LITERAL:
            printf(" (%s)", node->data.literal.bool_value ? "true" : "false");
            break;
        default:
            break;
        }
    }

    printf("\n");

    for (int i = 0; i < node->child_count; i++) {
        print_ast(node->children[i], indent + 1);
    }
}

void print_ast_debug(ast_node *node, int indent) {
    if (!node) return;

    for (int i = 0; i < indent; i++) {
        printf("  ");
    }

    printf("[%p] %s", (void *)node, node_type_name(node->type));
    if (node->value) {
        printf(": \"%s\"", node->value);
    }
    printf(" (children: %d/%d)", node->child_count, node->child_capacity);
    printf("\n");

    for (int i = 0; i < node->child_count; i++) {
        print_ast_debug(node->children[i], indent + 1);
    }
}

// Memory management
void ast_cleanup_all(void) {
    // This would be used to clean up any global AST state
    total_nodes_allocated = 0;
    total_memory_used = 0;
}

size_t ast_memory_usage(void) {
    return total_memory_used;
}
