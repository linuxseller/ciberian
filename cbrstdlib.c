#include "types.h"
#include "functions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

void cbrstd_print(Token *expr, size_t call_exprc, Variables *variables, size_t depth){
    size_t i = 0;
    Token token = expr[i];
    while(i<call_exprc){
        switch(token.type){
            case TOKEN_STR_LITERAL:
                printf("%.*s", SVVARG(token.sv));
                break;
            case TOKEN_NAME:
                {
                    Variable var = get_var_by_name(token.sv, variables, depth);
                    if(var.name.data!=NULL){ // if token is variable, get numeric of value and exit
                        printf("%zd", get_num_value(var));
                        break;
                    }
                    // if token is not var name, then it should be function
                    // so we will grep fucntion call and call it a day
                    Token *expr_start = &expr[i];
                    int exprc=0;
                    for(int depth_level=0; token.type!=TOKEN_CPAREN && depth_level!=0;){
                        token = expr[++i];
                        printf("d_level %d\n", depth_level);
                        exprc++;
                        switch(token.type){
                            case TOKEN_OPAREN:depth_level++;break;
                            case TOKEN_CPAREN:depth_level--;break;
                            default:break;
                        }   
                    }
                    int64_t tmp = evaluate_expr(expr_start, exprc, variables, depth);
                    printf("%zd", tmp);
                }
                break;
            case TOKEN_NUMERIC:
                printf("%ld", SVTOL(token.sv));
                break;
            default:
                TOKENERROR(" Error: You dumbass forgot semicolon");
        }
        token = expr[++i];
    }
}

void cbrstd_dprint(Token *expr, size_t call_exprc, Variables *variables, size_t depth){
    size_t i=0;
    Token token = expr[i];
    while(i<call_exprc){
        switch(token.type){
            case TOKEN_NAME:
                printf("%s %.*s = %zd\n",
                        TYPE_TO_STR[get_var_by_name(token.sv, variables, depth).type], 
                        SVVARG(token.sv),
                        get_num_value(get_var_by_name(token.sv, variables, depth)));
                break;
            default:
                TOKENERROR(" Error: 'dprint' supports only variables, got ");
        }
        token = expr[++i];
    }
}

void cbrstd_readlnTo(Token *expr, size_t call_exprc, Variables *variables, size_t depth){
    size_t i=0;
    Token token = expr[i];
    int mlced=256;
    char *str_input=malloc(mlced);
    char *str_input_copy=str_input;
    fgets(str_input, mlced, stdin);
    while(i<call_exprc){
        switch(token.type){
            case TOKEN_NAME:{
                int64_t scanned = strtol(str_input, &str_input, 10);
                Variable var = get_var_by_name(token.sv, variables, depth);
                var_num_cast(&var, scanned);
                break;
            }
            default:
                TOKENERROR(" Error: 'readTo' supports only variables, got ");
        }
        token = expr[++i];
    }
    free(str_input_copy);
}
void (*cbrstd_functions[1024]) (Token*, size_t, Variables*, size_t);
void setup_cbrstd(void){
    cbrstd_functions[0] = &cbrstd_print;
    cbrstd_functions[1] = &cbrstd_dprint;
    cbrstd_functions[2] = &cbrstd_readlnTo;
}

CBReturn stdcall(Token *expr, size_t call_exprc, Variables *variables, size_t depth){
    size_t fn_id;
    size_t i = 0;
    Token token = expr[i];
    if(SVCMP(token.sv, "print") == 0) {
        fn_id = 0;
    } else if(SVCMP(token.sv, "dprint") == 0) {
        fn_id = 1;
    }else if(SVCMP(token.sv, "readlnTo") == 0) {
        fn_id = 2;
    } else {
        TOKENERROR(" You stupid bitch, stop using unknown shit, got ");
    }
    cbrstd_functions[fn_id](expr+1, call_exprc-1, variables, depth);
    return (CBReturn){.returned=false};
}
