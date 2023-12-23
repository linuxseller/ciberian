#include "types.h"

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
        case '[':
            token_type = TOKEN_OSQUAR; break;
        case ']':
            token_type = TOKEN_CSQUAR; break;
        case ';':
            token_type = TOKEN_SEMICOLON; break;
        case ':':
            token_type = TOKEN_RETURN_COLON; break;
        case ',':
            token_type = TOKEN_COMMA; break;
        case '.':
            token_type = TOKEN_DOT; break;
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
