#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

#ifndef _TYPES_H
#define _TYPES_H
#define CURR this->source[this->pos]
#define MAX(a,b) (a>b)?a:b
#define SVCMP(sv, b) strncmp(b, sv.data, MAX(sv.size, strlen(b)))
#define SVSVCMP(sv, b) strncmp(b.data, sv.data, MAX(sv.size, b.size))
#define SVVARG(sv) (int)sv.size, sv.data
#define SVTOL(sv) strtol(sv.data, NULL, 10)
#define TOKENERROR(error) { \
    printloc(token.loc); \
    printf(error "'%.*s'\n", SVVARG(token.sv)); \
    exit(1); \
}
enum TypeEnum {
    TYPE_NOT_A_TYPE,
    TYPE_STRING,
    TYPE_I8,
    TYPE_I32,
    TYPE_I64,
    TYPE_U8,
    TYPE_U32,
    TYPE_U64,
};

char *TYPE_TO_STR[]={
    [TYPE_I8     ] = "i8",
    [TYPE_I32    ] = "i32",
    [TYPE_I64    ] = "i64",
    [TYPE_STRING ] = "string"
};

typedef struct {
    bool returned; 
    int64_t value;
} CBReturn;

enum TokenEnum {
    TOKEN_FN_DECL,
    TOKEN_NAME,
    TOKEN_FN_CALL,
    TOKEN_OCURLY,
    TOKEN_CCURLY,
    TOKEN_OPAREN,
    TOKEN_CPAREN,
    TOKEN_OSQUAR,
    TOKEN_CSQUAR,
    TOKEN_STR_LITERAL,
    TOKEN_SEMICOLON,
    TOKEN_RETURN,
    TOKEN_RETURN_COLON,
    TOKEN_COMMA,
    TOKEN_NUMERIC,
    TOKEN_EQUAL_SIGN,
    TOKEN_OP_DIV,
    TOKEN_OP_PLUS,
    TOKEN_OP_MINUS,
    TOKEN_OP_MUL,
    TOKEN_OP_LESS,
    TOKEN_OP_GREATER,
    TOKEN_OP_NOT,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE
};

char *TOKEN_TO_STR[] = {
    [TOKEN_FN_DECL      ] = "TOKEN_FN_DECL ",
    [TOKEN_NAME         ] = "TOKEN_NAME",
    [TOKEN_FN_CALL      ] = "TOKEN_FN_CALL ",
    [TOKEN_OCURLY       ] = "TOKEN_OCURLY",
    [TOKEN_CCURLY       ] = "TOKEN_CCURLY",
    [TOKEN_OPAREN       ] = "TOKEN_OPAREN",
    [TOKEN_CPAREN       ] = "TOKEN_CPAREN",
    [TOKEN_STR_LITERAL  ] = "TOKEN_STR_LITERAL ",
    [TOKEN_SEMICOLON    ] = "TOKEN_SEMICOLON ",
    [TOKEN_RETURN       ] = "TOKEN_RETURN",
    [TOKEN_RETURN_COLON ] = "TOKEN_RETURN_COLON",
    [TOKEN_COMMA        ] = "TOKEN_COMMA ",
    [TOKEN_NUMERIC      ] = "TOKEN_NUMERIC ",
    [TOKEN_EQUAL_SIGN   ] = "TOKEN_EQUAL_SIGN",
    [TOKEN_OP_DIV       ] = "TOKEN_OP_DIV",
    [TOKEN_OP_PLUS      ] = "TOKEN_OP_PLUS ",
    [TOKEN_OP_MINUS     ] = "TOKEN_OP_MINUS",
    [TOKEN_OP_MUL       ] = "TOKEN_OP_MUL",
    [TOKEN_OP_LESS      ] = "TOKEN_OP_LESS ",
    [TOKEN_OP_GREATER   ] = "TOKEN_OP_GREATER",
    [TOKEN_OP_NOT       ] = "TOKEN_OP_NOT",
    [TOKEN_IF           ] = "TOKEN_IF",
    [TOKEN_ELSE         ] = "TOKEN_ELSE",
    [TOKEN_WHILE        ] = "TOKEN_WHILE",
    [TOKEN_OSQUAR       ] = "TOKEN_OSQUAR",
    [TOKEN_CSQUAR       ] = "TOKEN_CSQUAR"
};

enum ModifyerEnum {
    MOD_NO_MOD,
    MOD_PTR,
    MOD_ARRAY
};

char *MOD_TO_STR[] = {
    [MOD_NO_MOD ] = "MOD_NO_MOD",
    [MOD_ARRAY  ] = "MOD_ARRAY",
    [MOD_PTR    ] = "MOD_PTR"
};

typedef struct {
    char *data;
    size_t size;
} SView;

typedef struct {
    char *file_name;
    char *source;
    size_t line;
    size_t pos;
    size_t bol;
} Lexer;

typedef struct {
    char *file_path;
    size_t row;
    size_t col;
} Location;

typedef struct {
    enum TokenEnum type;
    SView sv;
    Location loc;
} Token;

typedef struct {
    SView name;
    enum TypeEnum type;
} Var_signature;

typedef struct {
    SView name;
    enum TypeEnum type;
    void *ptr;
    enum ModifyerEnum modifyer;
    size_t size;
} Variable;

typedef struct {
    Variable *variables;
    size_t varc;
} Variables;

typedef struct {
    char *name;
    Variable args[42];
} Funcall;

typedef struct {
    Token *code;
    Variables *variables;
    size_t exprc;
    int depth;
} CodeBlock;

typedef struct {
    SView name;
    enum TypeEnum ret_type;
    Var_signature *args;
    size_t argc;
    CodeBlock body;
} Func;

typedef struct {
    size_t return_token_id;
    size_t return_function_id;
} CallStack;

#endif 
