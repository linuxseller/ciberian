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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>

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
    [TYPE_I8]="i8",
    [TYPE_I32]="i32",
    [TYPE_I64]="i64",
    [TYPE_STRING]="string"
};

enum TokenEnum {
    TOKEN_FN_DECL,
    TOKEN_NAME,
    TOKEN_FN_CALL,
    TOKEN_OCURLY,
    TOKEN_CCURLY,
    TOKEN_OPAREN,
    TOKEN_CPAREN,
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
[TOKEN_WHILE        ] = "TOKEN_WHILE"
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
} Variable;

typedef struct {
    char *name;
    Variable args[42];
} Funcall;

typedef struct {
    Token *code;
    Variable *variables;
    size_t varc;
    size_t exprc;
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

size_t hash(SView sv){
    size_t hash = 5381;
    int c;
    char *str = sv.data;
    while (sv.size--){
        c = *str++;
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

void usage(char *program_name){
    printf("usage: %s [flags] <filename.cbr>\n", program_name);
    printf("flags:\n");
    printf("\t--verbose : provides additional info\n");
}

char *args_shift(int *argc, char ***argv){
    (*argc)--;
    return *((*argv)++);
}

bool verbose=false;
void chop_char(Lexer *this){
    if(CURR=='\0'){
        return;
    }
    char x = CURR;
    this->pos++;
    if(x=='\n'){
        this->bol = this->pos;
        this->line++;
    }
}

void trim_left(Lexer *this){
    while(isspace(CURR) && CURR!='\0'){
      chop_char(this); 
    }
}

void drop_line(Lexer *this) {
    while(CURR!='\0' && CURR!='\n') {
        chop_char(this);
    }
    if (CURR!='\0') {
        chop_char(this);
    }
}

Token next_token(Lexer *this){
    trim_left(this);
    SView sv = {0};
    char first_char = CURR;
    while(first_char=='#'){
        drop_line(this);
        first_char = CURR;
    }
    Location loc = {.col       = this->pos-this->bol+1,
                    .row       = this->line,
                    .file_path = this->file_name};
    
    sv.data = this->source+this->pos;
    size_t start = this->pos;
    enum TokenEnum token_type;
    if(isalpha(first_char)){
        while(CURR!='\0' && isalnum(CURR)) {
           chop_char(this); 
        }
        sv.size = this->pos - start;
        if(SVCMP(sv, "fn")==0){
            token_type = TOKEN_FN_DECL;
        } else if(SVCMP(sv, "return")==0){
            token_type = TOKEN_RETURN;
        } else if(SVCMP(sv, "while")==0){
            token_type = TOKEN_WHILE;
        } else if(SVCMP(sv, "if")==0){
            token_type = TOKEN_IF;
        } else if(SVCMP(sv, "else")==0){
            token_type = TOKEN_ELSE;
        } else {
            token_type = TOKEN_NAME;
        }
        return (Token){.loc=loc, .sv=sv, .type=token_type};
    }
    if(isdigit(first_char)){
        while(CURR!='\0' && isdigit(CURR)) {
            chop_char(this);
            sv.size = this->pos - start;
        }
            return (Token){.loc=loc, .sv=sv, .type=TOKEN_NUMERIC};
    }
    sv.size=1;
    switch(first_char){
        case '{':
            token_type = TOKEN_OCURLY; break;
        case '}':
            token_type = TOKEN_CCURLY; break;
        case '(':
            token_type = TOKEN_OPAREN; break;
        case ')':
            token_type = TOKEN_CPAREN; break;
        case ';':
            token_type = TOKEN_SEMICOLON; break;
        case ':':
            token_type = TOKEN_RETURN_COLON; break;
        case ',':
            token_type = TOKEN_COMMA; break;
        case '=':
            token_type = TOKEN_EQUAL_SIGN; break;
        case '+':
            token_type = TOKEN_OP_PLUS; break;
        case '-':
            token_type = TOKEN_OP_MINUS; break;
        case '/':
            token_type = TOKEN_OP_DIV; break;
        case '*':
            token_type = TOKEN_OP_MUL; break;
        case '<':
            token_type = TOKEN_OP_LESS; break;
        case '>':
            token_type = TOKEN_OP_GREATER; break;
        case '!':
            token_type = TOKEN_OP_NOT; break;
        case '"': { // parsing string literal
                    chop_char(this);
                    token_type = TOKEN_STR_LITERAL;
                    size_t str_lit_len = strpbrk(this->source+this->pos ,"\"") - (this->source + this->pos);
                    sv.data = calloc(str_lit_len+1, 1);
                    for(int i=0; CURR != '"'; i++){
                        if(CURR=='\\'){
                            chop_char(this);
                            switch(CURR){
                                case 'n':
                                    sv.data[i]='\n'; break;
                                case '\\':
                                    sv.data[i]='\\'; break;
                                case 't':
                                    sv.data[i]='\t'; break;
                                case '"':
                                    sv.data[i]='\"'; break;
                            }
                            chop_char(this);
                            continue;
                        }
                        sv.data[i]=CURR;
                        chop_char(this);
                    }
                    sv.size = str_lit_len;
                    break;
                  } // token string literal 
    }
    chop_char(this);
    return (Token){.loc=loc, .sv=sv, .type=token_type};
}

void printloc(Location loc){
    printf("%s:%lu:%lu", loc.file_path, loc.row, loc.col);
}

enum TypeEnum parse_type(Lexer *lexer){
    Token token = next_token(lexer);
    enum TypeEnum type;
    if(SVCMP(token.sv, "i8")==0){
        type = TYPE_I8;
    } else if(SVCMP(token.sv, "i32")==0){
        type = TYPE_I32;
    } else if(SVCMP(token.sv, "i64")==0){
        type = TYPE_I64;
    } else if(SVCMP(token.sv, "u8")==0){
        type = TYPE_U8;
    } else if(SVCMP(token.sv, "u32")==0){
        type = TYPE_U32;
    } else if(SVCMP(token.sv, "u64")==0){
        type = TYPE_U64;
    } else if(SVCMP(token.sv, "string")==0){
        type = TYPE_STRING;
    } else {
        TOKENERROR(" Error: unknown type ");
    }
    return type; 
}
enum TypeEnum token_variable_type(Token token){
    enum TypeEnum type;
    if(SVCMP(token.sv, "i8")==0){
        type = TYPE_I8;
    } else if(SVCMP(token.sv, "i32")==0){
        type = TYPE_I32;
    } else if(SVCMP(token.sv, "i64")==0){
        type = TYPE_I64;
    } else if(SVCMP(token.sv, "u8")==0){
        type = TYPE_U8;
    } else if(SVCMP(token.sv, "u32")==0){
        type = TYPE_U32;
    } else if(SVCMP(token.sv, "u64")==0){
        type = TYPE_U64;
    } else if(SVCMP(token.sv, "string")==0){
        type = TYPE_STRING;
    } else {
        type = TYPE_NOT_A_TYPE;
    }
    return type; 
}

int get_type_size_in_bytes(enum TypeEnum type){
    switch(type){
        case TYPE_I8:
        case TYPE_U8:
            return 1;
        case TYPE_I32:
        case TYPE_U32:
            return 4;
        case TYPE_I64:
        case TYPE_U64:
            return 8;
        case TYPE_STRING:
            return sizeof(SView);
       default:
           return -1; 
    }
}

Func parse_function(Lexer *lexer){
    Func func = {0};
    Token token = next_token(lexer);
    func.name = token.sv;
    token = next_token(lexer);
    if(token.type!=TOKEN_OPAREN){
        TOKENERROR(" Error: expected '(', got ");
    }
    Var_signature *vsp = malloc(sizeof(Var_signature)*10);
    func.args = vsp;
    token = next_token(lexer);
    while(token.type!=TOKEN_CPAREN){
        enum TypeEnum type = parse_type(lexer);
        token = next_token(lexer);
        if(token.type!=TOKEN_NAME){
            TOKENERROR(" Error: wanted variable name, got ");
        }
        Var_signature vs   = {.name=token.sv, .type=type};
        *vsp++ = vs;
        func.argc++;
        token = next_token(lexer);
    }
    token = next_token(lexer);
    if(token.type==TOKEN_RETURN_COLON){
        func.ret_type = parse_type(lexer);
        token = next_token(lexer);
    }
    if(token.type!=TOKEN_OCURLY){
        TOKENERROR(" Error: expected '{', got ");
    }
    int depth_level = 1;
    func.body.code = malloc(sizeof(Token)*100);
    for(int i=0, e=100; token.type!=TOKEN_CCURLY || depth_level>0; i++){
        if(i>=e){
            e+=50;
            func.body.code = realloc(func.body.code, e);
        }
        token = next_token(lexer);
        func.body.code[i]=token;
        func.body.exprc++;
        switch(token.type){
            case TOKEN_OCURLY:depth_level++;break;
            case TOKEN_CCURLY:depth_level--;break;
            default:break;
        }
    }
    return func;
}

int64_t get_num_value(Variable var){
    int64_t value = 0;
    switch(var.type){
        case TYPE_I8:
            value = *(int8_t*)var.ptr;
            break;
        case TYPE_I32:
            value = *(int32_t*)var.ptr;
            break;
        case TYPE_I64:
            value = *(int64_t*)var.ptr;
            break;
        default:
           printf("EROR: i8 i32 i64 types supported for get_num_value\n");
           exit(1);
    }
    return value;
}

Variable get_var_by_name(SView sv, Variable *variables, size_t varc){
    while(varc--){
        if(SVSVCMP(sv, variables[varc].name)==0){
            return variables[varc];
        }
    }
    return (Variable){0};
}

int64_t evaluate_num_expr(Token *expr, int64_t expr_size, Variable *variables, size_t varc){
    if(expr_size<=0){return 0;}
    int64_t value;
    bool negate=false;
    Variable var;
    /* printf("Evaluating %.*s\n", SVVARG(expr[expr_size-1].sv)); */
    switch(expr[expr_size-1].type){
        case TOKEN_OP_MINUS:
            negate = true;
            expr++;
        case TOKEN_NUMERIC:
            value = SVTOL(expr[expr_size-1].sv);
            value = (negate)?-value:value;
            break;
        case TOKEN_NAME:
            var = get_var_by_name(expr[expr_size-1].sv, variables, varc);
            value = get_num_value(var);
            break;
        default:
            printloc(expr[expr_size-1].loc);
            printf(" Error: unexpected token '%.*s'(type %s), expected variable or numeric\n", SVVARG(expr[expr_size-1].sv), TOKEN_TO_STR[expr[expr_size-1].type]);
            exit(1);
    }
    int64_t prev_part = evaluate_num_expr(expr, expr_size-2, variables, varc);
    switch(expr[expr_size-2].type){
        case TOKEN_OP_PLUS:
            return prev_part + value;
        case TOKEN_OP_MINUS:
            return prev_part - value;
        case TOKEN_OP_MUL:
            return prev_part * value;
        case TOKEN_OP_DIV:
            return prev_part / value;
        default:
            return value;
    }
}

bool evaluate_bool_expr(Token *expr, int64_t expr_size, Variable *variables, size_t varc){
    if(expr_size<=0){return 0;}
    int64_t left_expr_size = 0;
    while(expr[left_expr_size].type!=TOKEN_OP_LESS && expr[left_expr_size].type!=TOKEN_OP_GREATER && expr[left_expr_size].type!=TOKEN_OP_NOT){
        left_expr_size++;
    }
    enum TokenEnum token_op = expr[left_expr_size].type;
    int64_t right_expr_size = expr_size-left_expr_size-1;
    int64_t left_value = evaluate_num_expr(expr, left_expr_size, variables, varc);
    int64_t right_value = evaluate_num_expr(expr+left_expr_size+1, right_expr_size, variables, varc);
    switch(token_op){
        case TOKEN_OP_LESS:
            return left_value < right_value;
        case TOKEN_OP_GREATER:
            return left_value>right_value;
        case TOKEN_OP_NOT:
            return left_value!=right_value;
        default:break;
    }
    return false;
}

Variable var_num_cast(Variable var, size_t src){
    switch(var.type){
        case TYPE_I8:
            *(int8_t*)var.ptr = src;
            break;
        case TYPE_I32:
            *(int32_t*)var.ptr = src;
            break;
        case TYPE_I64:
            *(size_t*)var.ptr = src;
            break;
        default:
           printf("ERROR: i8 i32 i64 types supported\n");
           exit(1);
    }
    return var;
}


void evaluate_code_block(CodeBlock block){
    for(size_t i=0; i<block.exprc; i++){
        Token token = block.code[i];
        switch(token.type){
            case TOKEN_NAME:
                {
                    if(token_variable_type(token) != TYPE_NOT_A_TYPE || get_var_by_name(token.sv, block.variables, block.varc).ptr!=NULL){ 
                        Variable var = {0};
                        bool new_var=false;
                        if(token_variable_type(token)!=TYPE_NOT_A_TYPE){
                            var.type = token_variable_type(token);
                            token    = block.code[++i];
                            var.name = token.sv;
                            var.ptr  = malloc(get_type_size_in_bytes(var.type));
                            new_var=true;
                        } else {
                            var = get_var_by_name(token.sv, block.variables, block.varc);
                        }
                        if(var.type==TYPE_STRING){
                            printloc(token.loc);
                            printf(" Error: strings are unsupported\n");
                            exit(1);
                        }
                        token = block.code[++i];
                        if(token.type!=TOKEN_EQUAL_SIGN){
                            TOKENERROR(" Error: expected '=', got ");
                        }
                        token = block.code[++i];
                        int expr_size=0;
                        Token *expr_start=&block.code[i];
                        while(token.type!=TOKEN_SEMICOLON){
                            token = block.code[++i];
                            expr_size++;
                        }
                        var = var_num_cast(var, evaluate_num_expr(expr_start, expr_size, block.variables, block.varc));
                        block.variables[block.varc] = var;
                        if(new_var){
                            block.varc++;
                        }
                    } else if(SVCMP(token.sv, "print") == 0) {
                        token = block.code[++i];
                        while(token.type!=TOKEN_SEMICOLON){
                            switch(token.type){
                                case(TOKEN_STR_LITERAL):
                                    printf("%.*s", SVVARG(token.sv));
                                    break;
                                case TOKEN_NAME:
                                    printf("%zd", get_num_value(get_var_by_name(token.sv, block.variables, block.varc)));
                                    break;
                                case TOKEN_NUMERIC:
                                    printf("%ld", SVTOL(token.sv));
                                    break;
                                default:
                                    printloc(token.loc);
                                    printf(" Error: Unreachble.\n");
                                    exit(1);
                            }
                            token = block.code[++i];
                        }
                    }  else if(SVCMP(token.sv, "dprint") == 0) {
                        token = block.code[++i];
                        while(token.type!=TOKEN_SEMICOLON){
                            switch(token.type){
                                case TOKEN_NAME:
                                    printf("%s %.*s = %zd\n",
                                            TYPE_TO_STR[get_var_by_name(token.sv, block.variables, block.varc).type], 
                                            SVVARG(token.sv),
                                            get_num_value(get_var_by_name(token.sv, block.variables, block.varc)));
                                    break;
                                default:
                                    TOKENERROR(" Error: 'dprint' supports only variables, got ");
                            }
                            token = block.code[++i];
                        }
                    }else {
                        TOKENERROR(" Error: unknown directive ");
                    }
                }
                break;
            case TOKEN_IF:
                {
                    token = block.code[++i];
                    if(token.type!=TOKEN_OPAREN){
                        TOKENERROR(" Error, expected '(', got ");
                    }
                    token = block.code[++i];
                    Token *expr_start=&block.code[i];
                    int exprc=0;
                    while(token.type!=TOKEN_CPAREN){
                        token = block.code[++i];
                        exprc++;
                    }
                    bool if_true = evaluate_bool_expr(expr_start, exprc, block.variables, block.varc);
                    if(!if_true){ // skip if block
                        while(token.type!=TOKEN_CCURLY){
                            token = block.code[++i];
                        }
                    }
                    // if block, else, just code
                    if(if_true || block.code[i+1].type==TOKEN_ELSE){
                        token = block.code[++i];
                        token = block.code[++i];
                        if(token.type==TOKEN_OCURLY){
                            token = block.code[++i];
                        }
                        Token *expr_start=&block.code[i];
                        int exprc=0;
                        for(int depth_level=0; token.type!=TOKEN_CCURLY || depth_level>0;){
                            token = block.code[++i];
                            exprc++;
                            switch(token.type){
                                case TOKEN_OCURLY:depth_level++;break;
                                case TOKEN_CCURLY:depth_level--;break;
                                default:break;
                            }
                        }
                        evaluate_code_block((CodeBlock){.code=expr_start, .varc=block.varc, .exprc=exprc, .variables=block.variables});
                        if(block.code[i+1].type==TOKEN_ELSE){ // skip else block
                            while(block.code[i+1].type!=TOKEN_CCURLY){
                                token = block.code[++i];
                            }
                        }
                    }
                }
                break;
            case TOKEN_WHILE:
                {
                    token = block.code[++i];
                    if(token.type!=TOKEN_OPAREN){
                        TOKENERROR(" Error, expected '(', got ");
                    }
                    token = block.code[++i];
                    Token *while_expr_start=&block.code[i];
                    int while_expr_exprc=0;
                    while(token.type!=TOKEN_CPAREN){
                        token = block.code[++i];
                        while_expr_exprc++;
                    }
                    token = block.code[++i];
                    Token *while_block_start = &block.code[++i];
                    // START WHILE SOMEWHERE HERE
                    bool if_true = evaluate_bool_expr(while_expr_start, while_expr_exprc, block.variables, block.varc);
                    if(!if_true){ // skip while block
                        while(token.type!=TOKEN_CCURLY){
                            token = block.code[++i];
                        }
                    }
                    if(if_true){
                        int exprc=0;
                        for(int depth_level=1; token.type!=TOKEN_CCURLY || depth_level>0;){
                            token = block.code[++i];
                            exprc++;
                            switch(token.type){
                                case TOKEN_OCURLY:depth_level++;break;
                                case TOKEN_CCURLY:depth_level--;break;
                                default:break;
                            }
                        }
                        while(evaluate_bool_expr(while_expr_start, while_expr_exprc, block.variables, block.varc)){
                            evaluate_code_block((CodeBlock){.code=while_block_start, .exprc=exprc, .varc=block.varc, .variables=block.variables});
                        }
                    }
                }
                break;
            case TOKEN_RETURN:
                break;
            case TOKEN_CCURLY:
                break;
            default:
                TOKENERROR(" Error: unexpected token ");
        }
    }
}

Func functions[1024];

int main(int argc, char **argv){
    char *program_name = args_shift(&argc, &argv);
    if(argc == 0){
        usage(program_name);
        return 0;
    }
    char *next_arg = args_shift(&argc, &argv);
    if(strcmp(next_arg, "--verbose") == 0){
        verbose = true;
        next_arg = args_shift(&argc, &argv);
    }
    char *code_file_name = next_arg;
    FILE *code_file      = fopen(code_file_name, "r");
    if(code_file == NULL){
        printf("Error reading provided file. freezing out.\n");
        return 1;
    }
    fseek(code_file, 0, SEEK_END);
    int32_t code_file_size = ftell(code_file);
    fseek(code_file, 0, SEEK_SET);
    char *code_src = calloc(code_file_size+1, sizeof(*code_src));
    fread(code_src, code_file_size, 1, code_file);
    fclose(code_file);
    Lexer lexer = { .file_name = code_file_name,
                    .line = 1, .bol = 0, .pos = 0,
                    .source = code_src};
    Func fn;
    while(lexer.source[lexer.pos+1]!=0){
        Token token = next_token(&lexer);
        switch(token.type){
            case TOKEN_FN_DECL:
                fn = parse_function(&lexer);
                fn.body.variables = malloc(sizeof(Variable)*10);
                functions[hash(fn.name)%1024] = fn;
                break;
            default:
                printloc(token.loc);
                printf(" Error: unimplemented token '%.*s' in global scope\n", SVVARG(token.sv));
        }
    }
    // interpritation
    fn = functions[hash((SView){"main", 4})%1024];
    evaluate_code_block(fn.body);
    free(fn.body.variables);
    free(fn.args);
    free(fn.body.code);
    free(code_src);
    return 0;
}
