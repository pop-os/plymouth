#define _GNU_SOURCE
#include "ply-image.h"
#include "ply-utils.h"
#include "script.h"
#include "script-parse.h"
#include "script-object.h"
#include "script-parse.h"
#include "script-execute.h"
#include "script-lib-image.h" 
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#define STRINGIFY_VAR script_lib_image_string
#include "script-lib-image.string"


static void image_free (script_obj* obj)
{
 ply_image_t *image = obj->data.native.object_data;
 ply_image_free (image);
}

static script_return image_new (script_state* state, void* user_data)
{
 script_lib_image_data_t* data = user_data;
 script_obj* reply;
 char* path_filename;
 char* filename = script_obj_hash_get_string (state->local, "filename");
 char* test_string = filename;
 char* prefix_string = "special://";
 while (*test_string && *prefix_string && *test_string == *prefix_string){
    test_string++;
    prefix_string++;
    }
 if (!*prefix_string) {
    if (strcmp(test_string, "logo") == 0)
        path_filename = strdup (PLYMOUTH_LOGO_FILE);
    else
        path_filename = strdup ("");
    }
 else
    asprintf(&path_filename, "%s/%s", data->image_dir, filename);
 
 
 ply_image_t *image = ply_image_new (path_filename);
 if (ply_image_load (image)){
    reply = script_obj_new_native (image, data->class);
    }
 else {
    ply_image_free (image);
    reply = script_obj_new_null ();
    }
 free(filename);
 free(path_filename);
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, reply};
}

static script_return image_get_width (script_state* state, void* user_data)
{
 script_lib_image_data_t* data = user_data;
 ply_image_t *image = script_obj_hash_get_native_of_class (state->local, "image", data->class);
 if (image)
    return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_int (ply_image_get_width (image))};
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_null ()};
}

static script_return image_get_height (script_state* state, void* user_data)
{
 script_lib_image_data_t* data = user_data;
 ply_image_t *image = script_obj_hash_get_native_of_class (state->local, "image", data->class);
 if (image)
    return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_int (ply_image_get_height (image))};
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_null ()};
}

static script_return image_rotate (script_state* state, void* user_data)
{
 script_lib_image_data_t* data = user_data;
 ply_image_t *image = script_obj_hash_get_native_of_class (state->local, "image", data->class);
 float angle = script_obj_hash_get_float (state->local, "angle");
 if (image){
    ply_image_t *new_image = ply_image_rotate (image,
                                               ply_image_get_width (image) / 2,
                                               ply_image_get_height (image) / 2,
                                               angle);
    return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_native (new_image, data->class)};
    }
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_null ()};
}

static script_return image_scale (script_state* state, void* user_data)
{
 script_lib_image_data_t* data = user_data;
 ply_image_t *image = script_obj_hash_get_native_of_class (state->local, "image", data->class);
 int width = script_obj_hash_get_int (state->local, "width");
 int height = script_obj_hash_get_int (state->local, "height");
 if (image){
    ply_image_t *new_image = ply_image_resize (image, width, height);
    return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_native (new_image, data->class)};
    }
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_null ()};
}

script_lib_image_data_t* script_lib_image_setup(script_state *state, char* image_dir)
{
 script_lib_image_data_t* data = malloc(sizeof(script_lib_image_data_t));
 data->class = script_obj_native_class_new(image_free, "image", data);
 data->image_dir = strdup(image_dir);

 script_add_native_function (state->global, "ImageNew", image_new, data, "filename", NULL);
 script_add_native_function (state->global, "ImageRotate", image_rotate, data, "image", "angle", NULL);
 script_add_native_function (state->global, "ImageScale", image_scale, data, "image", "width", "height", NULL);
 script_add_native_function (state->global, "ImageGetWidth", image_get_width, data, "image", NULL);
 script_add_native_function (state->global, "ImageGetHeight", image_get_height, data, "image", NULL);
 
 data->script_main_op = script_parse_string (script_lib_image_string);
 script_return ret = script_execute(state, data->script_main_op);
 script_obj_unref(ret.object);
 
 return data;
}


void script_lib_image_destroy(script_lib_image_data_t* data)
{
 script_obj_native_class_destroy(data->class);
 free(data->image_dir);
 script_parse_op_free (data->script_main_op);
 free(data);
}

 
