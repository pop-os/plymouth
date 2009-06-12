#ifndef SCRIPT_PARSE
#define SCRIPT_PARSE

#include "script.h"

script_op* script_parse_file (char* filename);
script_op* script_parse_string (char* string);
void script_parse_op_free (script_op* op);



#endif /* SCRIPT_PARSE */
