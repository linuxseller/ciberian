#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

#include "types.h"
#include "lexer.c"
#include "functions.h"
#include "cbrstdlib.c"

Func functions[1024] = {0};

// print and debug functions
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
    for(size_t i = 0; i<block.variables[block.depth].varc; i++){
        debug_variable(block.variables[block.depth].variables[i]);
    }
    printf("}\n");
}
//Xprint and debug functions

// Hash function for functions TODO: test for variables
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

// User Experience
void usage(char *program_name){
    printf("usage: %s [flags] <filename.cbr>\n", program_name);
    printf("flags:\n");
    printf("\t--verbose : provides additional info\n");
}
// verbose output on error
bool verbose = false;

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
        if(sizeof(size_t)!=sizeof(int64_t)){
            TOKENERROR(" Error: i64 not supported on this architecture");
        }
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

// Cast int to variable
void var_num_cast(Variable *var, int64_t src){
    bool overflow = false;
    bool underflow = false;
    switch(var->type){
        case TYPE_I8:
            if(src>INT8_MAX){
                overflow = true;
                break;
            } else if(src<INT8_MIN){
                underflow = true;
                break;
            }
            *(int8_t*)var->ptr = src;
            break;
        case TYPE_I32:
            if(src>INT32_MAX){
                overflow = true;
                break;
            } else if(src<INT32_MIN){
                underflow = true;
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
        Var_signature vs = {.name=token.sv, .type = type};
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
    for(int i=0, e = 100; token.type!=TOKEN_CCURLY || depth_level>0; i++){
        if(i>=e){
            e+=50;
            func.body.code = realloc(func.body.code, e);
        }
        token = next_token(lexer);
        func.body.code[i] = token;
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
int64_t get_arr_num_value(Variable var, size_t index){
    int64_t value = 0;
    switch(var.type){
        case TYPE_I8:
            value = *((int8_t*)var.ptr+index);
            break;
        case TYPE_I32:
            value = *((int32_t*)var.ptr+index);
            break;
        case TYPE_I64:
            value = *((int64_t*)var.ptr+index);
            break;
        default:
           printf("EROR: i8 i32 i64 types supported for get_num_value\n");
           exit(1);
    }
    return value;
}
Variable get_var_by_name(SView sv, Variables *variables, int64_t depth){
    while(depth>-1){
        for(size_t i = 0; i<variables[depth].varc; i++){
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
    [TOKEN_OP_MUL] = 4,
    [TOKEN_OP_DIV] = 3,
    [TOKEN_OP_PLUS] = 2,
    [TOKEN_OP_MINUS] = 1,
};

int64_t evaluate_expr(Token *expr, int64_t expr_size, Variables *variables, size_t depth){
    RpnObject *stack = malloc(sizeof(RpnObject)*expr_size);
    RpnObject *postfix = malloc(sizeof(RpnObject)*expr_size);
    int64_t value;
    Variable var;
    int top = -1;
    int j = 0;
    for(int i = 0; i<expr_size; i++){
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
                    fn_to_call.body.variables=malloc(sizeof(Variables)*10);
                    for(int i = 0; i<10; i++){
                        fn_to_call.body.variables[i].varc = 0;
                    }
                    for(size_t j = 0; j<fn_to_call.argc; j++){
                        Token *arg_expr_start = &expr[i];
                        int arg_exprc = 0;
                        while(token.type != TOKEN_COMMA){
                            if(token.type == TOKEN_CPAREN){
                                TOKENERROR(" Error: uhhm, bruh. You forgor some arguments ")
                            }
                            token = expr[++i];
                            arg_exprc++;
                        }
                        token = expr[++i];
                        int64_t argument_value = evaluate_expr(arg_expr_start, arg_exprc, variables, depth);
                        Variable var;
                        var.name = fn_to_call.args[j].name;
                        var.type = fn_to_call.args[j].type;
                        var.modifyer = MOD_NO_MOD;
                        var.ptr = malloc(get_type_size_in_bytes(var.type));
                        var_num_cast(&var, argument_value);
                        if(variables[depth].varc == 0){
                            fn_to_call.body.variables[1].variables = malloc(sizeof(Variable));
                        } else {
                            fn_to_call.body.variables[1].variables =
                                realloc(fn_to_call.body.variables[1].variables, sizeof(Variable)*(fn_to_call.body.variables[1].varc+1));
                        }
                        fn_to_call.body.variables[1].variables[j] = var; 
                        fn_to_call.body.variables[1].varc++;
                        fn_to_call.body.depth = 1;
                    }
                    if(fn_to_call.name.data == NULL){
                        TOKENERROR(" Error: unknown directive ");
                    }
                    while(token.type != TOKEN_SEMICOLON){
                        token = expr[++i];
                    }
                    postfix[j].type = RPN_NUM;
                    postfix[j].numeric = evaluate_code_block(fn_to_call.body).value;
                    j++;
                    break;
                }
            default:break;
        }
    }
    while (top > -1) {
        postfix[j++] = stack[top--];
    }
    top = 0;
    for(int i = 0; i<j; i++){
        /* switch(postfix[i].type){ */
        /*     case RPN_OPERATOR: */
        /*         printf("Operator: %s\n", TOKEN_TO_STR[postfix[i].oper.type]); */
        /*         break; */
        /*     default: */
        /*         printf("Numeric: %zd\n", postfix[i].numeric); */
        /*         break; */
        /* } */
        stack[top] = postfix[i];
        if(stack[top].type == RPN_OPERATOR){
            stack[top-2].type = RPN_NUM;
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
            top = top-2;
        }
        top++;
    }
    /* printf("RETURNING %zd\n", stack[top].numeric); */
    int64_t rval = stack[top-1].numeric;
    free(stack);
    free(postfix);
    return rval;
}

bool evaluate_bool_expr(Token *expr, int64_t expr_size, Variables *variables, size_t depth){
    if(expr_size <= 0){return 0;}
    int64_t left_expr_size = 0;
    while(expr[left_expr_size].type != TOKEN_OP_LESS && expr[left_expr_size].type != TOKEN_OP_GREATER
            && expr[left_expr_size].type != TOKEN_OP_NOT && expr[left_expr_size].type != TOKEN_OP_NOT
            && expr[left_expr_size].type != TOKEN_EQUAL_SIGN){
        left_expr_size++;
    }
    enum TokenEnum token_op = expr[left_expr_size].type;
    Token token_op_next = expr[left_expr_size+1];
    /* int eqsn=0; */
    /* if(token_op_next.type==TOKEN_EQUAL_SIGN){ */
    /*     eqsn=1; */
    /* } */
    int64_t right_expr_size = expr_size-left_expr_size-1;
    int64_t left_value = evaluate_expr(expr, left_expr_size, variables, depth);
    int64_t right_value = evaluate_expr(expr+left_expr_size+1, right_expr_size, variables, depth);
    switch(token_op){
        case TOKEN_OP_LESS:
            if(token_op_next.type!=TOKEN_EQUAL_SIGN){
                return left_value<=right_value;
            }
            return left_value < right_value;
        case TOKEN_OP_GREATER:
            if(token_op_next.type!=TOKEN_EQUAL_SIGN){
                return left_value>=right_value;
            }
            return left_value>right_value;
        case TOKEN_OP_NOT:
            if(token_op_next.type!=TOKEN_EQUAL_SIGN){
                printloc(token_op_next.loc);
                printf(" Error: expected '!=', got !%.*s\n", SVVARG(token_op_next.sv));
                exit(1);
            }
            return left_value != right_value;
        case TOKEN_EQUAL_SIGN:
            if(token_op_next.type!=TOKEN_EQUAL_SIGN){
                printloc(token_op_next.loc);
                printf(" Error: assignation on condition, expected '==', got =%.*s\n", SVVARG(token_op_next.sv));
                exit(1);
            }
            return left_value == right_value;
        default:break;
    }
    return false;
}



bool function_return = false;
CBReturn evaluate_code_block(CodeBlock block){
    CBReturn ret = {0};
    for(size_t i = 0; i<block.exprc; i++){
        Token token = block.code[i];
        switch(token.type){
            case TOKEN_NAME:
                {
                    if(token_variable_type(token) != TYPE_NOT_A_TYPE
                        || get_var_by_name(token.sv, block.variables, block.depth).ptr != NULL){ 
                        Variable var = {0};
                        bool new_var = false;
                        if(token_variable_type(token) != TYPE_NOT_A_TYPE){
                            var.type = token_variable_type(token);
                            token    = block.code[++i];
                            var.name = token.sv;
                            if(block.code[i+1].type != TOKEN_OSQUAR){
                                var.modifyer = MOD_NO_MOD;
                                var.ptr  = malloc(get_type_size_in_bytes(var.type));
                            } else { // variable is array
                                token = block.code[i++];
                                var.modifyer = MOD_ARRAY;
                                Token *expr_start = &block.code[i];
                                size_t expr_size=0;
                                while(token.type != TOKEN_CSQUAR){
                                    token = block.code[++i];
                                    expr_size++;
                                }
                                size_t arrlen = evaluate_expr(expr_start, expr_size, block.variables, block.depth); 
                                var.ptr = malloc(get_type_size_in_bytes(var.type)*arrlen);
                                var.size = arrlen;
                                printf("var soize: %zd\n", var.size);
                            }
                            new_var = true;
                            if(block.variables[block.depth].varc == 0){
                                block.variables[block.depth].variables = malloc(sizeof(Variable));
                            } else {
                                block.variables[block.depth].variables =
                                    realloc(block.variables[block.depth].variables, sizeof(Variable)*(block.variables[block.depth].varc+1));
                            }
                            if(var.modifyer == MOD_ARRAY){
                                size_t varc = block.variables[block.depth].varc;
                                block.variables[block.depth].variables[varc] = var;
                                block.variables[block.depth].varc++;
                                token=block.code[++i];
                                break;} // array initialisation not supported for now
                        } else {
                            var = get_var_by_name(token.sv, block.variables, block.depth);
                        }
                        if(var.type == TYPE_STRING){
                            printloc(token.loc);
                            printf(" Error: strings are unsupported\n");
                            exit(1);
                        }
                        if(block.code[i+1].type == TOKEN_OSQUAR){ // if square bracket after variable name, then it is acces to array.
                            if(var.modifyer!=MOD_ARRAY){
                                TOKENERROR(" Error: trying to use usual variable as array, expected '[', got ");
                            }
                            token = block.code[i++];
                            Token *expr_start = &block.code[i];
                            size_t expr_size=0;
                            while(token.type != TOKEN_CSQUAR){
                                token = block.code[++i];
                                expr_size++;
                            }
                            size_t arr_index = evaluate_expr(expr_start, expr_size, block.variables, block.depth);
                            void *arr_id_ptr=NULL;
                            switch(var.type){
                                case TYPE_I8:
                                    arr_id_ptr = (int8_t*)var.ptr+arr_index;
                                    break;
                                case TYPE_I32:
                                    arr_id_ptr = (int32_t*)var.ptr+arr_index;
                                    break;
                                case TYPE_I64:
                                    arr_id_ptr = (int64_t*)var.ptr+arr_index;
                                    break;
                                default:
                                    break;
                            }
                            var = (Variable){.name = var.name, .modifyer = MOD_NO_MOD, .type = var.type, .ptr = arr_id_ptr};
                        }
                        token = block.code[++i];
                        if(token.type == TOKEN_EQUAL_SIGN){
                            token = block.code[++i];
                            int expr_size = 0;
                            Token *expr_start = &block.code[i];
                            while(token.type != TOKEN_SEMICOLON){
                                token = block.code[++i];
                                expr_size++;
                            }
                            int64_t val = evaluate_expr(expr_start, expr_size, block.variables, block.depth); 
                            var_num_cast(&var, val);
                            size_t varc = block.variables[block.depth].varc;
                            
                            if(new_var){
                                block.variables[block.depth].variables[varc] = var;
                                block.variables[block.depth].varc++;
                            }else{
                            }
                        } else if(token.type == TOKEN_SEMICOLON){
                            var_num_cast(&var, 0);
                            size_t varc = block.variables[block.depth].varc;
                            block.variables[block.depth].variables[varc] = var;
                            if(new_var){
                                block.variables[block.depth].varc++;
                            }
                        } else {
                            TOKENERROR(" Error: expected ' = ' or ';', got ");
                        }
                    } else {
                        Token *expr_start = &block.code[i];
                        size_t exprc = 0;
                        while(token.type != TOKEN_SEMICOLON){
                            token = block.code[++i];
                            exprc++;
                        }
                        stdcall(expr_start, exprc, block.variables, block.depth);
                    }
                }
                break;
            case TOKEN_IF:
                {
                    token = block.code[++i];
                    if(token.type != TOKEN_OPAREN){
                        TOKENERROR(" Error, expected '(', got ");
                    }
                    token = block.code[++i];
                    Token *expr_start = &block.code[i];
                    int exprc = 0;
                    while(token.type != TOKEN_CPAREN){
                        token = block.code[++i];
                        exprc++;
                    }
                    bool if_true = evaluate_bool_expr(expr_start, exprc, block.variables, block.depth);
                    if(!if_true){ // skip if block
                       
                       for(int depth_level = 0; token.type != TOKEN_CCURLY || depth_level>0;){
                            token = block.code[++i];
                            switch(token.type){
                                case TOKEN_OCURLY:depth_level++;break;
                                case TOKEN_CCURLY:depth_level--;break;
                                default:break;
                            }
                        }
                    }
                    // if block, else, just code
                    if(if_true || block.code[i+1].type == TOKEN_ELSE){
                        while(token.type != TOKEN_OCURLY){
                            token = block.code[++i];
                        }
                        token = block.code[++i];
                        Token *expr_start = &block.code[i];
                        int exprc = 0;
                        for(int depth_level = 1; token.type != TOKEN_CCURLY || depth_level>0;){
                            token = block.code[++i];
                            exprc++;
                            switch(token.type){
                                case TOKEN_OCURLY:depth_level++;break;
                                case TOKEN_CCURLY:depth_level--;break;
                                default:break;
                            }
                        }
                        // HERE 1
                        ret = evaluate_code_block(
                                (CodeBlock){
                                    .code = expr_start,
                                    .exprc = exprc,
                                    .variables = block.variables,
                                    .depth = block.depth+1}
                                    );
                        /* debug_token(block.code[i+1]); */
                        if(block.code[i+1].type == TOKEN_ELSE){ // skip else block
                            while(block.code[i+1].type != TOKEN_CCURLY){
                                /* debug_token(token); */
                                token = block.code[++i];
                            }
                        }
                        if(ret.returned){
                            goto eval_ret;
                        }
                    }
                }
                break;
            case TOKEN_WHILE:
                {
                    token = block.code[++i];
                    if(token.type != TOKEN_OPAREN){
                        TOKENERROR(" Error, expected '(', got ");
                    }
                    token = block.code[++i];
                    Token *while_expr_start = &block.code[i];
                    int while_expr_exprc = 0;
                    while(token.type != TOKEN_CPAREN){
                        token = block.code[++i];
                        while_expr_exprc++;
                    }
                    token = block.code[++i];
                    Token *while_block_start = &block.code[++i];
                    bool if_true = evaluate_bool_expr(while_expr_start, while_expr_exprc, block.variables, block.depth);
                    if(!if_true){ // skip while block
                        while(token.type != TOKEN_CCURLY){
                            token = block.code[++i];
                        }
                    }
                    if(if_true){
                        int exprc = 0;
                        for(int depth_level = 1; token.type != TOKEN_CCURLY || depth_level>0;){
                            token = block.code[++i];
                            exprc++;
                            switch(token.type){
                                case TOKEN_OCURLY:depth_level++;break;
                                case TOKEN_CCURLY:depth_level--;break;
                                default:break;
                            }
                        }
                        while(evaluate_bool_expr(while_expr_start, while_expr_exprc, block.variables, block.depth)){
                            ret = evaluate_code_block(
                                    (CodeBlock){
                                        .code = while_block_start,
                                        .exprc = exprc,
                                        .variables = block.variables,
                                        .depth = block.depth+1,
                                        });
                            if(ret.returned){
                                goto eval_ret;
                            }
                        }
                    }
                }
                break;
            case TOKEN_RETURN:
                function_return=true;
                token = block.code[++i];
                Token *expr_start = &block.code[i];
                size_t exprc = 0;
                while(token.type != TOKEN_SEMICOLON){
                    token = block.code[++i];
                    exprc++;
                }
                ret.value = evaluate_expr(expr_start, exprc, block.variables, block.depth);
                ret.returned = true;
                while(block.variables[block.depth].varc--){
                    free(block.variables[block.depth].variables[block.variables[block.depth].varc].ptr);
                }
                goto eval_ret;
                break;
            case TOKEN_CCURLY:
                break;
            default:
                TOKENERROR(" Error: unexpected token ");
        }
    }
    // GC
    
eval_ret:
    return ret;
}

// interpreter argument shifter functions
char *args_shift(int *argc, char ***argv){
    (*argc)--;
    return *((*argv)++);
}

int main(int argc, char **argv){
    // prepare interpreter
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
    // Load program code
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
    // Setup lexer
    Lexer lexer = { .file_name = code_file_name,
                    .line = 1, .bol = 0, .pos = 0,
                    .source = code_src};
    // Parse Functions into memory
    Func fn;
    while(lexer.source[lexer.pos+1] != 0){
        Token token = next_token(&lexer);
        switch(token.type){
            case TOKEN_FN_DECL:
                fn = parse_function(&lexer);
                fn.body.variables = malloc(sizeof(Variables)*10);
                for(int i = 0; i<10; i++){
                    fn.body.variables[i].varc = 0;
                }
                fn.body.depth = 1;
                functions[hash(fn.name)%1024] = fn;
                break;
            default:
                printloc(token.loc);
                printf(" Error: unimplemented token '%.*s' in global scope\n", SVVARG(token.sv));
        }
    }
    // Interpritation
    fn = functions[hash((SView){"main", 4})%1024];
    fn.body.depth = 1;
    setup_cbrstd();
    evaluate_code_block(fn.body);
    // FREE !!!
    free(fn.args);
    free(fn.body.code);
    free(code_src);
    return 0;
}
