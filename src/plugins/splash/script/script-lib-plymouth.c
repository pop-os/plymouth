#define _GNU_SOURCE
#include "ply-utils.h"
#include "script.h"
#include "script-parse.h"
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


static script_return plymouth_set_function (script_state* state, void* user_data)
{
 script_obj** script_func = user_data;
 script_obj* obj = script_obj_hash_get_element (state->local, "function");
 script_obj_deref(&obj);
 script_obj_unref(*script_func);
 
 if (obj->type == SCRIPT_OBJ_TYPE_FUNCTION){
    *script_func = obj;
    }
 else {
    *script_func = NULL;
    script_obj_unref(obj);
    }

 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_null ()};
}


script_lib_plymouth_data_t* script_lib_plymouth_setup(script_state *state)
{
 script_lib_plymouth_data_t* data = malloc(sizeof(script_lib_plymouth_data_t));
 
 data->script_refresh_func = NULL;
 data->script_boot_progress_func = NULL;
 data->script_root_mounted_func = NULL;
 data->script_keyboard_input_func = NULL;
 data->script_update_status_func = NULL;
 data->script_display_normal_func = NULL;
 data->script_display_password_func = NULL;
 data->script_display_question_func = NULL;
 
 script_add_native_function (state->global, "PlymouthSetRefreshFunction",         plymouth_set_function, &data->script_refresh_func,          "function", NULL);
 script_add_native_function (state->global, "PlymouthSetBootProgressFunction",    plymouth_set_function, &data->script_boot_progress_func,    "function", NULL);
 script_add_native_function (state->global, "PlymouthSetRootMountedFunction",     plymouth_set_function, &data->script_root_mounted_func,     "function", NULL);
 script_add_native_function (state->global, "PlymouthSetKeyboardInputFunction",   plymouth_set_function, &data->script_keyboard_input_func,   "function", NULL);
 script_add_native_function (state->global, "PlymouthSetUpdateStatusFunction",    plymouth_set_function, &data->script_update_status_func,    "function", NULL);
 script_add_native_function (state->global, "PlymouthSetDisplayNormalFunction",   plymouth_set_function, &data->script_display_normal_func,   "function", NULL);
 script_add_native_function (state->global, "PlymouthSetDisplayPasswordFunction", plymouth_set_function, &data->script_display_password_func, "function", NULL);
 script_add_native_function (state->global, "PlymouthSetDisplayQuestionFunction", plymouth_set_function, &data->script_display_question_func, "function", NULL);
 data->script_main_op = script_parse_string (script_lib_plymouth_string);
 script_return ret = script_execute(state, data->script_main_op);
 script_obj_unref(ret.object);                  // Throw anything sent back away

 return data;
}


void script_lib_plymouth_destroy(script_lib_plymouth_data_t* data)
{
 script_parse_op_free (data->script_main_op);
 script_obj_unref(data->script_refresh_func);
 script_obj_unref(data->script_boot_progress_func);
 script_obj_unref(data->script_root_mounted_func);
 script_obj_unref(data->script_keyboard_input_func);
 script_obj_unref(data->script_update_status_func);
 script_obj_unref(data->script_display_normal_func);
 script_obj_unref(data->script_display_password_func);
 script_obj_unref(data->script_display_question_func);
 free(data);
}

void script_lib_plymouth_on_refresh(script_state* state, script_lib_plymouth_data_t* data)
{
 script_obj* refresh_func_obj = data->script_refresh_func;
 if (refresh_func_obj &&  refresh_func_obj->type == SCRIPT_OBJ_TYPE_FUNCTION){
    script_return ret = script_execute_function (state, refresh_func_obj->data.function, NULL);
    script_obj_unref(ret.object);
    }
}

void script_lib_plymouth_on_boot_progress(script_state* state, script_lib_plymouth_data_t* data, float duration, float progress)
{
 script_obj* boot_progress_func_obj = data->script_boot_progress_func;
 if (boot_progress_func_obj &&  boot_progress_func_obj->type == SCRIPT_OBJ_TYPE_FUNCTION){
    script_obj* duration_obj = script_obj_new_float (duration);
    script_obj* progress_obj = script_obj_new_float (progress);
    script_return ret = script_execute_function (state, boot_progress_func_obj->data.function, duration_obj, progress_obj, NULL);
    script_obj_unref(ret.object);
    script_obj_unref(duration_obj);
    script_obj_unref(progress_obj);
    }
}

void script_lib_plymouth_on_root_mounted(script_state* state, script_lib_plymouth_data_t* data)
{
 script_obj* root_mounted_func_obj = data->script_root_mounted_func;
 if (root_mounted_func_obj &&  root_mounted_func_obj->type == SCRIPT_OBJ_TYPE_FUNCTION){
    script_return ret = script_execute_function (state, root_mounted_func_obj->data.function, NULL);
    script_obj_unref(ret.object);
    }
}

void script_lib_plymouth_on_keyboard_input(script_state* state, script_lib_plymouth_data_t* data, const char* keyboard_input)
{
 script_obj* keyboard_input_func_obj = data->script_keyboard_input_func;
 if (keyboard_input_func_obj &&  keyboard_input_func_obj->type == SCRIPT_OBJ_TYPE_FUNCTION){
    script_obj* keyboard_input_obj = script_obj_new_string (keyboard_input);
    script_return ret = script_execute_function (state, keyboard_input_func_obj->data.function, keyboard_input_obj, NULL);
    script_obj_unref(keyboard_input_obj);
    script_obj_unref(ret.object);
    }
}


void script_lib_plymouth_on_update_status(script_state* state, script_lib_plymouth_data_t* data, const char* new_status)
{
 script_obj* update_status_func_obj = data->script_update_status_func;
 if (update_status_func_obj &&  update_status_func_obj->type == SCRIPT_OBJ_TYPE_FUNCTION){
    script_obj* new_status_obj = script_obj_new_string (new_status);
    script_return ret = script_execute_function (state, update_status_func_obj->data.function, new_status_obj, NULL);
    script_obj_unref(new_status_obj);
    script_obj_unref(ret.object);
    }
}


void script_lib_plymouth_on_display_normal(script_state* state, script_lib_plymouth_data_t* data)
{
 script_obj* display_normal_func_obj = data->script_display_normal_func;
 if (display_normal_func_obj &&  display_normal_func_obj->type == SCRIPT_OBJ_TYPE_FUNCTION){
    script_return ret = script_execute_function (state, display_normal_func_obj->data.function, NULL);
    script_obj_unref(ret.object);
    }
}


void script_lib_plymouth_on_display_password(script_state* state, script_lib_plymouth_data_t* data, const char *prompt, int bullets)
{
 script_obj* display_password_func_obj = data->script_display_password_func;
 if (display_password_func_obj &&  display_password_func_obj->type == SCRIPT_OBJ_TYPE_FUNCTION){
    script_obj* prompt_obj = script_obj_new_string (prompt);
    script_obj* bullets_obj = script_obj_new_int (bullets);
    script_return ret = script_execute_function (state, display_password_func_obj->data.function, prompt_obj, bullets_obj, NULL);
    script_obj_unref(prompt_obj);
    script_obj_unref(bullets_obj);
    script_obj_unref(ret.object);
    }
}

void script_lib_plymouth_on_display_question(script_state* state, script_lib_plymouth_data_t* data, const char *prompt, const char *entry_text)
{
 script_obj* display_question_func_obj = data->script_display_question_func;
 if (display_question_func_obj &&  display_question_func_obj->type == SCRIPT_OBJ_TYPE_FUNCTION){
    script_obj* prompt_obj = script_obj_new_string (prompt);
    script_obj* entry_text_obj = script_obj_new_string (entry_text);
    script_return ret = script_execute_function (state, display_question_func_obj->data.function, prompt_obj, entry_text_obj, NULL);
    script_obj_unref(prompt_obj);
    script_obj_unref(entry_text_obj);
    script_obj_unref(ret.object);
    }
}

