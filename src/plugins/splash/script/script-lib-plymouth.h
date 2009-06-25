#ifndef SCRIPT_LIB_PLYMOUTH
#define SCRIPT_LIB_PLYMOUTH

#include "script.h"

typedef struct
{
  script_op      *script_main_op;
  script_obj     *script_refresh_func;
} script_lib_plymouth_data_t;


script_lib_plymouth_data_t* script_lib_plymouth_setup(script_state *state);
void script_lib_plymouth_destroy(script_lib_plymouth_data_t* data);

void script_lib_plymouth_on_refresh(script_state* state, script_lib_plymouth_data_t* data);

#endif /* SCRIPT_LIB_PLYMOUTH */
