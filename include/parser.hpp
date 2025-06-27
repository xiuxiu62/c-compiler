#pragma once

#include "ast.hpp"
#include "lexer.hpp"

struct parser {
    struct lexer *lexer;
    token current_token;
    int error_count;
    bool panic_mode;
};

// Core parser functions
parser create_parser(struct lexer *lexer);
ast_node *parse(struct parser *parser);
void parser_advance(struct parser *parser);
bool consume(struct parser *parser, token_type expected);
bool match(struct parser *parser, token_type type);
bool check(struct parser *parser, token_type type);

// Expression parsing (precedence climbing)
ast_node *parse_expression(struct parser *parser);
ast_node *parse_assignment(struct parser *parser);
ast_node *parse_ternary(struct parser *parser);
ast_node *parse_logical_or(struct parser *parser);
ast_node *parse_logical_and(struct parser *parser);
ast_node *parse_bitwise_or(struct parser *parser);
ast_node *parse_bitwise_xor(struct parser *parser);
ast_node *parse_bitwise_and(struct parser *parser);
ast_node *parse_equality(struct parser *parser);
ast_node *parse_relational(struct parser *parser);
ast_node *parse_shift(struct parser *parser);
ast_node *parse_additive(struct parser *parser);
ast_node *parse_multiplicative(struct parser *parser);
ast_node *parse_unary(struct parser *parser);
ast_node *parse_postfix(struct parser *parser);
ast_node *parse_primary(struct parser *parser);

// Statement parsing
ast_node *parse_statement(struct parser *parser);
ast_node *parse_expression_statement(struct parser *parser);
ast_node *parse_block(struct parser *parser);
ast_node *parse_if_statement(struct parser *parser);
ast_node *parse_while_statement(struct parser *parser);
ast_node *parse_for_statement(struct parser *parser);
ast_node *parse_do_while_statement(struct parser *parser);
ast_node *parse_switch_statement(struct parser *parser);
ast_node *parse_return_statement(struct parser *parser);
ast_node *parse_break_statement(struct parser *parser);
ast_node *parse_continue_statement(struct parser *parser);

// Declaration parsing
ast_node *parse_declaration(struct parser *parser);
ast_node *parse_variable_declaration(struct parser *parser);
ast_node *parse_function_declaration(struct parser *parser);
ast_node *parse_struct_declaration(struct parser *parser);
ast_node *parse_enum_declaration(struct parser *parser);
ast_node *parse_union_declaration(struct parser *parser);

// Type parsing
ast_node *parse_type(struct parser *parser);
ast_node *parse_type_specifier(struct parser *parser);
bool is_type_token(token_type type);

// Function and parameter parsing
ast_node *parse_function_call(struct parser *parser);
ast_node *parse_parameter_list(struct parser *parser);
ast_node *parse_argument_list(struct parser *parser);

// Array and pointer parsing
ast_node *parse_array_access(struct parser *parser);
ast_node *parse_member_access(struct parser *parser);

// Module system parsing
ast_node *parse_import_statement(struct parser *parser);
ast_node *parse_export_statement(struct parser *parser);
ast_node *parse_module_declaration(struct parser *parser);

// Error handling
void parser_error(struct parser *parser, const char *message);
void parser_error_at_current(struct parser *parser, const char *message);
void parser_error_at_previous(struct parser *parser, const char *message);
void synchronize(struct parser *parser);

// Utility functions
token previous_token(struct parser *parser);
bool is_at_end(struct parser *parser);
void reset_parser_state(struct parser *parser);
