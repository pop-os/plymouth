#define _GNU_SOURCE
#include "ply-utils.h"
#include "script.h"
#include "script-parse.h"
#include "script-execute.h"
#include "script-object.h"
#include "script-lib-math.h" 
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#define STRINGIFY_VAR script_lib_math_string

#include "script-lib-math.string"



script_lib_math_data_t* script_lib_math_setup(script_state *state)
{
 script_lib_math_data_t* data = malloc(sizeof(script_lib_math_data_t));
 
 data->script_main_op = script_parse_string (script_lib_math_string);
 script_return ret = script_execute(state, data->script_main_op);
 if (ret.object) script_obj_unref(ret.object);                  // Throw anything sent back away

 return data;
}


void script_lib_math_destroy(script_lib_math_data_t* data)
{
 script_parse_op_free (data->script_main_op);
 free(data);
}

 
