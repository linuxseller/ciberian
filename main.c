#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

#include "types.h"
#include "lexer.c"

void printloc(Location loc);
void debug_token(Token token);
void debug_variable(Variable variable);
void debug_block(CodeBlock block);
size_t hash(SView sv);
void usage(char *program_name);
char *args_shift(int *argc, char ***argv);
enum TypeEnum parse_type(Lexer *lexer);
enum TypeEnum token_variable_type(Token token);
int get_type_size_in_bytes(enum TypeEnum type);
void var_num_cast(Variable *var, int64_t src);
Func parse_function(Lexer *lexer);
int64_t get_num_value(Variable var);
Variable get_var_by_name(SView sv, Variables *variables, int64_t depth);
int64_t evaluate_num_expr(Token *expr, int64_t expr_size, Variables *variables, size_t depth);
bool evaluate_bool_expr(Token *expr, int64_t expr_size, Variables *variables, size_t depth);
int64_t evaluate_code_block(CodeBlock block);

Func functions[1024]={0};
void printloc(Location loc){
    printf("%s:%lu:%lu", loc.file_path, loc.row, loc.col);
}

void debug_token(Token token){
    printf("Token {\n");
    printf("\tsv: %.*s\n", SVVARG(token.sv));
    printf("\ttype: %s\n", TOKEN_TO_STR[token.type]);
    printf("\tloc: ");
    printloc(token.loc);
    printf("\n}\n");
}

void debug_variable(Variable variable){
    printf("Varible {\n");
    printf("\tname: %.*s\n", SVVARG(variable.name));
    printf("\ttype: %s\n", TYPE_TO_STR[variable.type]);
    printf("\tvalue: %zd\n", get_num_value(variable));
    printf("}\n");
}

void debug_block(CodeBlock block){
    printf("Block {\n");
    printf("\tdepth: %d\n", block.depth);
    printf("\texprc: %zu\n", block.exprc);
    printf("\tVariables: {\n");
    for(size_t i=0; i<block.variables[block.depth].varc; i++){
        debug_variable(block.variables[block.depth].variables[i]);
    }
    printf("}\n");
}

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
void var_num_cast(Variable *var, int64_t src){
    bool overflow = false;
    bool underflow = false;
    switch(var->type){
        case TYPE_I8:
            if(src>INT8_MAX){
                overflow=true;
                break;
            } else if(src<INT8_MIN){
                underflow=true;
                break;
            }
            *(int8_t*)var->ptr = src;
            break;
        case TYPE_I32:
            if(src>INT32_MAX){
                overflow=true;
                break;
            } else if(src<INT32_MIN){
                underflow=true;
                break;
            }
            *(int32_t*)var->ptr = src;
            break;
        case TYPE_I64:
            *(int64_t*)var->ptr = src;
            break;
        default:
           printf("ERROR: i8 i32 i64 types supported\n");
           exit(1);
    }
    if(overflow||underflow){
        printf("Error on assignation, %s %s in (tryed assigning %zd to '%.*s')\n",
                TYPE_TO_STR[var->type],
                (underflow)?"underflow":"overflow",
                src, SVVARG(var->name));
        if(verbose){
            printf("Type %s value range is ", TYPE_TO_STR[var->type]);
            switch(var->type){
                case TYPE_I8:
                    printf("[%d;%d]\n", INT8_MIN, INT8_MAX);
                    break;
                case TYPE_I32:
                    printf("[%d;%d]\n", INT32_MIN, INT32_MAX);
                    break;
                case TYPE_I64:
                    printf("[%zu;%zu]\n", INT64_MIN, INT64_MAX);
                    break;
                default:break;
            }
        }
        exit(1);
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
    func.args = malloc(sizeof(Var_signature)*10);
    token = next_token(lexer);
    while(token.type!=TOKEN_CPAREN){
        enum TypeEnum type = token_variable_type(token);
        token = next_token(lexer);
        if(token.type!=TOKEN_NAME){
            TOKENERROR(" Error: wanted variable name, got ");
        }
        Var_signature vs = {.name=token.sv, .type=type};
        func.args[func.argc] = vs;
        func.argc++;
        token = next_token(lexer);
        if(token.type==TOKEN_COMMA&&token.type!=TOKEN_CPAREN){
            token = next_token(lexer);
        }
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

Variable get_var_by_name(SView sv, Variables *variables, int64_t depth){
    while(depth>-1){
        for(size_t i=0; i<variables[depth].varc; i++){
            if(SVSVCMP(sv, variables[depth].variables[i].name) == 0){
                return variables[depth].variables[i];
            }
        }
        depth--;
    }
    return (Variable){0};
}

typedef struct {
    enum {RPN_OPERATOR, RPN_NUM} type;
    union {
        Token oper;
        int64_t numeric;
    };
} RpnObject;

int OP_PREC[] = {
    [TOKEN_OP_MUL]=4,
    [TOKEN_OP_DIV]=3,
    [TOKEN_OP_PLUS]=2,
    [TOKEN_OP_MINUS]=1,
};

int64_t evaluate_num_expr(Token *expr, int64_t expr_size, Variables *variables, size_t depth){
    RpnObject *stack = malloc(sizeof(RpnObject)*expr_size);
    RpnObject *postfix = malloc(sizeof(RpnObject)*expr_size);
    int64_t value;
    Variable var;
    int top=-1;
    int j=0;
    for(int i=0; i<expr_size; i++){
        switch(expr[i].type){
            case TOKEN_OP_DIV:
            case TOKEN_OP_MUL:
            case TOKEN_OP_PLUS:
            case TOKEN_OP_MINUS:
                while (top > -1 && OP_PREC[stack[top].type] >= OP_PREC[expr[i].type]){
                    postfix[j++] = stack[top--];
                }
                stack[top+1].type=RPN_OPERATOR;
                stack[top+1].oper.type = expr[i].type;
                top++;
                break;
            case TOKEN_NUMERIC:
                value = SVTOL(expr[i].sv);
                postfix[j].type=RPN_NUM;
                postfix[j].numeric=value;
                j++;
                break;
            case TOKEN_NAME:
                {
                    var = get_var_by_name(expr[i].sv, variables, depth);
                    if(var.name.data!=NULL){
                        value = get_num_value(var);
                        postfix[j].type=RPN_NUM;
                        postfix[j].numeric=value;
                        j++;
                        break;
                    }
                    Token fn_token = expr[i];
                    Token token = expr[++i];
                    if(token.type!=TOKEN_OPAREN){
                        TOKENERROR(" Error: you probably skipped '()' when function call. Otherwise you are f@cked up. Got ")
                    }
                    token = expr[++i];
                    Func fn_to_call = functions[hash(fn_token.sv)%1024];
                    fn_to_call.body.variables[1].varc=0;
                    for(size_t j=0; j<fn_to_call.argc; j++){
                        Token *arg_expr_start = &expr[i];
                        int arg_exprc = 0;
                        while(token.type!=TOKEN_COMMA){
                            if(token.type==TOKEN_CPAREN){
                                TOKENERROR(" Error: uhhm, bruh. You forgor some arguments ")
                            }
                            token = expr[++i];
                            arg_exprc++;
                        }
                        token = expr[++i];
                        int64_t argument_value = evaluate_num_expr(arg_expr_start, arg_exprc, variables, depth);
                        printf("argval %zd\n", argument_value);
                        Variable var;
                        var.name = fn_to_call.args[j].name;
                        var.type = fn_to_call.args[j].type;
                        var.ptr = malloc(get_type_size_in_bytes(var.type));
                        var_num_cast(&var, argument_value);
                        fn_to_call.body.variables[1].variables[j] = var; 
                        fn_to_call.body.variables[1].varc++;
                        fn_to_call.body.depth=1;
                    }
                    if(fn_to_call.name.data == NULL){
                        TOKENERROR(" Error: unknown directive ");
                    }
                    while(token.type!=TOKEN_SEMICOLON){
                        token = expr[++i];
                    }
                    postfix[j].type=RPN_NUM;
                    postfix[j].numeric=evaluate_code_block(fn_to_call.body);
                    j++;
                    break;
                }
            default:break;
        }
    }
    while (top > -1) {
        postfix[j++] = stack[top--];
    }
    top=0;
    for(int i=0; i<j; i++){
        /* switch(postfix[i].type){ */
        /*     case RPN_OPERATOR: */
        /*         printf("Operator: %s\n", TOKEN_TO_STR[postfix[i].oper.type]); */
        /*         break; */
        /*     default: */
        /*         printf("Numeric: %zd\n", postfix[i].numeric); */
        /*         break; */
        /* } */
        stack[top]=postfix[i];
        if(stack[top].type==RPN_OPERATOR){
            stack[top-2].type=RPN_NUM;
            switch(stack[top].oper.type){
                case TOKEN_OP_PLUS:
                    stack[top-2].numeric = stack[top-1].numeric + stack[top-2].numeric;
                    break;
                case TOKEN_OP_MINUS:
                    stack[top-2].numeric = stack[top-2].numeric - stack[top-1].numeric;
                    break;
                case TOKEN_OP_MUL:
                    stack[top-2].numeric = stack[top-1].numeric * stack[top-2].numeric;
                    break;
                case TOKEN_OP_DIV:
                    stack[top-2].numeric = stack[top-2].numeric / stack[top-1].numeric;
                    break;
                default:break;
            }
            top=top-2;
        }
        top++;
    }
    /* printf("RETURNING %zd\n", stack[top].numeric); */
    return stack[top-1].numeric;
    free(stack);
}

bool evaluate_bool_expr(Token *expr, int64_t expr_size, Variables *variables, size_t depth){
    if(expr_size<=0){return 0;}
    int64_t left_expr_size = 0;
    while(expr[left_expr_size].type!=TOKEN_OP_LESS && expr[left_expr_size].type!=TOKEN_OP_GREATER && expr[left_expr_size].type!=TOKEN_OP_NOT){
        left_expr_size++;
    }
    enum TokenEnum token_op = expr[left_expr_size].type;
    int64_t right_expr_size = expr_size-left_expr_size-1;
    int64_t left_value = evaluate_num_expr(expr, left_expr_size, variables, depth);
    int64_t right_value = evaluate_num_expr(expr+left_expr_size+1, right_expr_size, variables, depth);
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



int64_t evaluate_code_block(CodeBlock block){
    int64_t ret=0;
    for(size_t i=0; i<block.exprc; i++){
        Token token = block.code[i];
        switch(token.type){
            case TOKEN_NAME:
                {
                    if(token_variable_type(token) != TYPE_NOT_A_TYPE
                        || get_var_by_name(token.sv, block.variables, block.depth).ptr!=NULL){ 
                        Variable var = {0};
                        bool new_var=false;
                        if(token_variable_type(token)!=TYPE_NOT_A_TYPE){
                            var.type = token_variable_type(token);
                            token    = block.code[++i];
                            var.name = token.sv;
                            var.ptr  = malloc(get_type_size_in_bytes(var.type));
                            new_var=true;
                        } else {
                            var = get_var_by_name(token.sv, block.variables, block.depth);
                        }
                        if(var.type==TYPE_STRING){
                            printloc(token.loc);
                            printf(" Error: strings are unsupported\n");
                            exit(1);
                        }
                        token = block.code[++i];
                        if(token.type==TOKEN_EQUAL_SIGN){
                            token = block.code[++i];
                            int expr_size=0;
                            Token *expr_start=&block.code[i];
                            while(token.type!=TOKEN_SEMICOLON){
                                token = block.code[++i];
                                expr_size++;
                            }
                            int64_t val =evaluate_num_expr(expr_start, expr_size, block.variables, block.depth); 
                            var_num_cast(&var, val);
                            size_t varc = block.variables[block.depth].varc;
                            block.variables[block.depth].variables[varc] = var;
                            if(new_var){
                                block.variables[block.depth].varc++;
                            }
                        } else if(token.type==TOKEN_SEMICOLON){
                            var_num_cast(&var, 0);
                            size_t varc = block.variables[block.depth].varc;
                            block.variables[block.depth].variables[varc] = var;
                            if(new_var){
                                block.variables[block.depth].varc++;
                            }
                        } else {
                            TOKENERROR(" Error: expected '=' or ';', got ");
                        }
                    } else if(SVCMP(token.sv, "print") == 0) {
                        token = block.code[++i];
                        while(token.type!=TOKEN_SEMICOLON){
                            switch(token.type){
                                case(TOKEN_STR_LITERAL):
                                    printf("%.*s", SVVARG(token.sv));
                                    break;
                                case TOKEN_NAME:
                                    printf("%zd", get_num_value(get_var_by_name(token.sv, block.variables, block.depth)));
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
                                            TYPE_TO_STR[get_var_by_name(token.sv, block.variables, block.depth).type], 
                                            SVVARG(token.sv),
                                            get_num_value(get_var_by_name(token.sv, block.variables, block.depth)));
                                    break;
                                default:
                                    TOKENERROR(" Error: 'dprint' supports only variables, got ");
                            }
                            token = block.code[++i];
                        }
                    }else { // function call
                        Token fn_token = token;
                        token = block.code[++i];
                        if(token.type!=TOKEN_OPAREN){
                            TOKENERROR(" Error: you probably skipped '()' when function call. Otherwise you are f@cked up. Got ")
                        }
                        token = block.code[++i];
                        Func fn_to_call = functions[hash(fn_token.sv)%1024];
                        fn_to_call.body.variables[1].varc=0;
                        for(size_t j=0; j<fn_to_call.argc; j++){
                            Token *arg_expr_start = &block.code[i];
                            int arg_exprc = 0;
                            while(token.type!=TOKEN_COMMA){
                                if(token.type==TOKEN_CPAREN){
                                    TOKENERROR(" Error: uhhm, bruh. You forgor some arguments ")
                                }
                                token = block.code[++i];
                                arg_exprc++;
                            }
                            token = block.code[++i];
                            int64_t argument_value = evaluate_num_expr(arg_expr_start, arg_exprc, block.variables, block.depth);
                            Variable var;
                            var.name = fn_to_call.args[j].name;
                            var.type = fn_to_call.args[j].type;
                            var.ptr = malloc(get_type_size_in_bytes(var.type));
                            var_num_cast(&var, argument_value);
                            fn_to_call.body.variables[1].variables[j] = var; 
                            fn_to_call.body.variables[1].varc++;
                            fn_to_call.body.depth=1;
                        }
                        if(fn_to_call.name.data == NULL){
                            TOKENERROR(" Error: unknown directive ");
                        }
                        while(token.type!=TOKEN_SEMICOLON){
                            token = block.code[++i];
                        }
                        printf("%zd\n", evaluate_code_block(fn_to_call.body));

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
                    bool if_true = evaluate_bool_expr(expr_start, exprc, block.variables, block.depth);
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
                        evaluate_code_block(
                                (CodeBlock){
                                    .code=expr_start,
                                    .exprc=exprc,
                                    .variables=block.variables,
                                    .depth=block.depth+1}
                                    );
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
                    bool if_true = evaluate_bool_expr(while_expr_start, while_expr_exprc, block.variables, block.depth);
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
                        while(evaluate_bool_expr(while_expr_start, while_expr_exprc, block.variables, block.depth)){
                            evaluate_code_block(
                                    (CodeBlock){
                                        .code=while_block_start,
                                        .exprc=exprc,
                                        .variables=block.variables,
                                        .depth=block.depth+1,
                                        });
                        }
                    }
                }
                break;
            case TOKEN_RETURN:
                token = block.code[++i];
                Token *expr_start=&block.code[i];
                size_t exprc=0;
                while(token.type!=TOKEN_SEMICOLON){
                    token = block.code[++i];
                    exprc++;
                }
                ret = evaluate_num_expr(expr_start, exprc, block.variables, block.depth);
                goto eval_ret;
                break;
            case TOKEN_CCURLY:
                break;
            default:
                TOKENERROR(" Error: unexpected token ");
        }
    }
    /* while(block.variables[block.depth].varc--){ */
    /*     debug_variable(block.variables[block.depth].variables[block.variables[block.depth].varc]); */
    /*     free(block.variables[block.depth].variables[block.variables[block.depth].varc].ptr); */
    /* } */
eval_ret:
    return ret;
}


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
                fn.body.variables = malloc(sizeof(Variables)*10);
                for(int i=0; i<10; i++){
                    fn.body.variables[i].variables = calloc(sizeof(Variable),10);
                    fn.body.variables[i].varc=0;
                }
                fn.body.depth = 1;
                functions[hash(fn.name)%1024] = fn;
                break;
            default:
                printloc(token.loc);
                printf(" Error: unimplemented token '%.*s' in global scope\n", SVVARG(token.sv));
        }
    }
    // interpritation
    fn = functions[hash((SView){"main", 4})%1024];
    fn.body.depth=1;
    evaluate_code_block(fn.body);
    for(int i=0; i<10; i++){
        free(fn.body.variables[i].variables);
    }
    free(fn.body.variables);
    free(fn.args);
    free(fn.body.code);
    free(code_src);
    return 0;
}
