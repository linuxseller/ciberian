/*
 * fn main {
 *     print "Hello"
 * }
 */

#define CURR this->source[this->pos]
#define SVCMP(sv, b) strncmp(b, sv.data, sv.size)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

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

enum TokenEnum {
    TOKEN_FN_DECL,
    TOKEN_NAME,
    TOKEN_FN_CALL,
    TOKEN_OCURLY,
    TOKEN_CCURLY,
    TOKEN_STR_LITERAL,
    TOKEN_SEMICOLON
};

typedef struct {
    enum TokenEnum type;
    SView svstr;
    Location loc;
} Token;

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
    while(CURR!='\0' && isspace(CURR)){
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
    /* comment handling
    while(CURR!='\0'){
      chop_char(this); 
      drop_line(this);
      trim_left(this);
    }
    */
    SView sv  = {0};
    sv.data = this->source+this->pos;

    Location loc = {.col                   = this->pos,
                    .row       = this->line,
                    .file_path = this->file_name};
    char first_char = CURR;
    size_t start = this->pos;
    enum TokenEnum token_type;
    if(isalpha(first_char)){
        while(CURR!='\0' && isalnum(CURR)) {
           chop_char(this); 
        }
        sv.size = this->pos - start;
        if(SVCMP(sv, "fn")==0){
            token_type = TOKEN_FN_DECL;
        } else {
            token_type = TOKEN_NAME;
        }
        return (Token){.loc=loc, .svstr=sv, .type=token_type};
    }
    switch(first_char){
        case '{':
            token_type = TOKEN_OCURLY; sv.size=1; break;
        case '}':
            token_type = TOKEN_CCURLY; sv.size=1; break;
        case ';':
            token_type = TOKEN_SEMICOLON; sv.size=1; break;
        case '"': { // parsing string literal
                    chop_char(this);
                    token_type = TOKEN_STR_LITERAL; 
                    while(CURR != '"'){
                        chop_char(this);
                    }
                    sv.data++;
                    sv.size = this->pos - start-1;
                    break;
                  } // token string literal 
    }
    chop_char(this);
    return (Token){.loc=loc, .svstr=sv, .type=token_type};
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
    char *code_src = calloc(code_file_size, sizeof(*code_src));
    fread(code_src, code_file_size, 1, code_file);
    

    Lexer lexer = { .file_name = code_file_name,
                    .line = 1, .bol = 0, .pos = 0,
                    .source = code_src};
    Token tokens[1024];
    for(int i=0; lexer.source[lexer.pos+1]!=0; i++){
        tokens[i] = next_token(&lexer);
    }
    if(verbose){
        printf("src file name: %s\n", code_file_name);
        printf("file length is %d ascii symbols\n", code_file_size);
        printf("code is:\n'''\n%s\n'''\n", code_src);
        for(int i=0; i<10; i++){
            switch(tokens[i].type){
                case TOKEN_OCURLY:
                    printf("OCURLY: "); break;
                case TOKEN_CCURLY:
                    printf("CCURLY: "); break;
                case TOKEN_STR_LITERAL:
                    printf("STR_LIT: "); break;
                case TOKEN_SEMICOLON:
                    printf("SEMICOLON: "); break;
                case TOKEN_FN_DECL:
                    printf("FN: "); break;
                case TOKEN_NAME:
                    printf("NAME: "); break;
                default:
                    printf("ERROR: unsupported token\n");
                    exit(1);
            }
            printf("%.*s\n", (int)tokens[i].svstr.size, tokens[i].svstr.data);
        }
    }
    

    // interpritation
    size_t token_id = 0;
    Token token = tokens[token_id];
    while(token.type != TOKEN_FN_DECL && strncmp("main", token.svstr.data, token.svstr.size)){
        token = tokens[token_id++];
    }
    size_t entry_point_id = ++token_id;
    bool main_exit = false;
    for(size_t i=entry_point_id; !main_exit; i++){
        token = tokens[i];
        int depth_level=1;
        switch(token.type){
            case TOKEN_NAME:
                {
                    if(SVCMP(token.svstr, "print")==0){
                        token = tokens[++i];
                        while(token.type!=TOKEN_SEMICOLON){
                            printf("%.*s", (int)token.svstr.size, token.svstr.data);
                            token = tokens[++i];
                        }
                    }
                }; break;
            case TOKEN_OCURLY:
                depth_level++;break;
            case TOKEN_CCURLY:
                if(--depth_level==0){
                    main_exit=true;
                } break;
            default: printf("Nothing changed..,\n");
        }
    }
    return 0;
}
