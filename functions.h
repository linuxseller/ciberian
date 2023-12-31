#include "types.h"
#ifndef _FUNCTIONS_H
#define _FUNCTIONS_H
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
int64_t get_num_value(Variable var, Location loc);
int64_t get_arr_num_value(Variable var, size_t index);
Variable get_var_by_name(SView sv, Variables *variables, int64_t depth);
int64_t evaluate_expr(Token *expr, int64_t expr_size, Variables *variables, size_t depth);
bool evaluate_bool_expr(Token *expr, int64_t expr_size, Variables *variables, size_t depth);
CBReturn evaluate_code_block(CodeBlock block);
CBReturn stdcall(Token *expr, size_t exprc, Variables *variables, size_t depth);
#endif
