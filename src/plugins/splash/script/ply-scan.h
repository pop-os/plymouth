/* ply-scan.h - lexical scanner
 *
 * Copyright (C) 2009 Charlie Brej <cbrej@cs.man.ac.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by: Charlie Brej <cbrej@cs.man.ac.uk>
 */
#ifndef PLY_SCAN_H
#define PLY_SCAN_H

#include "ply-bitarray.h"
#include <stdbool.h>

typedef enum
{
  PLY_SCAN_TOKEN_TYPE_EMPTY,
  PLY_SCAN_TOKEN_TYPE_EOF,
  PLY_SCAN_TOKEN_TYPE_INTEGER,
  PLY_SCAN_TOKEN_TYPE_FLOAT,
  PLY_SCAN_TOKEN_TYPE_IDENTIFIER,
  PLY_SCAN_TOKEN_TYPE_STRING,
  PLY_SCAN_TOKEN_TYPE_SYMBOL,
  PLY_SCAN_TOKEN_TYPE_COMMENT,
  PLY_SCAN_TOKEN_TYPE_ERROR,
} ply_scan_token_type_t;

typedef struct
{
  ply_scan_token_type_t type;
  union
  {
    char *string;
    char symbol;
    long long int integer;
    double floatpoint;
  } data;
  int whitespace;
  int line_index;
  int column_index;
} ply_scan_token_t;

typedef struct
{
  union
  {
    int fd;
    const char *string;
  } source;
  unsigned char cur_char;
  ply_bitarray_t *identifier_1st_char;
  ply_bitarray_t *identifier_nth_char;
  int tokencount;
  ply_scan_token_t **tokens;
  int line_index;
  int column_index;
  bool source_is_file;
} ply_scan_t;


#define ply_scan_token_is_symbol(__token) \
      (__token->type == PLY_SCAN_TOKEN_TYPE_SYMBOL)
#define ply_scan_token_is_symbol_of_value(__token,__value) \
      (__token->type == PLY_SCAN_TOKEN_TYPE_SYMBOL \
      && __token->data.symbol == __value)
#define ply_scan_token_is_identifier(__token) \
      (__token->type == PLY_SCAN_TOKEN_TYPE_IDENTIFIER)
#define ply_scan_token_is_identifier_of_value(__token,__value) \
      (__token->type == PLY_SCAN_TOKEN_TYPE_IDENTIFIER \
      && !strcmp(__token->data.string, __value))
#define ply_scan_token_is_integer(__token) \
      (__token->type == PLY_SCAN_TOKEN_TYPE_INTEGER)
#define ply_scan_token_is_string(__token) \
      (__token->type == PLY_SCAN_TOKEN_TYPE_STRING)
#define ply_scan_token_is_float(__token) \
      (__token->type == PLY_SCAN_TOKEN_TYPE_FLOAT)




ply_scan_t *ply_scan_file (const char *filename);
ply_scan_t *ply_scan_string (const char *string);
void ply_scan_token_clean (ply_scan_token_t *token);
void ply_scan_free (ply_scan_t *scan);
unsigned char ply_scan_get_current_char (ply_scan_t *scan);
unsigned char ply_scan_get_next_char (ply_scan_t *scan);
ply_scan_token_t *ply_scan_get_current_token (ply_scan_t *scan);
ply_scan_token_t *ply_scan_get_next_token (ply_scan_t *scan);
ply_scan_token_t *ply_scan_peek_next_token (ply_scan_t *scan);
void ply_scan_read_next_token (ply_scan_t       *scan,
                               ply_scan_token_t *token);


#endif /* PLY_SCAN_H */
