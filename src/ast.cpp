#include "ast.hpp"
#include "defines.hpp"
#include "memory.hpp"
#include "stdio.h"
#include "string.h"
#include "util.hpp"

extern compiler_memory *memory;
static size_t total_nodes_allocated = 0;

#define REQUIRE_MEMORY()                                                                                               \
    if (!memory) {                                                                                                     \
        fatal("Compiler memory not initialized\n");                                                                    \
    }

// Core AST Node functions - updated to use memory pool
ast_node *create_node(node_type type) {
    REQUIRE_MEMORY();

    // Allocate from AST arena instead of malloc
    ast_node *node = (ast_node *)arena_alloc(&memory->ast_arena, sizeof(ast_node),
                                             8); // 8-byte alignment
    if (!node) return NULL;

    node->type = type;
    node->value = NULL;
    node->children = NULL; // Will allocate on first child
    node->child_count = 0;
    node->child_capacity = 0;

    // Initialize metadata
    memset(&node->data, 0, sizeof(node->data));

    // Update tracking
    total_nodes_allocated++;

    return node;
}

ast_node *create_node_with_value(node_type type, const char *value) {
    REQUIRE_MEMORY();

    ast_node *node = create_node(type);
    if (!node) return NULL;

    if (value) {
        // Use string pool instead of string_duplicate
        node->value = pool_string_copy(&memory->strings, value);
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
    REQUIRE_MEMORY();

    // Allocate or grow children array if needed
    if (parent->child_count >= parent->child_capacity) {
        size_t new_capacity = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;

        // Allocate new children array from AST arena
        ast_node **new_children = (ast_node **)arena_alloc(&memory->ast_arena, 
                                                          new_capacity * sizeof(ast_node *),
                                                          8); // pointer alignment
        if (!new_children) return;

        // Copy existing children if any
        if (parent->children && parent->child_count > 0) {
            memcpy(new_children, parent->children, parent->child_count * sizeof(ast_node *));
        }

        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }

    parent->children[parent->child_count++] = child;
}

void insert_child(ast_node *parent, int index, ast_node *child) {
    if (!parent || !child || index < 0 || index > parent->child_count) return;

    // Ensure we have space
    if (parent->child_count >= parent->child_capacity) {
        // This will allocate new space if needed
        add_child(parent, child); // Temporary add to ensure space
        parent->child_count--;    // Remove the temporary add
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

// NOTE: free_node is now much simpler since we don't free individual allocations
void free_node(ast_node *node) {
    // With arena allocation, we don't free individual nodes
    // The entire arena gets reset/freed at once
    // This function is kept for API compatibility but does nothing

    if (node) {
        total_nodes_allocated--;
    }
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