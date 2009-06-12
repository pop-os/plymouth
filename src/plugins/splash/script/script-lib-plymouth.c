#define _GNU_SOURCE
#include "ply-utils.h"
#include "script.h"
#include "script-execute.h"
#include "script-object.h"
#include "script-lib-plymouth.h" 
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#define STRINGIFY_VAR script_lib_plymouth_string

#include "script-lib-plymouth.string"


static script_return plymouth_set_refresh (script_state* state, void* user_data)
{
 script_lib_plymouth_data_t* data = user_data;
 script_obj* obj = script_obj_hash_get_element (state->local, "function");
 script_obj_deref(&obj);
 if (obj->type == SCRIPT_OBJ_TYPE_FUNCTION){
    data->script_refresh_func = obj;
    }
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_null ()};
}


script_lib_plymouth_data_t* script_lib_plymouth_setup(script_state *state)
{
 script_lib_plymouth_data_t* data = malloc(sizeof(script_lib_plymouth_data_t));
 
 data->script_refresh_func = NULL;
 
 script_add_native_function (state->global, "PlymouthSetRefreshFunction", plymouth_set_refresh, data, "function", NULL);
 data->script_main_op = script_parse_string (script_lib_plymouth_string);
 script_return ret = script_execute(state, data->script_main_op);
 if (ret.object) script_obj_unref(ret.object);                  // Throw anything sent back away

 return data;
}


void script_lib_plymouth_destroy(script_lib_plymouth_data_t* data)
{
 script_parse_op_free (data->script_main_op);
 script_obj_unref(data->script_refresh_func);
 free(data);
}
