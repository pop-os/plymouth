#include "ply-bitarray.h"
#include "ply-scan.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>

static ply_scan_t* ply_scan_new(void)
{
 unsigned char* chars;
 ply_scan_t* scan = calloc(1, sizeof(ply_scan_t));
 scan->tokens = NULL;
 scan->tokencount = 0;
 
 scan->identifier_1st_char = ply_bitarray_new(256);
 scan->identifier_nth_char = ply_bitarray_new(256);
 
 for (chars = (unsigned char*) "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_"; *chars; chars++){
    ply_bitarray_set(scan->identifier_1st_char, *chars);
    }
 for (chars = (unsigned char*) "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789"; *chars; chars++){
    ply_bitarray_set(scan->identifier_nth_char, *chars);
    }
 return scan;
}

ply_scan_t* ply_scan_file(char* filename)
{
 int fd = open(filename, O_RDONLY);
 if (fd<0) return NULL;
 ply_scan_t* scan = ply_scan_new();
 scan->source.fd = fd;
 scan->source_is_file = true;
 ply_scan_get_next_char(scan);
 return scan;
}

ply_scan_t* ply_scan_string(char* string)
{
 ply_scan_t* scan = ply_scan_new();
 scan->source.string = string;
 scan->source_is_file = false;
 ply_scan_get_next_char(scan);
 return scan;
}

void ply_scan_token_clean(ply_scan_token_t* token)
{
 switch (token->type){
    case PLY_SCAN_TOKEN_TYPE_EMPTY:
    case PLY_SCAN_TOKEN_TYPE_EOF:
    case PLY_SCAN_TOKEN_TYPE_INTEGER:
    case PLY_SCAN_TOKEN_TYPE_SYMBOL:
        break;
    case PLY_SCAN_TOKEN_TYPE_IDENTIFIER:
    case PLY_SCAN_TOKEN_TYPE_STRING:
        free(token->data.string);
        break;
    }
 token->type = PLY_SCAN_TOKEN_TYPE_EMPTY;
}

void ply_scan_free(ply_scan_t* scan)
{
 int i;
 if (scan->source_is_file) close(scan->source.fd);
 for (i=0; i<scan->tokencount; i++){
    ply_scan_token_clean(scan->tokens[i]);
    free(scan->tokens[i]);
    }
 ply_bitarray_free(scan->identifier_1st_char);
 ply_bitarray_free(scan->identifier_nth_char);
 free(scan->tokens);
 free(scan);
}

unsigned char ply_scan_get_current_char(ply_scan_t* scan)
{
 return scan->cur_char;
}

unsigned char ply_scan_get_next_char(ply_scan_t* scan)
{
 
 if (scan->source_is_file) {
    int got = read (scan->source.fd, &scan->cur_char, 1);
    if (!got) return 0;                                             // FIXME a better way of doing EOF etc
    }
 else {
    scan->cur_char = *scan->source.string;
    if (scan->cur_char) scan->source.string++;
    }
 // update indexes
 return scan->cur_char;
}

void ply_scan_read_next_token(ply_scan_t* scan, ply_scan_token_t* token)
{
 unsigned char curchar = ply_scan_get_current_char(scan);
 
 while(true){
    if (curchar == ' ')  {curchar = ply_scan_get_next_char(scan); continue;}
    if (curchar == '\n') {curchar = ply_scan_get_next_char(scan); continue;}
    if (curchar == '\t') {curchar = ply_scan_get_next_char(scan); continue;}
    break;
    }
 if (ply_bitarray_lookup(scan->identifier_1st_char, curchar)){
    token->type = PLY_SCAN_TOKEN_TYPE_IDENTIFIER;
    int index = 0;
    token->data.string = NULL;
    do {
        token->data.string = realloc(token->data.string, (index+2)*sizeof(char));
        token->data.string[index] = curchar;
        token->data.string[index+1] = '\0';
        index++;
        curchar = ply_scan_get_next_char(scan);
        }
    while (ply_bitarray_lookup(scan->identifier_nth_char, curchar));
    return;
    }
 if (curchar >= '0' && curchar <= '9'){
    token->type = PLY_SCAN_TOKEN_TYPE_INTEGER;
    long long int value = 0;
    do {
        value *= 10;
        value += curchar - '0';
        curchar = ply_scan_get_next_char(scan);
        }
    while (curchar >= '0' && curchar <= '9');
    token->data.integer = value;
    return;
    }
 
 if (!curchar) {
    token->type = PLY_SCAN_TOKEN_TYPE_EOF;
    return;
    }
 
 if (curchar == '"') {
    token->type = PLY_SCAN_TOKEN_TYPE_STRING;
    int index = 0;
    token->data.string = malloc(sizeof(char));
    token->data.string[0] = '\0';

    while (1){
        curchar = ply_scan_get_next_char(scan);
        if (curchar == '"') break;
        assert(curchar != '\0');
        if (curchar == '\\') {
            curchar = ply_scan_get_next_char(scan);
            switch (curchar){
                case 'n':
                    curchar = '\n';
                    break;
                case '0':
                    curchar = '\0';
                    break;
                case '"':
                    curchar = '\"';
                    break;
                default:
                    break;
                }
            }
        token->data.string = realloc(token->data.string, (index+2)*sizeof(char));
        token->data.string[index] = curchar;
        token->data.string[index+1] = '\0';
        index++;
        }
    ply_scan_get_next_char(scan);
    return;
    }
 
 // all other
 token->type = PLY_SCAN_TOKEN_TYPE_SYMBOL;
 token->data.symbol = curchar;
 ply_scan_get_next_char(scan);
 return;
}


static ply_scan_token_t* ply_scan_peek_token(ply_scan_t* scan, int n)
{
 int i;
 if (scan->tokencount <= n){
    scan->tokens = realloc(scan->tokens, (n+1)*sizeof(ply_scan_token_t*));
    for (i = scan->tokencount; i<=n; i++){                                      // FIXME warning about possibely inifnite loop
        scan->tokens[i] = malloc(sizeof(ply_scan_token_t));
        scan->tokens[i]->type = PLY_SCAN_TOKEN_TYPE_EMPTY;
        }
    scan->tokencount = n+1;
    }
 
 if (scan->tokens[n]->type == PLY_SCAN_TOKEN_TYPE_EMPTY){
    if (n > 0 && scan->tokens[n-1]->type == PLY_SCAN_TOKEN_TYPE_EMPTY)
        ply_scan_peek_token(scan, n-1);
    ply_scan_read_next_token(scan, scan->tokens[n]);
//     printf("%d:", n);
//     switch (scan->tokens[n]->type){
//         case PLY_SCAN_TOKEN_TYPE_STRING:
//             printf("\"%s\"\n", scan->tokens[n]->data.string);
//             break;
//         case PLY_SCAN_TOKEN_TYPE_IDENTIFIER:
//             printf("%s\n", scan->tokens[n]->data.string);
//             break;
//         case PLY_SCAN_TOKEN_TYPE_SYMBOL:
//             printf("'%c'\n", scan->tokens[n]->data.symbol);
//             break;
//         case PLY_SCAN_TOKEN_TYPE_INTEGER:
//             printf("%d\n", (int)scan->tokens[n]->data.integer);
//             break;
//         }
    } 
 return scan->tokens[n];
}

ply_scan_token_t* ply_scan_get_next_token(ply_scan_t* scan)
{
 int i;
 ply_scan_token_clean(scan->tokens[0]);
 for (i=0; i<(scan->tokencount-1); i++){
    *scan->tokens[i] = *scan->tokens[i+1];
    }
 scan->tokens[(scan->tokencount-1)]->type = PLY_SCAN_TOKEN_TYPE_EMPTY;
 return ply_scan_peek_token(scan, 0);
}

ply_scan_token_t* ply_scan_get_current_token(ply_scan_t* scan)
{
 return ply_scan_peek_token(scan, 0);
}

ply_scan_token_t* ply_scan_peek_next_token(ply_scan_t* scan)
{
 return ply_scan_peek_token(scan, 1);
}


