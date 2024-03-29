#include "types.h"
#include "functions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define sleep Sleep
#else
#include <unistd.h>
#endif

CBReturn cbrstd_print(Token *expr, size_t call_exprc, Variables *variables, size_t depth){
    CBReturn ret = {.returned=false, .type=0, .num=0};
    size_t i = 0;
    Token token = expr[i];
    while(i<call_exprc){
        switch(token.type){
            case TOKEN_STR_LITERAL:
                printf("%.*s", SVVARG(token.sv));
                break;
            case TOKEN_NUMERIC:
                printf("%ld", SVTOL(token.sv));
                break;
            case TOKEN_NAME:
                {
                    Variable var = get_var_by_name(token.sv, variables, depth);
                    if(var.name.data!=NULL){ // if token is variable, get numeric of value and exit
                        if(var.modifyer==MOD_ARRAY && var.type==TYPE_STRING){
                            printf("%.*s", (int)var.size, (char*)var.ptr);
                            break;
                        }
                        if(var.modifyer==MOD_ARRAY){
                            if(expr[i+1].type!=TOKEN_OSQUAR){
                                printf("{");
                                for(size_t j=0; j<var.size-1; j++){
                                    printf("%zd, ", get_arr_num_value(var, j));
                                }
                                printf("%zd", get_arr_num_value(var, var.size-1));
                                printf("}");
                                break;
                            }
                            i++;
                            Token *expr_start = &expr[++i];
                            size_t exprc;
                            COLLECT_EXPR(TOKEN_OSQUAR, TOKEN_CSQUAR, expr, i);
                            i++;
                            ssize_t index = evaluate_expr(expr_start, exprc, variables, depth).num;
                            printf("%zd", get_arr_num_value(var, index));
                            break;
                        }
                        printf("%zd", get_num_value(var, token.loc));
                        break;
                    }
                    // if token is not var name, then it should be function
                    // so we will grep fucntion call and call it a day
                    Token *expr_start = &expr[i];
                    size_t exprc=0;
                    COLLECT_EXPR(TOKEN_OPAREN, TOKEN_CPAREN, expr, i);
                    ssize_t tmp = evaluate_expr(expr_start, exprc, variables, depth).num;
                    printf("%zd", tmp);
                }
                break;
            default:
                TOKENERROR(" Error: You dumbass forgot semicolon, got ");
        }
        token = expr[++i];
    }
    return ret;
}

CBReturn cbrstd_dprint(Token *expr, size_t call_exprc, Variables *variables, size_t depth){
    CBReturn ret = {.returned=false, .type=0, .num=0};
    size_t i=0;
    Token token = expr[i];
    Variable var;
    while(i<call_exprc){
        switch(token.type){
            case TOKEN_NAME:
                var = get_var_by_name(token.sv, variables, depth);
                if(var.modifyer==MOD_ARRAY){
                    //debug_variable(var);
                    printf("%s %.*s[%zu] = {", TYPE_TO_STR[var.type], SVVARG(var.name), var.size);
                    for(size_t j=0; j<var.size-1; j++){
                        printf("%zd, ", get_arr_num_value(var, j));
                    }
                    printf("%zd", get_arr_num_value(var, var.size-1));
                    puts("}");
                } else {
                    printf("%s %.*s = %zd\n", TYPE_TO_STR[var.type],
                            SVVARG(var.name), get_num_value(var, token.loc));
                }
                break;
            default:
                TOKENERROR(" Error: 'dprint' supports only variables, got ");
        }
        token = expr[++i];
    }
    return ret;
}

CBReturn cbrstd_readTo(Token *expr, size_t call_exprc, Variables *variables, size_t depth){
    CBReturn ret = {.returned=false, .type=0, .num=0};
    size_t i=0;
    Token token = expr[i];
    int mlced=256;
    char *str_input=malloc(mlced);
    char *str_input_copy=str_input;
    fgets(str_input, mlced, stdin);
    while(i<call_exprc){
        switch(token.type){
            case TOKEN_NAME:{
                ssize_t scanned = strtol(str_input, &str_input, 10);
                Variable var = get_var_by_name(token.sv, variables, depth);
                if(var.modifyer==MOD_ARRAY){
                    TOKENERROR(" Error: readTo does notr support arrays, got ")
                }
                CBReturn tmpret = {.type=TYPE_I64, .num=scanned};
                var_cast(&var, tmpret);
                break;
            }
            default:
                TOKENERROR(" Error: 'readTo' supports only variables, got ");
        }
        token = expr[++i];
    }
    free(str_input_copy);
    return ret;
}

CBReturn cbrstd_readlnTo(Token *expr, size_t call_exprc, Variables *variables, size_t depth){
    CBReturn ret = {.returned=false, .type=0, .num=0};
    size_t i=0;
    Token token = expr[i];
    int mlced=256;
    char *str_input=malloc(mlced);
    char *str_input_copy=str_input;
    fgets(str_input, mlced, stdin);
    while(i<call_exprc){
        switch(token.type){
            case TOKEN_NAME:{
                Variable var = get_var_by_name(token.sv, variables, depth);
                ssize_t scanned = strtol(str_input, &str_input, 10);
                if(var.modifyer==MOD_ARRAY){
                    token = expr[i++];
                    Token *expr_start = &expr[i];
                    size_t expr_size=0;
                    while(token.type != TOKEN_CSQUAR){
                        token = expr[++i];
                        expr_size++;
                    }
                    size_t arr_index = evaluate_expr(expr_start, expr_size, variables, depth).num;
                    void *arr_id_ptr=NULL;
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
                CBReturn tmpret = {.type=var.type, .num=scanned};
                var_cast(&var, tmpret);
                break;
            }
            default:
                TOKENERROR(" Error: 'readTo' supports only variables, got ");
        }
        token = expr[++i];
    }
    free(str_input_copy);
    return ret;
}

CBReturn cbrstd_sleep(Token *expr, size_t call_exprc, Variables *variables, size_t depth){
    (void) expr;
    (void) call_exprc;
    (void) variables;
    (void) depth;
    CBReturn ret = {.returned=false, .type=0, .num=0};
    Token token = expr[0];
    if(expr[0].type!=TOKEN_NUMERIC){
        RUNTIMEERROR(" Error: only numerics are supported for std 'sleep' function now")
    }
    sleep(SVTOL(expr[0].sv));
    return ret;
}

CBReturn cbrstd_random(Token *expr, size_t call_exprc, Variables *variables, size_t depth){
    (void) expr;
    (void) call_exprc;
    (void) variables;
    (void) depth;
    CBReturn ret = {.returned=true, .type=TYPE_I32, .num=0};
    ret.num = random()&0xffffffff;
    return ret;
}
#define STD_CAP 1024
CBReturn (*cbrstd_functions[STD_CAP]) (Token*, size_t, Variables*, size_t);

unsigned long char_hash(char *str){
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

void setup_cbrstd(void){
    srandom(time(NULL));
    cbrstd_functions[char_hash("print") % STD_CAP] = &cbrstd_print;
    cbrstd_functions[char_hash("dprint") % STD_CAP] = &cbrstd_dprint;
    cbrstd_functions[char_hash("readlnTo") % STD_CAP] = &cbrstd_readlnTo;
    cbrstd_functions[char_hash("readTo") % STD_CAP] = &cbrstd_readTo;
    cbrstd_functions[char_hash("sleep") % STD_CAP] = &cbrstd_sleep;
    cbrstd_functions[char_hash("random") % STD_CAP] = &cbrstd_random;
}

CBReturn stdcall(Token *expr, size_t call_exprc, Variables *variables, size_t depth){
    size_t i = 0;
    Token token = expr[i];
    if(cbrstd_functions[hash(token.sv)%STD_CAP]==NULL){
        /* printloc(global_location); */
        logf("Error: unknown stdcall %.*s\n", SVVARG(token.sv));
        exit(69);
    }
    return cbrstd_functions[hash(token.sv)%STD_CAP](expr+1, call_exprc-1, variables, depth);
}
