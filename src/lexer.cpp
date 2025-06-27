#include "lexer.hpp"
#include "ctype.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "util.hpp"

// Lexer functions
lexer create_lexer(const char *source) {
    struct lexer lexer;
    lexer.source = source;
    lexer.position = 0;
    lexer.line = 1;
    lexer.column = 1;
    lexer.length = strlen(source);
    lexer.track_newlines = false;
    return lexer;
}

void advance(struct lexer *lexer) {
    if (lexer->position < lexer->length) {
        if (lexer->source[lexer->position] == '\n') {
            lexer->line++;
            lexer->column = 1;
        } else {
            lexer->column++;
        }
        lexer->position++;
    }
}

char peek(struct lexer *lexer, int offset) {
    int pos = lexer->position + offset;
    if (pos >= lexer->length) return '\0';
    return lexer->source[pos];
}

void skip_whitespace(struct lexer *lexer) {
    while (lexer->position < lexer->length) {
        char c = lexer->source[lexer->position];
        if (c == ' ' || c == '\t' || c == '\r') {
            advance(lexer);
        } else if (c == '\n') {
            if (lexer->track_newlines) {
                break; // Don't skip newlines if they're significant
            }
            advance(lexer);
        } else {
            break;
        }
    }
}

void skip_line_comment(struct lexer *lexer) {
    // Skip //
    advance(lexer);
    advance(lexer);

    while (lexer->position < lexer->length && lexer->source[lexer->position] != '\n') {
        advance(lexer);
    }
}

void skip_block_comment(struct lexer *lexer) {
    // Skip /*
    advance(lexer);
    advance(lexer);

    while (lexer->position < lexer->length - 1) {
        if (lexer->source[lexer->position] == '*' && lexer->source[lexer->position + 1] == '/') {
            advance(lexer);
            advance(lexer);
            break;
        }
        advance(lexer);
    }
}

token read_number(struct lexer *lexer, int start_col) {
    int start_pos = lexer->position;
    bool is_float = false;

    // Read integer part
    while (lexer->position < lexer->length && isdigit(lexer->source[lexer->position])) {
        advance(lexer);
    }

    // Check for decimal point
    if (lexer->position < lexer->length && lexer->source[lexer->position] == '.') {
        is_float = true;
        advance(lexer);

        // Read fractional part
        while (lexer->position < lexer->length && isdigit(lexer->source[lexer->position])) {
            advance(lexer);
        }
    }

    // Check for scientific notation
    if (lexer->position < lexer->length &&
        (lexer->source[lexer->position] == 'e' || lexer->source[lexer->position] == 'E')) {
        is_float = true;
        advance(lexer);

        if (lexer->position < lexer->length &&
            (lexer->source[lexer->position] == '+' || lexer->source[lexer->position] == '-')) {
            advance(lexer);
        }

        while (lexer->position < lexer->length && isdigit(lexer->source[lexer->position])) {
            advance(lexer);
        }
    }

    struct token token;
    token.type = is_float ? TOKEN_FLOAT : TOKEN_NUMBER;
    token.value = string_duplicate(lexer->source + start_pos, lexer->position - start_pos);
    token.line = lexer->line;
    token.column = start_col;

    if (is_float) {
        token.literal.float_value = atof(token.value);
    } else {
        token.literal.int_value = atoll(token.value);
    }

    return token;
}

token read_string(struct lexer *lexer, int start_col) {
    advance(lexer); // skip opening quote
    int start_pos = lexer->position;

    while (lexer->position < lexer->length && lexer->source[lexer->position] != '"') {
        if (lexer->source[lexer->position] == '\\') {
            advance(lexer); // skip escape character
            if (lexer->position < lexer->length) {
                advance(lexer); // skip escaped character
            }
        } else {
            advance(lexer);
        }
    }

    char *value = string_duplicate(lexer->source + start_pos, lexer->position - start_pos);

    if (lexer->position < lexer->length) {
        advance(lexer); // skip closing quote
    }

    struct token token;
    token.type = TOKEN_STRING;
    token.value = value;
    token.line = lexer->line;
    token.column = start_col;
    return token;
}

token read_char(struct lexer *lexer, int start_col) {
    advance(lexer); // skip opening quote
    char char_value = 0;

    if (lexer->position < lexer->length && lexer->source[lexer->position] != '\'') {
        if (lexer->source[lexer->position] == '\\') {
            advance(lexer);
            if (lexer->position < lexer->length) {
                char c = lexer->source[lexer->position];
                switch (c) {
                case 'n':
                    char_value = '\n';
                    break;
                case 't':
                    char_value = '\t';
                    break;
                case 'r':
                    char_value = '\r';
                    break;
                case '\\':
                    char_value = '\\';
                    break;
                case '\'':
                    char_value = '\'';
                    break;
                case '0':
                    char_value = '\0';
                    break;
                default:
                    char_value = c;
                    break;
                }
                advance(lexer);
            }
        } else {
            char_value = lexer->source[lexer->position];
            advance(lexer);
        }
    }

    if (lexer->position < lexer->length && lexer->source[lexer->position] == '\'') {
        advance(lexer); // skip closing quote
    }

    struct token token;
    token.type = TOKEN_CHAR;
    char *value = (char *)malloc(2);
    value[0] = char_value;
    value[1] = '\0';
    token.value = value;
    token.line = lexer->line;
    token.column = start_col;
    token.literal.int_value = char_value;
    return token;
}

token_type get_keyword_type(const char *value) {
    // Primitive types
    if (strcmp(value, "i8") == 0) return TOKEN_I8;
    if (strcmp(value, "i16") == 0) return TOKEN_I16;
    if (strcmp(value, "i32") == 0) return TOKEN_I32;
    if (strcmp(value, "i64") == 0) return TOKEN_I64;
    if (strcmp(value, "u8") == 0) return TOKEN_U8;
    if (strcmp(value, "u16") == 0) return TOKEN_U16;
    if (strcmp(value, "u32") == 0) return TOKEN_U32;
    if (strcmp(value, "u64") == 0) return TOKEN_U64;
    if (strcmp(value, "f32") == 0) return TOKEN_F32;
    if (strcmp(value, "f64") == 0) return TOKEN_F64;
    if (strcmp(value, "bool") == 0) return TOKEN_BOOL_TYPE;
    if (strcmp(value, "void") == 0) return TOKEN_VOID;

    // Keywords
    if (strcmp(value, "struct") == 0) return TOKEN_STRUCT;
    if (strcmp(value, "enum") == 0) return TOKEN_ENUM;
    if (strcmp(value, "union") == 0) return TOKEN_UNION;
    if (strcmp(value, "return") == 0) return TOKEN_RETURN;
    if (strcmp(value, "if") == 0) return TOKEN_IF;
    if (strcmp(value, "else") == 0) return TOKEN_ELSE;
    if (strcmp(value, "while") == 0) return TOKEN_WHILE;
    if (strcmp(value, "for") == 0) return TOKEN_FOR;
    if (strcmp(value, "do") == 0) return TOKEN_DO;
    if (strcmp(value, "switch") == 0) return TOKEN_SWITCH;
    if (strcmp(value, "case") == 0) return TOKEN_CASE;
    if (strcmp(value, "default") == 0) return TOKEN_DEFAULT;
    if (strcmp(value, "break") == 0) return TOKEN_BREAK;
    if (strcmp(value, "continue") == 0) return TOKEN_CONTINUE;
    if (strcmp(value, "const") == 0) return TOKEN_CONST;
    if (strcmp(value, "static") == 0) return TOKEN_STATIC;
    if (strcmp(value, "extern") == 0) return TOKEN_EXTERN;
    if (strcmp(value, "sizeof") == 0) return TOKEN_SIZEOF;

    // Boolean literals
    if (strcmp(value, "true") == 0) return TOKEN_TRUE;
    if (strcmp(value, "false") == 0) return TOKEN_FALSE;
    if (strcmp(value, "null") == 0) return TOKEN_NULL;

    // Module system
    if (strcmp(value, "import") == 0) return TOKEN_IMPORT;
    if (strcmp(value, "export") == 0) return TOKEN_EXPORT;
    if (strcmp(value, "module") == 0) return TOKEN_MODULE;

    return TOKEN_IDENTIFIER;
}

bool is_valid_identifier_start(char c) {
    return isalpha(c) || c == '_';
}

bool is_valid_identifier_char(char c) {
    return isalnum(c) || c == '_';
}

token read_identifier(struct lexer *lexer, int start_col) {
    int start_pos = lexer->position;

    while (lexer->position < lexer->length && is_valid_identifier_char(lexer->source[lexer->position])) {
        advance(lexer);
    }

    char *value = string_duplicate(lexer->source + start_pos, lexer->position - start_pos);
    token_type type = get_keyword_type(value);

    struct token token;
    token.type = type;
    token.value = value;
    token.line = lexer->line;
    token.column = start_col;

    // Set literal value for boolean keywords
    if (type == TOKEN_TRUE) {
        token.literal.bool_value = true;
    } else if (type == TOKEN_FALSE) {
        token.literal.bool_value = false;
    }

    return token;
}

token next_token(struct lexer *lexer) {
    skip_whitespace(lexer);

    if (lexer->position >= lexer->length) {
        struct token token;
        token.type = TOKEN_EOF;
        token.value = string_duplicate("", 0);
        token.line = lexer->line;
        token.column = lexer->column;
        return token;
    }

    int start_col = lexer->column;
    char c = lexer->source[lexer->position];
    char next_c = peek(lexer, 1);

    struct token token;
    token.line = lexer->line;
    token.column = start_col;

    // Comments
    if (c == '/' && next_c == '/') {
        skip_line_comment(lexer);
        return next_token(lexer);
    }

    if (c == '/' && next_c == '*') {
        skip_block_comment(lexer);
        return next_token(lexer);
    }

    // Two-character operators
    if (c == '+' && next_c == '+') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_INCREMENT;
        token.value = string_duplicate("++", 2);
        return token;
    }

    if (c == '-' && next_c == '-') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_DECREMENT;
        token.value = string_duplicate("--", 2);
        return token;
    }

    if (c == '+' && next_c == '=') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_PLUS_ASSIGN;
        token.value = string_duplicate("+=", 2);
        return token;
    }

    if (c == '-' && next_c == '=') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_MINUS_ASSIGN;
        token.value = string_duplicate("-=", 2);
        return token;
    }

    if (c == '*' && next_c == '=') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_MULTIPLY_ASSIGN;
        token.value = string_duplicate("*=", 2);
        return token;
    }

    if (c == '/' && next_c == '=') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_DIVIDE_ASSIGN;
        token.value = string_duplicate("/=", 2);
        return token;
    }

    if (c == '%' && next_c == '=') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_MODULO_ASSIGN;
        token.value = string_duplicate("%=", 2);
        return token;
    }

    if (c == '=' && next_c == '=') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_EQUAL;
        token.value = string_duplicate("==", 2);
        return token;
    }

    if (c == '!' && next_c == '=') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_NOT_EQUAL;
        token.value = string_duplicate("!=", 2);
        return token;
    }

    if (c == '<' && next_c == '=') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_LESS_EQUAL;
        token.value = string_duplicate("<=", 2);
        return token;
    }

    if (c == '>' && next_c == '=') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_GREATER_EQUAL;
        token.value = string_duplicate(">=", 2);
        return token;
    }

    if (c == '&' && next_c == '&') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_LOGICAL_AND;
        token.value = string_duplicate("&&", 2);
        return token;
    }

    if (c == '|' && next_c == '|') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_LOGICAL_OR;
        token.value = string_duplicate("||", 2);
        return token;
    }

    if (c == '<' && next_c == '<') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_LEFT_SHIFT;
        token.value = string_duplicate("<<", 2);
        return token;
    }

    if (c == '>' && next_c == '>') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_RIGHT_SHIFT;
        token.value = string_duplicate(">>", 2);
        return token;
    }

    if (c == '-' && next_c == '>') {
        advance(lexer);
        advance(lexer);
        token.type = TOKEN_ARROW;
        token.value = string_duplicate("->", 2);
        return token;
    }

    // Single character tokens
    switch (c) {
    case '"':
        return read_string(lexer, start_col);
    case '\'':
        return read_char(lexer, start_col);
    case '+':
        advance(lexer);
        token.type = TOKEN_PLUS;
        token.value = string_duplicate("+", 1);
        break;
    case '-':
        advance(lexer);
        token.type = TOKEN_MINUS;
        token.value = string_duplicate("-", 1);
        break;
    case '*':
        advance(lexer);
        token.type = TOKEN_MULTIPLY;
        token.value = string_duplicate("*", 1);
        break;
    case '/':
        advance(lexer);
        token.type = TOKEN_DIVIDE;
        token.value = string_duplicate("/", 1);
        break;
    case '%':
        advance(lexer);
        token.type = TOKEN_MODULO;
        token.value = string_duplicate("%", 1);
        break;
    case '=':
        advance(lexer);
        token.type = TOKEN_ASSIGN;
        token.value = string_duplicate("=", 1);
        break;
    case '<':
        advance(lexer);
        token.type = TOKEN_LESS_THAN;
        token.value = string_duplicate("<", 1);
        break;
    case '>':
        advance(lexer);
        token.type = TOKEN_GREATER_THAN;
        token.value = string_duplicate(">", 1);
        break;
    case '!':
        advance(lexer);
        token.type = TOKEN_LOGICAL_NOT;
        token.value = string_duplicate("!", 1);
        break;
    case '&':
        advance(lexer);
        token.type = TOKEN_BITWISE_AND;
        token.value = string_duplicate("&", 1);
        break;
    case '|':
        advance(lexer);
        token.type = TOKEN_BITWISE_OR;
        token.value = string_duplicate("|", 1);
        break;
    case '^':
        advance(lexer);
        token.type = TOKEN_BITWISE_XOR;
        token.value = string_duplicate("^", 1);
        break;
    case '~':
        advance(lexer);
        token.type = TOKEN_BITWISE_NOT;
        token.value = string_duplicate("~", 1);
        break;
    case '.':
        advance(lexer);
        token.type = TOKEN_DOT;
        token.value = string_duplicate(".", 1);
        break;
    case ';':
        advance(lexer);
        token.type = TOKEN_SEMICOLON;
        token.value = string_duplicate(";", 1);
        break;
    case ':':
        advance(lexer);
        token.type = TOKEN_COLON;
        token.value = string_duplicate(":", 1);
        break;
    case ',':
        advance(lexer);
        token.type = TOKEN_COMMA;
        token.value = string_duplicate(",", 1);
        break;
    case '(':
        advance(lexer);
        token.type = TOKEN_LEFT_PAREN;
        token.value = string_duplicate("(", 1);
        break;
    case ')':
        advance(lexer);
        token.type = TOKEN_RIGHT_PAREN;
        token.value = string_duplicate(")", 1);
        break;
    case '{':
        advance(lexer);
        token.type = TOKEN_LEFT_BRACE;
        token.value = string_duplicate("{", 1);
        break;
    case '}':
        advance(lexer);
        token.type = TOKEN_RIGHT_BRACE;
        token.value = string_duplicate("}", 1);
        break;
    case '[':
        advance(lexer);
        token.type = TOKEN_LEFT_BRACKET;
        token.value = string_duplicate("[", 1);
        break;
    case ']':
        advance(lexer);
        token.type = TOKEN_RIGHT_BRACKET;
        token.value = string_duplicate("]", 1);
        break;
    case '?':
        advance(lexer);
        token.type = TOKEN_QUESTION;
        token.value = string_duplicate("?", 1);
        break;
    case '\n':
        if (lexer->track_newlines) {
            advance(lexer);
            token.type = TOKEN_NEWLINE;
            token.value = string_duplicate("\n", 1);
            break;
        }
        // Fall through if not tracking newlines
    default:
        if (isdigit(c)) {
            return read_number(lexer, start_col);
        } else if (is_valid_identifier_start(c)) {
            return read_identifier(lexer, start_col);
        } else {
            advance(lexer);
            token.type = TOKEN_INVALID;
            char *invalid_char = (char *)malloc(2);
            invalid_char[0] = c;
            invalid_char[1] = '\0';
            token.value = invalid_char;
        }
        break;
    }

    return token;
}

// Helper functions
bool is_keyword(const char *value) {
    return get_keyword_type(value) != TOKEN_IDENTIFIER;
}

bool is_primitive_type(token_type type) {
    return type >= TOKEN_I8 && type <= TOKEN_VOID;
}

bool is_operator(token_type type) {
    return type >= TOKEN_PLUS && type <= TOKEN_DOT;
}

bool is_literal(token_type type) {
    return type == TOKEN_NUMBER || type == TOKEN_FLOAT || type == TOKEN_STRING || type == TOKEN_CHAR ||
           type == TOKEN_TRUE || type == TOKEN_FALSE || type == TOKEN_NULL;
}

void free_token(token *tok) {
    if (tok && tok->value) {
        free(tok->value);
        tok->value = NULL;
    }
}

token copy_token(const token *tok) {
    token new_token = *tok;
    if (tok->value) {
        new_token.value = string_duplicate(tok->value, strlen(tok->value));
    }
    return new_token;
}

void lexer_error(struct lexer *lexer, const char *message) {
    fprintf(stderr, "Lexer error at line %d, column %d: %s\n", lexer->line, lexer->column, message);
}

const char *token_type_name(token_type type) {
    switch (type) {
    // Literals
    case TOKEN_NUMBER:
        return "NUMBER";
    case TOKEN_FLOAT:
        return "FLOAT";
    case TOKEN_IDENTIFIER:
        return "IDENTIFIER";
    case TOKEN_STRING:
        return "STRING";
    case TOKEN_CHAR:
        return "CHAR";
    case TOKEN_BOOL:
        return "BOOL";

    // Primitive types
    case TOKEN_I8:
        return "I8";
    case TOKEN_I16:
        return "I16";
    case TOKEN_I32:
        return "I32";
    case TOKEN_I64:
        return "I64";
    case TOKEN_U8:
        return "U8";
    case TOKEN_U16:
        return "U16";
    case TOKEN_U32:
        return "U32";
    case TOKEN_U64:
        return "U64";
    case TOKEN_F32:
        return "F32";
    case TOKEN_F64:
        return "F64";
    case TOKEN_BOOL_TYPE:
        return "BOOL_TYPE";
    case TOKEN_VOID:
        return "VOID";

    // Keywords
    case TOKEN_STRUCT:
        return "STRUCT";
    case TOKEN_ENUM:
        return "ENUM";
    case TOKEN_UNION:
        return "UNION";
    case TOKEN_RETURN:
        return "RETURN";
    case TOKEN_IF:
        return "IF";
    case TOKEN_ELSE:
        return "ELSE";
    case TOKEN_WHILE:
        return "WHILE";
    case TOKEN_FOR:
        return "FOR";
    case TOKEN_DO:
        return "DO";
    case TOKEN_SWITCH:
        return "SWITCH";
    case TOKEN_CASE:
        return "CASE";
    case TOKEN_DEFAULT:
        return "DEFAULT";
    case TOKEN_BREAK:
        return "BREAK";
    case TOKEN_CONTINUE:
        return "CONTINUE";
    case TOKEN_CONST:
        return "CONST";
    case TOKEN_STATIC:
        return "STATIC";
    case TOKEN_EXTERN:
        return "EXTERN";
    case TOKEN_SIZEOF:
        return "SIZEOF";
    case TOKEN_TRUE:
        return "TRUE";
    case TOKEN_FALSE:
        return "FALSE";
    case TOKEN_NULL:
        return "NULL";

    // Module system
    case TOKEN_IMPORT:
        return "IMPORT";
    case TOKEN_EXPORT:
        return "EXPORT";
    case TOKEN_MODULE:
        return "MODULE";

    // Operators
    case TOKEN_PLUS:
        return "PLUS";
    case TOKEN_MINUS:
        return "MINUS";
    case TOKEN_MULTIPLY:
        return "MULTIPLY";
    case TOKEN_DIVIDE:
        return "DIVIDE";
    case TOKEN_MODULO:
        return "MODULO";
    case TOKEN_ASSIGN:
        return "ASSIGN";
    case TOKEN_PLUS_ASSIGN:
        return "PLUS_ASSIGN";
    case TOKEN_MINUS_ASSIGN:
        return "MINUS_ASSIGN";
    case TOKEN_MULTIPLY_ASSIGN:
        return "MULTIPLY_ASSIGN";
    case TOKEN_DIVIDE_ASSIGN:
        return "DIVIDE_ASSIGN";
    case TOKEN_MODULO_ASSIGN:
        return "MODULO_ASSIGN";
    case TOKEN_INCREMENT:
        return "INCREMENT";
    case TOKEN_DECREMENT:
        return "DECREMENT";

    // Comparison
    case TOKEN_EQUAL:
        return "EQUAL";
    case TOKEN_NOT_EQUAL:
        return "NOT_EQUAL";
    case TOKEN_LESS_THAN:
        return "LESS_THAN";
    case TOKEN_GREATER_THAN:
        return "GREATER_THAN";
    case TOKEN_LESS_EQUAL:
        return "LESS_EQUAL";
    case TOKEN_GREATER_EQUAL:
        return "GREATER_EQUAL";

    // Logical
    case TOKEN_LOGICAL_AND:
        return "LOGICAL_AND";
    case TOKEN_LOGICAL_OR:
        return "LOGICAL_OR";
    case TOKEN_LOGICAL_NOT:
        return "LOGICAL_NOT";

    // Bitwise
    case TOKEN_BITWISE_AND:
        return "BITWISE_AND";
    case TOKEN_BITWISE_OR:
        return "BITWISE_OR";
    case TOKEN_BITWISE_XOR:
        return "BITWISE_XOR";
    case TOKEN_BITWISE_NOT:
        return "BITWISE_NOT";
    case TOKEN_LEFT_SHIFT:
        return "LEFT_SHIFT";
    case TOKEN_RIGHT_SHIFT:
        return "RIGHT_SHIFT";

    // Memory
    case TOKEN_ADDRESS_OF:
        return "ADDRESS_OF";
    case TOKEN_DEREFERENCE:
        return "DEREFERENCE";
    case TOKEN_ARROW:
        return "ARROW";
    case TOKEN_DOT:
        return "DOT";

    // Punctuation
    case TOKEN_SEMICOLON:
        return "SEMICOLON";
    case TOKEN_COLON:
        return "COLON";
    case TOKEN_COMMA:
        return "COMMA";
    case TOKEN_LEFT_PAREN:
        return "LEFT_PAREN";
    case TOKEN_RIGHT_PAREN:
        return "RIGHT_PAREN";
    case TOKEN_LEFT_BRACE:
        return "LEFT_BRACE";
    case TOKEN_RIGHT_BRACE:
        return "RIGHT_BRACE";
    case TOKEN_LEFT_BRACKET:
        return "LEFT_BRACKET";
    case TOKEN_RIGHT_BRACKET:
        return "RIGHT_BRACKET";
    case TOKEN_QUESTION:
        return "QUESTION";

    // Special
    case TOKEN_EOF:
        return "EOF";
    case TOKEN_INVALID:
        return "INVALID";
    case TOKEN_NEWLINE:
        return "NEWLINE";

    default:
        return "UNKNOWN";
    }
}
