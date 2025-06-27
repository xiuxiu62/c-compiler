#include "parser.hpp"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "util.hpp"

// Core parser functions
parser create_parser(struct lexer *lexer) {
    struct parser parser;
    parser.lexer = lexer;
    parser.current_token = next_token(lexer);
    parser.error_count = 0;
    parser.panic_mode = false;
    return parser;
}

void parser_advance(struct parser *parser) {
    free_token(&parser->current_token);
    parser->current_token = next_token(parser->lexer);

    if (parser->current_token.type == TOKEN_INVALID) {
        parser_error_at_current(parser, "Invalid token");
    }
}

bool consume(struct parser *parser, token_type expected) {
    if (parser->current_token.type == expected) {
        parser_advance(parser);
        return true;
    }

    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Expected %s, got %s", token_type_name(expected),
             token_type_name(parser->current_token.type));
    parser_error_at_current(parser, error_msg);
    return false;
}

bool match(struct parser *parser, token_type type) {
    if (check(parser, type)) {
        parser_advance(parser);
        return true;
    }
    return false;
}

bool check(struct parser *parser, token_type type) {
    return parser->current_token.type == type;
}

bool is_at_end(struct parser *parser) {
    return parser->current_token.type == TOKEN_EOF;
}

bool is_type_token(token_type type) {
    return (type >= TOKEN_I8 && type <= TOKEN_VOID) || type == TOKEN_STRUCT || type == TOKEN_ENUM ||
           type == TOKEN_UNION;
}

// Main parsing function
ast_node *parse(struct parser *parser) {
    ast_node *program = create_node(NODE_PROGRAM);

    while (!is_at_end(parser)) {
        if (parser->panic_mode) synchronize(parser);

        ast_node *decl = parse_declaration(parser);
        if (decl) {
            add_child(program, decl);
        }
    }

    return program;
}

// Declaration parsing
ast_node *parse_declaration(struct parser *parser) {
    if (match(parser, TOKEN_MODULE)) {
        return parse_module_declaration(parser);
    }
    if (match(parser, TOKEN_IMPORT)) {
        return parse_import_statement(parser);
    }
    if (match(parser, TOKEN_EXPORT)) {
        return parse_export_statement(parser);
    }
    if (match(parser, TOKEN_STRUCT)) {
        return parse_struct_declaration(parser);
    }
    if (match(parser, TOKEN_ENUM)) {
        return parse_enum_declaration(parser);
    }
    if (match(parser, TOKEN_UNION)) {
        return parse_union_declaration(parser);
    }

    // Check if this is a function declaration by looking ahead
    if (is_type_token(parser->current_token.type)) {
        // Save current position to backtrack if needed
        struct lexer saved_lexer = *parser->lexer;
        token saved_token = copy_token(&parser->current_token);

        // Try to parse as function declaration
        parse_type(parser); // consume type
        if (check(parser, TOKEN_IDENTIFIER)) {
            parser_advance(parser); // consume identifier
            if (check(parser, TOKEN_LEFT_PAREN)) {
                // It's a function declaration, restore and parse properly
                *parser->lexer = saved_lexer;
                free_token(&parser->current_token);
                parser->current_token = saved_token;
                return parse_function_declaration(parser);
            }
        }

        // Not a function, restore and parse as variable
        *parser->lexer = saved_lexer;
        free_token(&parser->current_token);
        parser->current_token = saved_token;
        return parse_variable_declaration(parser);
    }

    return parse_statement(parser);
}

ast_node *parse_variable_declaration(struct parser *parser) {
    ast_node *type_node = parse_type(parser);
    if (!type_node) return NULL;

    if (!check(parser, TOKEN_IDENTIFIER)) {
        parser_error_at_current(parser, "Expected variable name");
        free_node(type_node);
        return NULL;
    }

    char *name = string_duplicate(parser->current_token.value, strlen(parser->current_token.value));
    parser_advance(parser);

    ast_node *decl = create_node(NODE_VARIABLE_DECLARATION);
    decl->value = name;
    add_child(decl, type_node);

    // Optional initializer
    if (match(parser, TOKEN_ASSIGN)) {
        ast_node *init = parse_expression(parser);
        if (init) {
            add_child(decl, init);
        }
    }

    consume(parser, TOKEN_SEMICOLON);
    return decl;
}

ast_node *parse_function_declaration(struct parser *parser) {
    ast_node *return_type = parse_type(parser);
    if (!return_type) return NULL;

    if (!check(parser, TOKEN_IDENTIFIER)) {
        parser_error_at_current(parser, "Expected function name");
        free_node(return_type);
        return NULL;
    }

    char *name = string_duplicate(parser->current_token.value, strlen(parser->current_token.value));
    parser_advance(parser);

    ast_node *function = create_node(NODE_FUNCTION);
    function->value = name;
    add_child(function, return_type);

    consume(parser, TOKEN_LEFT_PAREN);
    ast_node *params = parse_parameter_list(parser);
    if (params) {
        add_child(function, params);
    }
    consume(parser, TOKEN_RIGHT_PAREN);

    // Function body
    ast_node *body = parse_block(parser);
    if (body) {
        add_child(function, body);
    }

    return function;
}

ast_node *parse_struct_declaration(struct parser *parser) {
    if (!check(parser, TOKEN_IDENTIFIER)) {
        parser_error_at_current(parser, "Expected struct name");
        return NULL;
    }

    char *name = string_duplicate(parser->current_token.value, strlen(parser->current_token.value));
    parser_advance(parser);

    ast_node *struct_node = create_node(NODE_STRUCT);
    struct_node->value = name;

    consume(parser, TOKEN_LEFT_BRACE);

    // Parse fields
    while (!check(parser, TOKEN_RIGHT_BRACE) && !is_at_end(parser)) {
        ast_node *field = parse_variable_declaration(parser);
        if (field) {
            add_child(struct_node, field);
        }
    }

    consume(parser, TOKEN_RIGHT_BRACE);
    return struct_node;
}

ast_node *parse_enum_declaration(struct parser *parser) {
    if (!check(parser, TOKEN_IDENTIFIER)) {
        parser_error_at_current(parser, "Expected enum name");
        return NULL;
    }

    char *name = string_duplicate(parser->current_token.value, strlen(parser->current_token.value));
    parser_advance(parser);

    ast_node *enum_node = create_node(NODE_ENUM);
    enum_node->value = name;

    consume(parser, TOKEN_LEFT_BRACE);

    // Parse enum values
    while (!check(parser, TOKEN_RIGHT_BRACE) && !is_at_end(parser)) {
        if (!check(parser, TOKEN_IDENTIFIER)) {
            parser_error_at_current(parser, "Expected enum value name");
            break;
        }

        char *value_name = string_duplicate(parser->current_token.value, strlen(parser->current_token.value));
        parser_advance(parser);

        ast_node *enum_value = create_node(NODE_ENUM_VALUE);
        enum_value->value = value_name;

        // Optional explicit value
        if (match(parser, TOKEN_ASSIGN)) {
            ast_node *value_expr = parse_expression(parser);
            if (value_expr) {
                add_child(enum_value, value_expr);
            }
        }

        add_child(enum_node, enum_value);

        if (!match(parser, TOKEN_COMMA)) {
            break;
        }
    }

    consume(parser, TOKEN_RIGHT_BRACE);
    return enum_node;
}

ast_node *parse_union_declaration(struct parser *parser) {
    if (!check(parser, TOKEN_IDENTIFIER)) {
        parser_error_at_current(parser, "Expected union name");
        return NULL;
    }

    char *name = string_duplicate(parser->current_token.value, strlen(parser->current_token.value));
    parser_advance(parser);

    ast_node *union_node = create_node(NODE_UNION);
    union_node->value = name;

    consume(parser, TOKEN_LEFT_BRACE);

    // Parse fields (same as struct)
    while (!check(parser, TOKEN_RIGHT_BRACE) && !is_at_end(parser)) {
        ast_node *field = parse_variable_declaration(parser);
        if (field) {
            add_child(union_node, field);
        }
    }

    consume(parser, TOKEN_RIGHT_BRACE);
    return union_node;
}

// Type parsing
ast_node *parse_type(struct parser *parser) {
    return parse_type_specifier(parser);
}

ast_node *parse_type_specifier(struct parser *parser) {
    ast_node *type_node = NULL;

    switch (parser->current_token.type) {
    case TOKEN_I8:
    case TOKEN_I16:
    case TOKEN_I32:
    case TOKEN_I64:
    case TOKEN_U8:
    case TOKEN_U16:
    case TOKEN_U32:
    case TOKEN_U64:
    case TOKEN_F32:
    case TOKEN_F64:
    case TOKEN_BOOL_TYPE:
    case TOKEN_VOID:
        type_node = create_node(NODE_TYPE);
        type_node->value = string_duplicate(parser->current_token.value, strlen(parser->current_token.value));
        parser_advance(parser);
        break;

    case TOKEN_STRUCT:
    case TOKEN_ENUM:
    case TOKEN_UNION:
        type_node = create_node(NODE_TYPE);
        type_node->value = string_duplicate(parser->current_token.value, strlen(parser->current_token.value));
        parser_advance(parser);

        if (check(parser, TOKEN_IDENTIFIER)) {
            // Named struct/enum/union
            char *combined = (char *)malloc(strlen(type_node->value) + strlen(parser->current_token.value) + 2);
            sprintf(combined, "%s %s", type_node->value, parser->current_token.value);
            free(type_node->value);
            type_node->value = combined;
            parser_advance(parser);
        }
        break;

    case TOKEN_IDENTIFIER:
        // User-defined type
        type_node = create_node(NODE_TYPE);
        type_node->value = string_duplicate(parser->current_token.value, strlen(parser->current_token.value));
        parser_advance(parser);
        break;

    default:
        parser_error_at_current(parser, "Expected type specifier");
        return NULL;
    }

    // Handle pointers
    while (match(parser, TOKEN_MULTIPLY)) {
        ast_node *pointer_type = create_node(NODE_POINTER_TYPE);
        add_child(pointer_type, type_node);
        type_node = pointer_type;
    }

    return type_node;
}

// Statement parsing
ast_node *parse_statement(struct parser *parser) {
    if (match(parser, TOKEN_IF)) {
        return parse_if_statement(parser);
    }
    if (match(parser, TOKEN_WHILE)) {
        return parse_while_statement(parser);
    }
    if (match(parser, TOKEN_FOR)) {
        return parse_for_statement(parser);
    }
    if (match(parser, TOKEN_DO)) {
        return parse_do_while_statement(parser);
    }
    if (match(parser, TOKEN_SWITCH)) {
        return parse_switch_statement(parser);
    }
    if (match(parser, TOKEN_RETURN)) {
        return parse_return_statement(parser);
    }
    if (match(parser, TOKEN_BREAK)) {
        return parse_break_statement(parser);
    }
    if (match(parser, TOKEN_CONTINUE)) {
        return parse_continue_statement(parser);
    }
    if (match(parser, TOKEN_LEFT_BRACE)) {
        return parse_block(parser);
    }

    return parse_expression_statement(parser);
}

ast_node *parse_block(struct parser *parser) {
    ast_node *block = create_node(NODE_BLOCK);

    while (!check(parser, TOKEN_RIGHT_BRACE) && !is_at_end(parser)) {
        ast_node *stmt = parse_statement(parser);
        if (stmt) {
            add_child(block, stmt);
        }
    }

    consume(parser, TOKEN_RIGHT_BRACE);
    return block;
}

ast_node *parse_if_statement(struct parser *parser) {
    consume(parser, TOKEN_LEFT_PAREN);
    ast_node *condition = parse_expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN);

    ast_node *then_stmt = parse_statement(parser);

    ast_node *if_node = create_node(NODE_IF_STATEMENT);
    add_child(if_node, condition);
    add_child(if_node, then_stmt);

    if (match(parser, TOKEN_ELSE)) {
        ast_node *else_stmt = parse_statement(parser);
        add_child(if_node, else_stmt);
    }

    return if_node;
}

ast_node *parse_while_statement(struct parser *parser) {
    consume(parser, TOKEN_LEFT_PAREN);
    ast_node *condition = parse_expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN);

    ast_node *body = parse_statement(parser);

    ast_node *while_node = create_node(NODE_WHILE_STATEMENT);
    add_child(while_node, condition);
    add_child(while_node, body);

    return while_node;
}

ast_node *parse_for_statement(struct parser *parser) {
    consume(parser, TOKEN_LEFT_PAREN);

    ast_node *for_node = create_node(NODE_FOR_STATEMENT);

    // Initializer (optional)
    if (!check(parser, TOKEN_SEMICOLON)) {
        if (is_type_token(parser->current_token.type)) {
            add_child(for_node, parse_variable_declaration(parser));
        } else {
            add_child(for_node, parse_expression_statement(parser));
        }
    } else {
        consume(parser, TOKEN_SEMICOLON);
        add_child(for_node, NULL); // No initializer
    }

    // Condition (optional)
    if (!check(parser, TOKEN_SEMICOLON)) {
        add_child(for_node, parse_expression(parser));
    } else {
        add_child(for_node, NULL); // No condition
    }
    consume(parser, TOKEN_SEMICOLON);

    // Increment (optional)
    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        add_child(for_node, parse_expression(parser));
    } else {
        add_child(for_node, NULL); // No increment
    }
    consume(parser, TOKEN_RIGHT_PAREN);

    // Body
    ast_node *body = parse_statement(parser);
    add_child(for_node, body);

    return for_node;
}

ast_node *parse_return_statement(struct parser *parser) {
    ast_node *ret = create_node(NODE_RETURN_STATEMENT);

    if (!check(parser, TOKEN_SEMICOLON)) {
        ast_node *expr = parse_expression(parser);
        add_child(ret, expr);
    }

    consume(parser, TOKEN_SEMICOLON);
    return ret;
}

ast_node *parse_break_statement(struct parser *parser) {
    consume(parser, TOKEN_SEMICOLON);
    return create_node(NODE_BREAK_STATEMENT);
}

ast_node *parse_continue_statement(struct parser *parser) {
    consume(parser, TOKEN_SEMICOLON);
    return create_node(NODE_CONTINUE_STATEMENT);
}

ast_node *parse_expression_statement(struct parser *parser) {
    ast_node *expr = parse_expression(parser);
    consume(parser, TOKEN_SEMICOLON);

    ast_node *stmt = create_node(NODE_EXPRESSION_STATEMENT);
    add_child(stmt, expr);
    return stmt;
}

// Expression parsing (precedence climbing)
ast_node *parse_expression(struct parser *parser) {
    return parse_assignment(parser);
}

ast_node *parse_assignment(struct parser *parser) {
    ast_node *expr = parse_ternary(parser);

    if (match(parser, TOKEN_ASSIGN) || match(parser, TOKEN_PLUS_ASSIGN) || match(parser, TOKEN_MINUS_ASSIGN) ||
        match(parser, TOKEN_MULTIPLY_ASSIGN) || match(parser, TOKEN_DIVIDE_ASSIGN) ||
        match(parser, TOKEN_MODULO_ASSIGN)) {

        token op = previous_token(parser);
        ast_node *right = parse_assignment(parser);

        ast_node *assign = create_node(NODE_ASSIGNMENT);
        assign->value = string_duplicate(op.value, strlen(op.value));
        add_child(assign, expr);
        add_child(assign, right);
        return assign;
    }

    return expr;
}

ast_node *parse_ternary(struct parser *parser) {
    ast_node *expr = parse_logical_or(parser);

    if (match(parser, TOKEN_QUESTION)) {
        ast_node *then_expr = parse_expression(parser);
        consume(parser, TOKEN_COLON);
        ast_node *else_expr = parse_ternary(parser);

        ast_node *ternary = create_node(NODE_TERNARY);
        add_child(ternary, expr);
        add_child(ternary, then_expr);
        add_child(ternary, else_expr);
        return ternary;
    }

    return expr;
}

ast_node *parse_logical_or(struct parser *parser) {
    ast_node *expr = parse_logical_and(parser);

    while (match(parser, TOKEN_LOGICAL_OR)) {
        token op = previous_token(parser);
        ast_node *right = parse_logical_and(parser);

        ast_node *binary = create_node(NODE_BINARY_OP);
        binary->value = string_duplicate(op.value, strlen(op.value));
        add_child(binary, expr);
        add_child(binary, right);
        expr = binary;
    }

    return expr;
}

ast_node *parse_logical_and(struct parser *parser) {
    ast_node *expr = parse_bitwise_or(parser);

    while (match(parser, TOKEN_LOGICAL_AND)) {
        token op = previous_token(parser);
        ast_node *right = parse_bitwise_or(parser);

        ast_node *binary = create_node(NODE_BINARY_OP);
        binary->value = string_duplicate(op.value, strlen(op.value));
        add_child(binary, expr);
        add_child(binary, right);
        expr = binary;
    }

    return expr;
}

ast_node *parse_bitwise_or(struct parser *parser) {
    ast_node *expr = parse_bitwise_xor(parser);

    while (match(parser, TOKEN_BITWISE_OR)) {
        token op = previous_token(parser);
        ast_node *right = parse_bitwise_xor(parser);

        ast_node *binary = create_node(NODE_BINARY_OP);
        binary->value = string_duplicate(op.value, strlen(op.value));
        add_child(binary, expr);
        add_child(binary, right);
        expr = binary;
    }

    return expr;
}

ast_node *parse_bitwise_xor(struct parser *parser) {
    ast_node *expr = parse_bitwise_and(parser);

    while (match(parser, TOKEN_BITWISE_XOR)) {
        token op = previous_token(parser);
        ast_node *right = parse_bitwise_and(parser);

        ast_node *binary = create_node(NODE_BINARY_OP);
        binary->value = string_duplicate(op.value, strlen(op.value));
        add_child(binary, expr);
        add_child(binary, right);
        expr = binary;
    }

    return expr;
}

ast_node *parse_bitwise_and(struct parser *parser) {
    ast_node *expr = parse_equality(parser);

    while (match(parser, TOKEN_BITWISE_AND)) {
        token op = previous_token(parser);
        ast_node *right = parse_equality(parser);

        ast_node *binary = create_node(NODE_BINARY_OP);
        binary->value = string_duplicate(op.value, strlen(op.value));
        add_child(binary, expr);
        add_child(binary, right);
        expr = binary;
    }

    return expr;
}

ast_node *parse_equality(struct parser *parser) {
    ast_node *expr = parse_relational(parser);

    while (match(parser, TOKEN_EQUAL) || match(parser, TOKEN_NOT_EQUAL)) {
        token op = previous_token(parser);
        ast_node *right = parse_relational(parser);

        ast_node *binary = create_node(NODE_BINARY_OP);
        binary->value = string_duplicate(op.value, strlen(op.value));
        add_child(binary, expr);
        add_child(binary, right);
        expr = binary;
    }

    return expr;
}

ast_node *parse_relational(struct parser *parser) {
    ast_node *expr = parse_shift(parser);

    while (match(parser, TOKEN_LESS_THAN) || match(parser, TOKEN_GREATER_THAN) || match(parser, TOKEN_LESS_EQUAL) ||
           match(parser, TOKEN_GREATER_EQUAL)) {
        token op = previous_token(parser);
        ast_node *right = parse_shift(parser);

        ast_node *binary = create_node(NODE_BINARY_OP);
        binary->value = string_duplicate(op.value, strlen(op.value));
        add_child(binary, expr);
        add_child(binary, right);
        expr = binary;
    }

    return expr;
}

ast_node *parse_shift(struct parser *parser) {
    ast_node *expr = parse_additive(parser);

    while (match(parser, TOKEN_LEFT_SHIFT) || match(parser, TOKEN_RIGHT_SHIFT)) {
        token op = previous_token(parser);
        ast_node *right = parse_additive(parser);

        ast_node *binary = create_node(NODE_BINARY_OP);
        binary->value = string_duplicate(op.value, strlen(op.value));
        add_child(binary, expr);
        add_child(binary, right);
        expr = binary;
    }

    return expr;
}

ast_node *parse_additive(struct parser *parser) {
    ast_node *expr = parse_multiplicative(parser);

    while (match(parser, TOKEN_PLUS) || match(parser, TOKEN_MINUS)) {
        token op = previous_token(parser);
        ast_node *right = parse_multiplicative(parser);

        ast_node *binary = create_node(NODE_BINARY_OP);
        binary->value = string_duplicate(op.value, strlen(op.value));
        add_child(binary, expr);
        add_child(binary, right);
        expr = binary;
    }

    return expr;
}

ast_node *parse_multiplicative(struct parser *parser) {
    ast_node *expr = parse_unary(parser);

    while (match(parser, TOKEN_MULTIPLY) || match(parser, TOKEN_DIVIDE) || match(parser, TOKEN_MODULO)) {
        token op = previous_token(parser);
        ast_node *right = parse_unary(parser);

        ast_node *binary = create_node(NODE_BINARY_OP);
        binary->value = string_duplicate(op.value, strlen(op.value));
        add_child(binary, expr);
        add_child(binary, right);
        expr = binary;
    }

    return expr;
}

ast_node *parse_unary(struct parser *parser) {
    if (match(parser, TOKEN_LOGICAL_NOT) || match(parser, TOKEN_BITWISE_NOT) || match(parser, TOKEN_MINUS) ||
        match(parser, TOKEN_PLUS) || match(parser, TOKEN_MULTIPLY) || match(parser, TOKEN_BITWISE_AND) ||
        match(parser, TOKEN_INCREMENT) || match(parser, TOKEN_DECREMENT)) {

        token op = previous_token(parser);
        ast_node *expr = parse_unary(parser);

        ast_node *unary = create_node(NODE_UNARY_OP);
        unary->value = string_duplicate(op.value, strlen(op.value));
        add_child(unary, expr);
        return unary;
    }

    if (match(parser, TOKEN_SIZEOF)) {
        consume(parser, TOKEN_LEFT_PAREN);
        ast_node *expr = parse_expression(parser);
        consume(parser, TOKEN_RIGHT_PAREN);

        ast_node *sizeof_node = create_node(NODE_SIZEOF);
        add_child(sizeof_node, expr);
        return sizeof_node;
    }

    return parse_postfix(parser);
}

ast_node *parse_postfix(struct parser *parser) {
    ast_node *expr = parse_primary(parser);

    while (true) {
        if (match(parser, TOKEN_LEFT_BRACKET)) {
            // Array access
            ast_node *index = parse_expression(parser);
            consume(parser, TOKEN_RIGHT_BRACKET);

            ast_node *array_access = create_node(NODE_ARRAY_ACCESS);
            add_child(array_access, expr);
            add_child(array_access, index);
            expr = array_access;
        } else if (match(parser, TOKEN_LEFT_PAREN)) {
            // Function call
            ast_node *call = create_node(NODE_FUNCTION_CALL);
            call->value = string_duplicate(expr->value ? expr->value : "", expr->value ? strlen(expr->value) : 0);

            // Parse arguments
            if (!check(parser, TOKEN_RIGHT_PAREN)) {
                do {
                    ast_node *arg = parse_expression(parser);
                    if (arg) {
                        add_child(call, arg);
                    }
                } while (match(parser, TOKEN_COMMA));
            }
            consume(parser, TOKEN_RIGHT_PAREN);

            free_node(expr);
            expr = call;
        } else if (match(parser, TOKEN_DOT) || match(parser, TOKEN_ARROW)) {
            // Member access
            token op = previous_token(parser);

            if (!check(parser, TOKEN_IDENTIFIER)) {
                parser_error_at_current(parser, "Expected member name");
                break;
            }

            char *member_name = string_duplicate(parser->current_token.value, strlen(parser->current_token.value));
            parser_advance(parser);

            ast_node *member_access = create_node(NODE_MEMBER_ACCESS);
            member_access->value = string_duplicate(op.value, strlen(op.value));
            add_child(member_access, expr);

            ast_node *member = create_node(NODE_IDENTIFIER);
            member->value = member_name;
            add_child(member_access, member);

            expr = member_access;
        } else if (match(parser, TOKEN_INCREMENT) || match(parser, TOKEN_DECREMENT)) {
            // Postfix increment/decrement
            token op = previous_token(parser);

            ast_node *postfix = create_node(NODE_POSTFIX_OP);
            postfix->value = string_duplicate(op.value, strlen(op.value));
            add_child(postfix, expr);
            expr = postfix;
        } else {
            break;
        }
    }

    return expr;
}

ast_node *parse_primary(struct parser *parser) {
    if (match(parser, TOKEN_TRUE) || match(parser, TOKEN_FALSE)) {
        token bool_token = previous_token(parser);
        ast_node *node = create_node(NODE_BOOL_LITERAL);
        node->value = string_duplicate(bool_token.value, strlen(bool_token.value));
        return node;
    }

    if (match(parser, TOKEN_NULL)) {
        ast_node *node = create_node(NODE_NULL_LITERAL);
        node->value = string_duplicate("null", 4);
        return node;
    }

    if (match(parser, TOKEN_NUMBER)) {
        token num_token = previous_token(parser);
        ast_node *node = create_node(NODE_NUMBER_LITERAL);
        node->value = string_duplicate(num_token.value, strlen(num_token.value));
        return node;
    }

    if (match(parser, TOKEN_FLOAT)) {
        token float_token = previous_token(parser);
        ast_node *node = create_node(NODE_FLOAT_LITERAL);
        node->value = string_duplicate(float_token.value, strlen(float_token.value));
        return node;
    }

    if (match(parser, TOKEN_STRING)) {
        token str_token = previous_token(parser);
        ast_node *node = create_node(NODE_STRING_LITERAL);
        node->value = string_duplicate(str_token.value, strlen(str_token.value));
        return node;
    }

    if (match(parser, TOKEN_CHAR)) {
        token char_token = previous_token(parser);
        ast_node *node = create_node(NODE_CHAR_LITERAL);
        node->value = string_duplicate(char_token.value, strlen(char_token.value));
        return node;
    }

    if (match(parser, TOKEN_IDENTIFIER)) {
        token id_token = previous_token(parser);
        ast_node *node = create_node(NODE_IDENTIFIER);
        node->value = string_duplicate(id_token.value, strlen(id_token.value));
        return node;
    }

    if (match(parser, TOKEN_LEFT_PAREN)) {
        ast_node *expr = parse_expression(parser);
        consume(parser, TOKEN_RIGHT_PAREN);
        return expr;
    }

    parser_error_at_current(parser, "Expected expression");
    return NULL;
}

// Parameter and argument parsing
ast_node *parse_parameter_list(struct parser *parser) {
    ast_node *param_list = create_node(NODE_PARAMETER_LIST);

    if (check(parser, TOKEN_RIGHT_PAREN)) {
        return param_list; // Empty parameter list
    }

    do {
        ast_node *type_node = parse_type(parser);
        if (!type_node) break;

        char *param_name = NULL;
        if (check(parser, TOKEN_IDENTIFIER)) {
            param_name = string_duplicate(parser->current_token.value, strlen(parser->current_token.value));
            parser_advance(parser);
        }

        ast_node *param = create_node(NODE_PARAMETER);
        param->value = param_name;
        add_child(param, type_node);
        add_child(param_list, param);

    } while (match(parser, TOKEN_COMMA));

    return param_list;
}

// Module system parsing
ast_node *parse_import_statement(struct parser *parser) {
    if (!check(parser, TOKEN_STRING) && !check(parser, TOKEN_IDENTIFIER)) {
        parser_error_at_current(parser, "Expected module name");
        return NULL;
    }

    char *module_name = string_duplicate(parser->current_token.value, strlen(parser->current_token.value));
    parser_advance(parser);

    ast_node *import_node = create_node(NODE_IMPORT);
    import_node->value = module_name;

    consume(parser, TOKEN_SEMICOLON);
    return import_node;
}

ast_node *parse_export_statement(struct parser *parser) {
    ast_node *export_node = create_node(NODE_EXPORT);

    // Parse what's being exported
    ast_node *exported_item = parse_declaration(parser);
    if (exported_item) {
        add_child(export_node, exported_item);
    }

    return export_node;
}

ast_node *parse_module_declaration(struct parser *parser) {
    if (!check(parser, TOKEN_IDENTIFIER)) {
        parser_error_at_current(parser, "Expected module name");
        return NULL;
    }

    char *module_name = string_duplicate(parser->current_token.value, strlen(parser->current_token.value));
    parser_advance(parser);

    ast_node *module_node = create_node(NODE_MODULE);
    module_node->value = module_name;

    consume(parser, TOKEN_SEMICOLON);
    return module_node;
}

// Switch statement parsing (placeholder for now)
ast_node *parse_switch_statement(struct parser *parser) {
    consume(parser, TOKEN_LEFT_PAREN);
    ast_node *expr = parse_expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN);

    consume(parser, TOKEN_LEFT_BRACE);

    ast_node *switch_node = create_node(NODE_SWITCH_STATEMENT);
    add_child(switch_node, expr);

    // Parse cases
    while (!check(parser, TOKEN_RIGHT_BRACE) && !is_at_end(parser)) {
        if (match(parser, TOKEN_CASE)) {
            ast_node *case_value = parse_expression(parser);
            consume(parser, TOKEN_COLON);

            ast_node *case_node = create_node(NODE_CASE_STATEMENT);
            add_child(case_node, case_value);

            // Parse statements until next case/default/break
            while (!check(parser, TOKEN_CASE) && !check(parser, TOKEN_DEFAULT) && !check(parser, TOKEN_RIGHT_BRACE) &&
                   !is_at_end(parser)) {
                ast_node *stmt = parse_statement(parser);
                if (stmt) {
                    add_child(case_node, stmt);
                }
                if (stmt && stmt->type == NODE_BREAK_STATEMENT) {
                    break;
                }
            }

            add_child(switch_node, case_node);
        } else if (match(parser, TOKEN_DEFAULT)) {
            consume(parser, TOKEN_COLON);

            ast_node *default_node = create_node(NODE_DEFAULT_STATEMENT);

            while (!check(parser, TOKEN_CASE) && !check(parser, TOKEN_RIGHT_BRACE) && !is_at_end(parser)) {
                ast_node *stmt = parse_statement(parser);
                if (stmt) {
                    add_child(default_node, stmt);
                }
                if (stmt && stmt->type == NODE_BREAK_STATEMENT) {
                    break;
                }
            }

            add_child(switch_node, default_node);
        } else {
            parser_error_at_current(parser, "Expected 'case' or 'default'");
            break;
        }
    }

    consume(parser, TOKEN_RIGHT_BRACE);
    return switch_node;
}

ast_node *parse_do_while_statement(struct parser *parser) {
    ast_node *body = parse_statement(parser);

    consume(parser, TOKEN_WHILE);
    consume(parser, TOKEN_LEFT_PAREN);
    ast_node *condition = parse_expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN);
    consume(parser, TOKEN_SEMICOLON);

    ast_node *do_while = create_node(NODE_DO_WHILE_STATEMENT);
    add_child(do_while, body);
    add_child(do_while, condition);

    return do_while;
}

// Error handling and utility functions
void parser_error(struct parser *parser, const char *message) {
    parser_error_at_current(parser, message);
}

void parser_error_at_current(struct parser *parser, const char *message) {
    if (parser->panic_mode) return;

    parser->panic_mode = true;
    parser->error_count++;

    fprintf(stderr, "[Line %d, Column %d] Error", parser->current_token.line, parser->current_token.column);

    if (parser->current_token.type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (parser->current_token.type == TOKEN_INVALID) {
        // Error already reported by lexer
    } else {
        fprintf(stderr, " at '%s'", parser->current_token.value);
    }

    fprintf(stderr, ": %s\n", message);
}

void parser_error_at_previous(struct parser *parser, const char *message) {
    if (parser->panic_mode) return;

    parser->panic_mode = true;
    parser->error_count++;

    // This would need access to previous token - simplified for now
    fprintf(stderr, "Error: %s\n", message);
}

void synchronize(struct parser *parser) {
    parser->panic_mode = false;

    while (parser->current_token.type != TOKEN_EOF) {
        if (previous_token(parser).type == TOKEN_SEMICOLON) return;

        switch (parser->current_token.type) {
        case TOKEN_STRUCT:
        case TOKEN_ENUM:
        case TOKEN_UNION:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_RETURN:
            return;
        default:
            break;
        }

        parser_advance(parser);
    }
}

token previous_token(struct parser *parser) {
    // This is a simplified implementation
    // In a real parser, you'd maintain a history of tokens
    token empty_token = {TOKEN_INVALID, NULL, 0, 0};
    return empty_token;
}

void reset_parser_state(struct parser *parser) {
    parser->error_count = 0;
    parser->panic_mode = false;
}
