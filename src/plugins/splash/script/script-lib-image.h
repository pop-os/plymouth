#ifndef SCRIPT_LIB_IMAGE
#define SCRIPT_LIB_IMAGE

#include "script.h"

typedef struct
{
  script_obj_native_class* class;
  script_op      *script_main_op;
  char* image_dir;
} script_lib_image_data_t;


script_lib_image_data_t* script_lib_image_setup(script_state *state, char* image_dir);
void script_lib_image_destroy(script_lib_image_data_t* data);

#endif /* SCRIPT_LIB_IMAGE */
