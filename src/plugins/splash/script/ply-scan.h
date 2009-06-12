#ifndef PLY_SCAN_H
#define PLY_SCAN_H

#include "ply-bitarray.h"
#include <stdbool.h>

typedef enum
{
 PLY_SCAN_TOKEN_TYPE_EMPTY,
 PLY_SCAN_TOKEN_TYPE_EOF,
 PLY_SCAN_TOKEN_TYPE_INTEGER,
 PLY_SCAN_TOKEN_TYPE_IDENTIFIER,
 PLY_SCAN_TOKEN_TYPE_STRING,
 PLY_SCAN_TOKEN_TYPE_SYMBOL,
} ply_scan_token_type_t;

typedef struct
{
 ply_scan_token_type_t type;
 union
 {
    char* string;
    char symbol;
    long long int integer;
 } data;
} ply_scan_token_t;

typedef struct
{
 union
 {
  int fd;
  char* string;
 } source;
  unsigned char cur_char;
  ply_bitarray_t *identifier_1st_char;
  ply_bitarray_t *identifier_nth_char;
  int tokencount;
  ply_scan_token_t **tokens;
  bool source_is_file;
} ply_scan_t;

ply_scan_t* ply_scan_file(char* filename);
ply_scan_t* ply_scan_string(char* string);
void ply_scan_token_clean(ply_scan_token_t* token);
void ply_scan_free(ply_scan_t* scan);
unsigned char ply_scan_get_current_char(ply_scan_t* scan);
unsigned char ply_scan_get_next_char(ply_scan_t* scan);
ply_scan_token_t* ply_scan_get_current_token(ply_scan_t* scan);
ply_scan_token_t* ply_scan_get_next_token(ply_scan_t* scan);
ply_scan_token_t* ply_scan_peek_next_token(ply_scan_t* scan);
void ply_scan_read_next_token(ply_scan_t* scan, ply_scan_token_t* token);

#endif /* PLY_SCAN_H */
