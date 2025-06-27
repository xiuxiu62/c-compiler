#pragma once

enum token_type {
    // Literals
    TOKEN_NUMBER,
    TOKEN_FLOAT,
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_CHAR,
    TOKEN_BOOL,

    // Primitive Types
    TOKEN_I8,
    TOKEN_I16,
    TOKEN_I32,
    TOKEN_I64,
    TOKEN_U8,
    TOKEN_U16,
    TOKEN_U32,
    TOKEN_U64,
    TOKEN_F32,
    TOKEN_F64,
    TOKEN_BOOL_TYPE,
    TOKEN_VOID,

    // Keywords
    TOKEN_STRUCT,
    TOKEN_ENUM,
    TOKEN_UNION,
    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_DO,
    TOKEN_SWITCH,
    TOKEN_CASE,
    TOKEN_DEFAULT,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_CONST,
    TOKEN_STATIC,
    TOKEN_EXTERN,
    TOKEN_SIZEOF,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,

    // Module system
    TOKEN_IMPORT,
    TOKEN_EXPORT,
    TOKEN_MODULE,

    // Operators
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_MODULO,
    TOKEN_ASSIGN,
    TOKEN_PLUS_ASSIGN,
    TOKEN_MINUS_ASSIGN,
    TOKEN_MULTIPLY_ASSIGN,
    TOKEN_DIVIDE_ASSIGN,
    TOKEN_MODULO_ASSIGN,
    TOKEN_INCREMENT,
    TOKEN_DECREMENT,

    // Comparison
    TOKEN_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_LESS_THAN,
    TOKEN_GREATER_THAN,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,

    // Logical
    TOKEN_LOGICAL_AND,
    TOKEN_LOGICAL_OR,
    TOKEN_LOGICAL_NOT,

    // Bitwise
    TOKEN_BITWISE_AND,
    TOKEN_BITWISE_OR,
    TOKEN_BITWISE_XOR,
    TOKEN_BITWISE_NOT,
    TOKEN_LEFT_SHIFT,
    TOKEN_RIGHT_SHIFT,

    // Memory
    TOKEN_ADDRESS_OF,
    TOKEN_DEREFERENCE,
    TOKEN_ARROW,
    TOKEN_DOT,

    // Punctuation
    TOKEN_SEMICOLON,
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_QUESTION,

    // Special
    TOKEN_EOF,
    TOKEN_INVALID,
    TOKEN_NEWLINE,
};

struct token {
    token_type type;
    char *value;
    int line;
    int column;
    union {
        long long int_value;
        double float_value;
        bool bool_value;
    } literal;
};

struct lexer {
    const char *source;
    int position;
    int line;
    int column;
    int length;
    bool track_newlines; // For potential significant whitespace
};

// Core lexer functions
lexer create_lexer(const char *source);
void advance(struct lexer *lexer);
void skip_whitespace(struct lexer *lexer);
token next_token(struct lexer *lexer);

// Token reading functions
token read_number(struct lexer *lexer, int start_col);
token read_float(struct lexer *lexer, int start_col);
token read_string(struct lexer *lexer, int start_col);
token read_char(struct lexer *lexer, int start_col);
token read_identifier(struct lexer *lexer, int start_col);

// Helper functions
token_type get_keyword_type(const char *value);
bool is_keyword(const char *value);
bool is_primitive_type(token_type type);
bool is_operator(token_type type);
bool is_literal(token_type type);

// Utility functions
const char *token_type_name(token_type type);
void free_token(token *tok);
token copy_token(const token *tok);

// Error handling
void lexer_error(struct lexer *lexer, const char *message);
bool is_valid_identifier_start(char c);
bool is_valid_identifier_char(char c);
