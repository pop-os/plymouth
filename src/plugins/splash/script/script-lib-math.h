#ifndef SCRIPT_LIB_MATH
#define SCRIPT_LIB_MATH

#include "script.h"

typedef struct
{
  script_op      *script_main_op;
} script_lib_math_data_t;


script_lib_math_data_t* script_lib_math_setup(script_state *state);
void script_lib_math_destroy(script_lib_math_data_t* data);

#endif /* SCRIPT_LIB_MATH */
