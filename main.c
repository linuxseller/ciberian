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

#define logf printf

Func functions[1024] = {0};

void printloc(Location loc){
    logf("%s:%lu:%lu", loc.file_path, loc.row, loc.col);
}

void debug_token(Token token){
    logf("Token {\n");
    logf("\tsv: %.*s\n", SVVARG(token.sv));
    logf("\ttype: %s\n", TOKEN_TO_STR[token.type]);
    logf("\tloc: ");
    printloc(token.loc);
    logf("\n}\n");
}

void debug_variable(Variable variable){
    logf("Varible {\n");
    logf("\tname: %.*s\n", SVVARG(variable.name));
    logf("\ttype: %s\n", TYPE_TO_STR[variable.type]);
    logf("\tvalue: %zd\n", get_num_value(variable, (Location){0}));
    logf("}\n");
}

void debug_block(CodeBlock block){
    logf("Block {\n");
    logf("\tdepth: %d\n", block.depth);
    logf("\texprc: %zu\n", block.exprc);
    logf("\tVariables: {\n");
    for(size_t i = 0; i<block.variables[block.depth].varc; i++){
        debug_variable(block.variables[block.depth].variables[i]);
    }
    logf("}\n");
}

// Hash function for function names
// TODO: test for variables
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
    logf("usage: %s [flags] <filename.cbr>\n", program_name);
    logf("flags:\n");
    logf("\t--verbose : provides additional info\n");
}
// TODO: verbose output on error
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
    } else if(SVCMP(token.sv, "void")==0){
        type = TYPE_VOID;
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
        if(sizeof(size_t)!=8){
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
            return sizeof(Variable);
       default:
           return -1;
    }
}

// Cast int to variable
void var_cast(Variable *var, CBReturn src){
    // variables that track over- and under-flows of integer types
    bool overflow = false;
    bool underflow = false;
    // Type checking
    // * mostly, assignation occurs on result of evaluate_expr function, which MUST calculate type of expression
    // * if expression is numeric, that it can be assigned to anything that is not overflow or underflowed
    if(src.type != var->type && src.type!=TYPE_NUMERIC){
        logf("Error on assignation of '%s %.*s' to type '%s'\n",
                TYPE_TO_STR[var->type],
                SVVARG(var->name),
                TYPE_TO_STR[src.type]);
        exit(1);
    }
    switch(var->type){
        case TYPE_STRING:
            var->ptr = src.string.data;
            var->size = src.string.size;
            break;
        case TYPE_I8:
            if(src.num>INT8_MAX){
                overflow = true;
                break;
            } else if(src.num<INT8_MIN){
                underflow = true;
                break;
            }
            *(int8_t*)var->ptr = src.num;
            break;
        case TYPE_I32:
            if(src.num>INT32_MAX){
                overflow = true;
                break;
            } else if(src.num<INT32_MIN){
                underflow = true;
                break;
            }
            *(int32_t*)var->ptr = src.num;
            break;
        case TYPE_I64:
            *(ssize_t*)var->ptr = src.num;
            break;
        default:
           logf("ERROR: i8 i32 i64 and string types supported for assignement\n");
           exit(1);
    }
    if(overflow||underflow){
        logf("Error on assignation, %s %s in (tryed assigning %zd to '%.*s')\n",
                TYPE_TO_STR[var->type],
                (underflow)?"underflow":"overflow",
                src.num, SVVARG(var->name));
        if(verbose){
            logf("Type %s value range is ", TYPE_TO_STR[var->type]);
            switch(var->type){
                case TYPE_I8:
                    logf("[%d;%d]\n", INT8_MIN, INT8_MAX);
                    break;
                case TYPE_I32:
                    logf("[%d;%d]\n", INT32_MIN, INT32_MAX);
                    break;
                case TYPE_I64:
                    logf("[%zu;%zu]\n", INT64_MIN, INT64_MAX);
                    break;
                default:break;
            }
        }
        exit(1);
    }
}

void copy_array(Variable dst, Variable src){
    if(dst.type!=src.type){
        logf("ERROR: array copying types mismatch\n");
        logf("tried assigning %s[%zd] to %s[%zd]\n", TYPE_TO_STR[src.type], src.size, TYPE_TO_STR[dst.type], dst.size);
        exit(1);
    }
    switch(src.type){
        case TYPE_STRING:
        case TYPE_I8:
            for(size_t i=0; i<src.size; i++){
                *((int8_t*)dst.ptr+i) = *((int8_t*)src.ptr+i);
            }
            break;
        case TYPE_I32:
            for(size_t i=0; i<src.size; i++){
                *((int32_t*)dst.ptr+i) = *((int32_t*)src.ptr+i);
            }
            break;
        case TYPE_I64:
            for(size_t i=0; i<src.size; i++){
                *((ssize_t*)dst.ptr+i) = *((ssize_t*)src.ptr+i);
            }
            break;
        default:
           logf("EROR: i8 i32 i64 types supported for get_num_value\n");
           exit(1);
    }
    ;
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
        enum TypeEnum type = token_variable_type(token);      // getting type
        token = next_token(lexer);                            // getting arg name
        if(token.type!=TOKEN_NAME){
            TOKENERROR(" Error: wanted variable name, got ");
        }
        SView argname = token.sv;
        token=next_token(lexer); // var modifyer || comma
        enum ModifyerEnum mod = MOD_NO_MOD;
        if(token.type == TOKEN_OSQUAR){
            mod = MOD_ARRAY;
            token=next_token(lexer); // arg was arr => next_token == ']'
            token = next_token(lexer); // if arg was arr -> get comma as token => next_token == ','
        }
        Var_signature vs = {.name=argname, .type = type, .modifyer=mod};
        func.args[func.argc] = vs;
        func.argc++;
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
    func.body.code = malloc(sizeof(Token)*200);
    for(int i=0, e = 200; token.type!=TOKEN_CCURLY || depth_level>0; i++){
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

ssize_t get_num_value(Variable var, Location loc){
    ssize_t value = 0;
    switch(var.type){
        case TYPE_I8:
            value = *(int8_t*)var.ptr;
            break;
        case TYPE_I32:
            value = *(int32_t*)var.ptr;
            break;
        case TYPE_I64:
            value = *(ssize_t*)var.ptr;
            break;
        case TYPE_NOT_A_TYPE:
            printloc(loc);
            logf(" Error: could not find variable!!!\n");
            exit(1);
            break;
        default:
           logf("EROR: i8 i32 i64 types supported for get_num_value\n");
           exit(1);
    }
    return value;
}
ssize_t get_arr_num_value(Variable var, size_t index){
    ssize_t value = 0;
    switch(var.type){
        case TYPE_I8:
            value = *((int8_t*)var.ptr+index);
            break;
        case TYPE_I32:
            value = *((int32_t*)var.ptr+index);
            break;
        case TYPE_I64:
            value = *((ssize_t*)var.ptr+index);
            break;
        default:
           logf("EROR: i8 i32 i64 types supported for get_num_value\n");
           exit(1);
    }
    return value;
}
Variable get_var_by_name(SView sv, Variables *variables, ssize_t depth){
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
        ssize_t numeric;
    };
} RpnObject;

int OP_PREC[] = {
    [TOKEN_OP_MUL] = 4,
    [TOKEN_OP_DIV] = 3,
    [TOKEN_OP_PLUS] = 2,
    [TOKEN_OP_MINUS] = 1,
};

Variable get_var_from_arr(Variable arr_var, ssize_t arr_index){
    if(arr_index>(ssize_t)arr_var.size || arr_index<0){
        //RUNTIMEERROR(" Error: array index is out of range [0;array.size)");
    }
    void *arr_id_ptr=NULL;
    switch(arr_var.type){
        case TYPE_I8:
            arr_id_ptr = (int8_t*)arr_var.ptr+arr_index;
            break;
        case TYPE_I32:
            arr_id_ptr = (int32_t*)arr_var.ptr+arr_index;
            break;
        case TYPE_I64:
            arr_id_ptr = (ssize_t*)arr_var.ptr+arr_index;
            break;
        default:
            break;
    }
    Variable var = (Variable){.name = arr_var.name, .modifyer = MOD_NO_MOD, .type = arr_var.type, .ptr = arr_id_ptr};
    return var;
}

CBReturn evaluate_expr(Token *expr, ssize_t expr_size, Variables *variables, size_t depth){
    RpnObject *stack = malloc(sizeof(RpnObject)*expr_size);
    RpnObject *postfix = malloc(sizeof(RpnObject)*expr_size);
    ssize_t value;
    Variable var;
    CBReturn rval;
    int top = -1;
    int j = 0;
    for(int i = 0; i<expr_size; i++){
        switch(expr[i].type){
            case TOKEN_STR_LITERAL:
                rval.type = TYPE_STRING;
                rval.string = expr[i].sv;
                goto eval_expr_ret;
                break;
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
                    if(var.type!=TYPE_NOT_A_TYPE){
                        if(var.modifyer==MOD_ARRAY){
                            Token token = expr[++i];
                            if(token.type==TOKEN_DOT){
                                i++;
                                if(SVCMP(expr[i].sv, "length")==0){
                                    value = var.size;
                                } else {
                                    TOKENERROR(" Error, array has no such field ");
                                }
                            } else if(token.type == TOKEN_OSQUAR){
                                Token *expr_start = &expr[i];
                                size_t exprc=0;
                                COLLECT_EXPR(TOKEN_OSQUAR, TOKEN_CSQUAR, expr, i);
                                size_t arr_index = evaluate_expr(expr_start, exprc, variables, depth).num;
                                Variable var_ret = get_var_from_arr(var, arr_index);
                                value = get_num_value(var_ret, token.loc);
                            } else {
                                RUNTIMEERROR("Expected .length or [index], got ");
                            }
                        } else {
                            value = get_num_value(var, expr[i].loc);
                        }
                        postfix[j].type=RPN_NUM;
                        postfix[j].numeric=value;
                        j++;
                        break;
                    }
                // function call handling
#include "tmp.c"
                }
            default:break;
        }
    }
    while (top > -1) {
        postfix[j++] = stack[top--];
    }
    top = 0;
    for(int i = 0; i<j; i++){
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
    rval.type = TYPE_NUMERIC;
    rval.num = stack[top-1].numeric;
    rval.returned = true;
eval_expr_ret:
    free(stack);
    free(postfix);
    return rval;
}

bool evaluate_bool_expr(Token *expr, ssize_t expr_size, Variables *variables, size_t depth){
    if(expr_size <= 0){return 0;}
    ssize_t left_expr_size = 0;
    while(expr[left_expr_size].type != TOKEN_OP_LESS && expr[left_expr_size].type != TOKEN_OP_GREATER
            && expr[left_expr_size].type != TOKEN_OP_NOT && expr[left_expr_size].type != TOKEN_OP_NOT
            && expr[left_expr_size].type != TOKEN_EQUAL_SIGN){
        left_expr_size++;
    }
    enum TokenEnum token_op = expr[left_expr_size].type;
    Token token_op_next = expr[left_expr_size+1];
    ssize_t right_expr_size = expr_size-left_expr_size-1;
    ssize_t left_value = evaluate_expr(expr, left_expr_size, variables, depth).num;
    ssize_t right_value = evaluate_expr(expr+left_expr_size+1, right_expr_size, variables, depth).num;
    switch(token_op){
        case TOKEN_OP_LESS:
            if(token_op_next.type==TOKEN_EQUAL_SIGN){
                return left_value<=right_value;
            }
            return left_value < right_value;
        case TOKEN_OP_GREATER:
            if(token_op_next.type==TOKEN_EQUAL_SIGN){
                return left_value>=right_value;
            }
            return left_value>right_value;
        case TOKEN_OP_NOT:
            if(token_op_next.type!=TOKEN_EQUAL_SIGN){
                printloc(token_op_next.loc);
                logf(" Error: expected '!=', got !%.*s\n", SVVARG(token_op_next.sv));
                exit(1);
            }
            return left_value != right_value;
        case TOKEN_EQUAL_SIGN:
            if(token_op_next.type!=TOKEN_EQUAL_SIGN){
                printloc(token_op_next.loc);
                logf(" Error: assignation on condition, expected '==', got =%.*s\n", SVVARG(token_op_next.sv));
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
                                if(var.type==TYPE_STRING){
                                    var.modifyer = MOD_ARRAY;
                                    var.ptr = malloc(token.sv.size+1); //TODO: ram leak?
                                } else {
                                    var.modifyer = MOD_NO_MOD;
                                    var.ptr  = malloc(get_type_size_in_bytes(var.type));
                                }
                            } else { // variable is array
                                token = block.code[i++];
                                var.modifyer = MOD_ARRAY;
                                Token *expr_start = &block.code[i];
                                size_t exprc=0;
                                COLLECT_EXPR(TOKEN_OSQUAR, TOKEN_CSQUAR, block.code, i);
                                size_t arrlen = evaluate_expr(expr_start, exprc, block.variables, block.depth).num;
                                var.ptr = malloc(get_type_size_in_bytes(var.type)*arrlen);
                                var.size = arrlen;
                            }
                            new_var = true;
                            if(block.variables[block.depth].varc == 0){
                                block.variables[block.depth].variables = malloc(sizeof(Variable));
                            } else {
                                block.variables[block.depth].variables =
                                    realloc(block.variables[block.depth].variables, sizeof(Variable)*(block.variables[block.depth].varc+1));
                            }
                            if(var.modifyer == MOD_ARRAY && var.type!=TYPE_STRING){
                                size_t varc = block.variables[block.depth].varc;
                                block.variables[block.depth].variables[varc] = var;
                                block.variables[block.depth].varc++;
                            } // array initialisation not supported for now
                        } else {
                            var = get_var_by_name(token.sv, block.variables, block.depth);
                        }
                        if(block.code[i+1].type == TOKEN_OSQUAR && !new_var){ // if square bracket after variable name, then it is acces to array.
                            if(var.modifyer!=MOD_ARRAY){
                                TOKENERROR(" Error: trying to use usual variable as array, expected '[', got ");
                            }
                            token = block.code[++i];
                            Token *expr_start = &block.code[++i];
                            size_t exprc=0;
                            COLLECT_EXPR(TOKEN_OSQUAR, TOKEN_CSQUAR, block.code, i);
                            size_t arr_index = evaluate_expr(expr_start, exprc, block.variables, block.depth).num;
                            void *arr_id_ptr=NULL;
                            if(arr_index>=var.size || arr_index<0){
                                printloc(token.loc);
                                logf(" Error: array index %zd is out of range [0;%zd)\n", arr_index, var.size);
                                exit(69);
                            }
                            switch(var.type){
                                case TYPE_I8:
                                    arr_id_ptr = (int8_t*)var.ptr+arr_index;
                                    break;
                                case TYPE_I32:
                                    arr_id_ptr = (int32_t*)var.ptr+arr_index;
                                    break;
                                case TYPE_I64:
                                    arr_id_ptr = (ssize_t*)var.ptr+arr_index;
                                    break;
                                default:
                                    break;
                            }
                            var = (Variable){.name = var.name, .modifyer = MOD_NO_MOD, .type = var.type, .ptr = arr_id_ptr};
                        }
                        Token op_token = block.code[++i];
                        // assignation
                        switch(op_token.type){
                            case TOKEN_EQUAL_SIGN:{
                                token = block.code[++i];
                                int expr_size = 0;
                                Token *expr_start = &block.code[i];
                                while(token.type != TOKEN_SEMICOLON){
                                    token = block.code[++i];
                                    expr_size++;
                                }
                                CBReturn val = evaluate_expr(expr_start, expr_size, block.variables, block.depth);
                                var_cast(&var, val);
                                size_t varc = block.variables[block.depth].varc;
                                if(new_var){
                                    block.variables[block.depth].variables[varc] = var;
                                    block.variables[block.depth].varc++;
                                }
                                break;
                            }
                            case TOKEN_SEMICOLON:{
                                var_cast(&var, (CBReturn){.type=TYPE_NUMERIC, .num=0});
                                size_t varc = block.variables[block.depth].varc;
                                block.variables[block.depth].variables[varc] = var;
                                if(new_var){
                                    block.variables[block.depth].varc++;
                                }
                                break;
                            }
                            default:{
                                    token = block.code[++i];
                                    if(token.type!=TOKEN_EQUAL_SIGN){
                                        TOKENERROR(" Error: expected '=', got ");
                                    }
                                    int expr_size = 0;
                                    Token *expr_start = &block.code[i];
                                    while(token.type != TOKEN_SEMICOLON){
                                        token = block.code[++i];
                                        expr_size++;
                                    }
                                    ssize_t val = evaluate_expr(expr_start, expr_size, block.variables, block.depth).num;
                                    CBReturn tmpret;
                                    tmpret.type = TYPE_NUMERIC;
                                    switch(op_token.type){
                                    case TOKEN_OP_PLUS:
                                        tmpret.num = get_num_value(var, token.loc)+val;
                                        var_cast(&var, tmpret);
                                        break;
                                    case TOKEN_OP_MINUS:
                                        tmpret.num = get_num_value(var, token.loc)+val;
                                        var_cast(&var, tmpret);
                                        break;
                                    case TOKEN_OP_MUL:
                                        tmpret.num = get_num_value(var, token.loc)+val;
                                        var_cast(&var, tmpret);
                                        break;
                                    case TOKEN_OP_DIV:
                                        tmpret.num = get_num_value(var, token.loc)+val;
                                        var_cast(&var, tmpret);
                                        break;
                                    default:
                                        TOKENERROR(" Error: expected '=' or ';', got ");
                                    }
                                    size_t varc = block.variables[block.depth].varc;
                                    if(new_var){
                                        block.variables[block.depth].variables[varc] = var;
                                        block.variables[block.depth].varc++;
                                    }
                            }
                        }
                    } else { // should be function call
                       bool is_stdcall=false;
                        if(SVCMP(block.code[i].sv, "std")==0){
                            if(block.code[i+1].type!=TOKEN_DOT){
                                token = block.code[i+1];
                                TOKENERROR("expected '.' after 'std' in stdcall");
                            }
                            i+=2;
                            is_stdcall = true;
                        }
                        Token *expr_start = &block.code[i];
                        size_t exprc = 0;
                        while(token.type != TOKEN_SEMICOLON){
                            token = block.code[++i];
                            exprc++;
                        }
                        if(is_stdcall){
                            stdcall(expr_start, exprc, block.variables, block.depth);
                        } else {
                            evaluate_expr(expr_start, exprc, block.variables, block.depth);
                        }
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
                        if(block.code[i+1].type == TOKEN_ELSE){ // skip else block
                            while(block.code[i+1].type != TOKEN_CCURLY){
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
                    //COUNT_TOKEN_EXPRC(while_expr_exprc)
                    for(int depth_level=1; token.type != TOKEN_CPAREN||depth_level>0; ){
                        token = block.code[++i];
                        while_expr_exprc++;
                        switch(token.type){
                            case TOKEN_OPAREN:depth_level++;break;
                            case TOKEN_CPAREN:depth_level--;break;
                            default:break;
                        }
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
                CBReturn ret_val = evaluate_expr(expr_start, exprc, block.variables, block.depth);
                if(ret_val.type==TYPE_STRING){
                    ret.string = ret_val.string;
                } else {
                    ret.num = ret_val.num;
                }
                ret.type = ret_val.type;
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
        logf("Error reading provided file. freezing out.\n");
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
                logf(" Error: unimplemented token '%.*s' in global scope\n", SVVARG(token.sv));
        }
    }
    // Interpritation
    fn = functions[hash((SView){"main", 4})%1024];
    if(fn.ret_type==TYPE_NOT_A_TYPE){
        logf("Error: could not find entry point 'fn main'\n");
        exit(69);
    }
    fn.body.depth = 1;
    setup_cbrstd();
    for (size_t i = 0; i < fn.body.exprc; i++) {
    }
    evaluate_code_block(fn.body);
    // FREE !!!
    free(fn.args);
    free(fn.body.code);
    free(code_src);
    return 0;
}
