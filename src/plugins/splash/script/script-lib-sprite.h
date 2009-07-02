#ifndef SCRIPT_LIB_SPRITE
#define SCRIPT_LIB_SPRITE

#include "script.h"

typedef struct
{
  ply_window_t   *window;
  ply_list_t     *sprite_list;
  script_obj_native_class* class;
  script_op      *script_main_op;
  uint32_t        background_color_start;
  uint32_t        background_color_end;
  bool            full_refresh;
} script_lib_sprite_data_t;


typedef struct
{
  int x; 
  int y;
  int z;
  float opacity;
  int old_x; 
  int old_y;
  int old_z;
  int old_width;
  int old_height;
  float old_opacity;
  bool refresh_me;
  bool remove_me;
  ply_image_t *image;
  script_obj *image_obj;
} sprite_t;


script_lib_sprite_data_t* script_lib_sprite_setup(script_state *state, ply_window_t *window);
void script_lib_sprite_refresh(script_lib_sprite_data_t* data);
void script_lib_sprite_destroy(script_lib_sprite_data_t* data);

#endif /* SCRIPT_LIB_SPRITE */
