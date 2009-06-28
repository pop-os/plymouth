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
#include <math.h>

#include "config.h"

#define STRINGIFY_VAR script_lib_math_string

#include "script-lib-math.string"


static script_return script_lib_math_single_float_function (script_state* state, void* user_data)
{
 float (*function) (float) = user_data;
 script_obj* obj = script_obj_hash_get_element (state->local, "value");
 script_obj* reply;
 float reply_float = function(script_obj_as_float(obj));
 script_obj_unref(obj);
 if (isnan(reply_float)) reply = script_obj_new_null ();
 else                    reply = script_obj_new_float (reply_float);
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, reply};
}

script_lib_math_data_t* script_lib_math_setup(script_state *state)
{
 script_lib_math_data_t* data = malloc(sizeof(script_lib_math_data_t));
 
 script_add_native_function (state->global, "MathCos", script_lib_math_single_float_function, cosf, "value", NULL);
 script_add_native_function (state->global, "MathSin", script_lib_math_single_float_function, sinf, "value", NULL);
 script_add_native_function (state->global, "MathTan", script_lib_math_single_float_function, tanf, "value", NULL);
 script_add_native_function (state->global, "MathSqrt",script_lib_math_single_float_function, sqrtf,"value", NULL);
 
 data->script_main_op = script_parse_string (script_lib_math_string);
 script_return ret = script_execute(state, data->script_main_op);
 script_obj_unref(ret.object);

 return data;
}


void script_lib_math_destroy(script_lib_math_data_t* data)
{
 script_parse_op_free (data->script_main_op);
 free(data);
}

 
