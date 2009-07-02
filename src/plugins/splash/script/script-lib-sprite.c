#include "ply-image.h"
#include "ply-utils.h"
#include "ply-window.h"
#include "ply-frame-buffer.h"
#include "ply-logger.h"
#include "ply-key-file.h"
#include "script.h"
#include "script-parse.h"
#include "script-execute.h"
#include "script-object.h"
#include "script-lib-image.h"
#include "script-lib-sprite.h"
#include <assert.h>
#include <stdlib.h>
#include <math.h>


#define STRINGIFY_VAR script_lib_sprite_string
#include "script-lib-sprite.string"



static void draw_area (script_lib_sprite_data_t* data, int x, int y, int width, int height);


static void sprite_free (script_obj* obj)
{
 sprite_t *sprite = obj->data.native.object_data;
 sprite->remove_me = true;
}

static script_return sprite_new (script_state* state, void* user_data)
{
 script_lib_sprite_data_t* data = user_data;
 script_obj* reply;
 
 sprite_t *sprite = calloc(1, sizeof(sprite_t));
 sprite->x = 0;
 sprite->y = 0;
 sprite->z = 0;
 sprite->opacity = 1.0;
 sprite->old_x = 0;
 sprite->old_y = 0;
 sprite->old_z = 0;
 sprite->old_opacity = 1.0;
 sprite->refresh_me = false;
 sprite->remove_me = false;
 sprite->image = NULL;
 sprite->image_obj = NULL;
 ply_list_append_data (data->sprite_list, sprite);
 
 reply = script_obj_new_native (sprite, data->class);
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, reply};
}


static script_return sprite_set_image (script_state* state, void* user_data)
{
 script_lib_sprite_data_t* data = user_data;
 script_obj* script_obj_sprite = script_obj_hash_get_element (state->local, "sprite");
 script_obj_deref(&script_obj_sprite);
 script_obj* script_obj_image = script_obj_hash_get_element (state->local, "image");
 script_obj_deref(&script_obj_image);
 
 if (script_obj_image->type == SCRIPT_OBJ_TYPE_NATIVE &&
     script_obj_sprite->type == SCRIPT_OBJ_TYPE_NATIVE &&
     script_obj_sprite->data.native.class == data->class){
    sprite_t *sprite = script_obj_sprite->data.native.object_data;
    ply_image_t *image = script_obj_image->data.native.object_data;
    script_obj_unref(sprite->image_obj);
    script_obj_ref(script_obj_image);
    sprite->image = image;
    sprite->image_obj = script_obj_image;
    sprite->refresh_me = true;
    }
 script_obj_unref(script_obj_sprite);
 script_obj_unref(script_obj_image);
 
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_null ()};
}

static script_return sprite_set_x (script_state* state, void* user_data)
{
 script_lib_sprite_data_t* data = user_data;
 script_obj* script_obj_sprite = script_obj_hash_get_element (state->local, "sprite");
 script_obj_deref(&script_obj_sprite);
 int value = script_obj_hash_get_int (state->local, "value");
 
 if (script_obj_sprite->type == SCRIPT_OBJ_TYPE_NATIVE &&
     script_obj_sprite->data.native.class == data->class){
    sprite_t *sprite = script_obj_sprite->data.native.object_data;
    sprite->x = value;
    }
 script_obj_unref(script_obj_sprite);
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_null ()};
}

static script_return sprite_set_y (script_state* state, void* user_data)
{
 script_lib_sprite_data_t* data = user_data;
 script_obj* script_obj_sprite = script_obj_hash_get_element (state->local, "sprite");
 script_obj_deref(&script_obj_sprite);
 int value = script_obj_hash_get_int (state->local, "value");
 
 if (script_obj_sprite->type == SCRIPT_OBJ_TYPE_NATIVE &&
     script_obj_sprite->data.native.class == data->class){
    sprite_t *sprite = script_obj_sprite->data.native.object_data;
    sprite->y = value;
    }
 script_obj_unref(script_obj_sprite);
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_null ()};
}

static script_return sprite_set_z (script_state* state, void* user_data)
{
 script_lib_sprite_data_t* data = user_data;
 script_obj* script_obj_sprite = script_obj_hash_get_element (state->local, "sprite");
 script_obj_deref(&script_obj_sprite);
 int value = script_obj_hash_get_int (state->local, "value");
 
 if (script_obj_sprite->type == SCRIPT_OBJ_TYPE_NATIVE &&
     script_obj_sprite->data.native.class == data->class){
    sprite_t *sprite = script_obj_sprite->data.native.object_data;
    sprite->z = value;
    }
 script_obj_unref(script_obj_sprite);
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_null ()};
}

static script_return sprite_set_opacity (script_state* state, void* user_data)
{
 script_lib_sprite_data_t* data = user_data;
 script_obj* script_obj_sprite = script_obj_hash_get_element (state->local, "sprite");
 script_obj_deref(&script_obj_sprite);
 float value = script_obj_hash_get_float (state->local, "value");
 
 if (script_obj_sprite->type == SCRIPT_OBJ_TYPE_NATIVE &&
     script_obj_sprite->data.native.class == data->class){
    sprite_t *sprite = script_obj_sprite->data.native.object_data;
    sprite->opacity = value;
    }
 script_obj_unref(script_obj_sprite);
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_null ()};
}

static script_return sprite_window_get_width (script_state* state, void* user_data)
{
 script_lib_sprite_data_t* data = user_data;
 ply_frame_buffer_t *frame_buffer = ply_window_get_frame_buffer (data->window);
 ply_frame_buffer_area_t area;
 ply_frame_buffer_get_size (frame_buffer, &area);
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_int (area.width)};
}

static script_return sprite_window_get_height (script_state* state, void* user_data)
{
 script_lib_sprite_data_t* data = user_data;
 ply_frame_buffer_t *frame_buffer = ply_window_get_frame_buffer (data->window);
 ply_frame_buffer_area_t area;
 ply_frame_buffer_get_size (frame_buffer, &area);
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_int (area.height)};
}

static uint32_t extract_rgb_color (script_state* state)
{
 script_obj* script_obj_red = script_obj_hash_get_element (state->local, "red");
 script_obj* script_obj_green = script_obj_hash_get_element (state->local, "green");
 script_obj* script_obj_blue = script_obj_hash_get_element (state->local, "blue");
 uint8_t red =   CLAMP(255 * script_obj_as_float(script_obj_red), 0, 255);
 uint8_t green = CLAMP(255 * script_obj_as_float(script_obj_green), 0, 255);
 uint8_t blue =  CLAMP(255 * script_obj_as_float(script_obj_blue), 0, 255);
 script_obj_unref(script_obj_red);
 script_obj_unref(script_obj_green);
 script_obj_unref(script_obj_blue);
 return (uint32_t) red<<16 | green<<8 | blue;
}
static script_return sprite_window_set_background_top_color (script_state* state, void* user_data)
{
 script_lib_sprite_data_t* data = user_data;
 data->background_color_start = extract_rgb_color (state);
 data->full_refresh = true;
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_null ()};
}

static script_return sprite_window_set_background_bottom_color (script_state* state, void* user_data)
{
 script_lib_sprite_data_t* data = user_data;
 data->background_color_end = extract_rgb_color (state);
 data->full_refresh = true;
 return (script_return){SCRIPT_RETURN_TYPE_RETURN, script_obj_new_null ()};
}




static void
draw_area (script_lib_sprite_data_t*            data,
           int                       x,
           int                       y,
           int                       width,
           int                       height)
{
  ply_frame_buffer_area_t clip_area;
  clip_area.x = x;
  clip_area.y = y;
  clip_area.width = width;
  clip_area.height = height;
  ply_frame_buffer_t *frame_buffer = ply_window_get_frame_buffer (data->window);
  
  ply_frame_buffer_pause_updates (frame_buffer);


  if (data->background_color_start == data->background_color_end) 
    ply_frame_buffer_fill_with_hex_color (frame_buffer, &clip_area, data->background_color_start);
  else
    ply_frame_buffer_fill_with_gradient (frame_buffer, &clip_area, data->background_color_start, data->background_color_end);
  ply_list_node_t *node;
  for (node = ply_list_get_first_node (data->sprite_list); node; node = ply_list_get_next_node (data->sprite_list, node))
    {
      sprite_t* sprite = ply_list_node_get_data (node);
      ply_frame_buffer_area_t sprite_area;
      if (!sprite->image) continue;

      if (sprite->remove_me) continue;
      if (sprite->opacity<0.011) continue;

      sprite_area.x = sprite->x;
      sprite_area.y = sprite->y;

      if (sprite_area.x>=(x+width)) continue;
      if (sprite_area.y>=(y+height)) continue;

      sprite_area.width =  ply_image_get_width (sprite->image);
      sprite_area.height = ply_image_get_height (sprite->image);

      if ((sprite_area.x+(int)sprite_area.width) <= x) continue; 
      if ((sprite_area.y+(int)sprite_area.height) <= y) continue;

      ply_frame_buffer_fill_with_argb32_data_at_opacity_with_clip (frame_buffer,
                                             &sprite_area, &clip_area, 0, 0,
                                             ply_image_get_data (sprite->image), sprite->opacity);
    }
  ply_frame_buffer_unpause_updates (frame_buffer);
}


script_lib_sprite_data_t* script_lib_sprite_setup(script_state *state, ply_window_t   *window)
{
 script_lib_sprite_data_t* data = malloc(sizeof(script_lib_sprite_data_t));
 data->class = script_obj_native_class_new(sprite_free, "sprite", data);
 data->sprite_list = ply_list_new();
 data->window = window;

 script_add_native_function (state->global, "SpriteNew", sprite_new, data, NULL);
 script_add_native_function (state->global, "SpriteSetImage", sprite_set_image, data, "sprite", "image", NULL);
 script_add_native_function (state->global, "SpriteSetX", sprite_set_x, data, "sprite", "value", NULL);
 script_add_native_function (state->global, "SpriteSetY", sprite_set_y, data, "sprite", "value", NULL);
 script_add_native_function (state->global, "SpriteSetZ", sprite_set_z, data, "sprite", "value", NULL);
 script_add_native_function (state->global, "SpriteSetOpacity", sprite_set_opacity, data, "sprite", "value", NULL);
 script_add_native_function (state->global, "SpriteWindowGetWidth", sprite_window_get_width, data, NULL);
 script_add_native_function (state->global, "SpriteWindowGetHeight", sprite_window_get_height, data, NULL);
 script_add_native_function (state->global, "SpriteWindowSetBackgroundTopColor", sprite_window_set_background_top_color, data, "red", "green", "blue", NULL);
 script_add_native_function (state->global, "SpriteWindowSetBackgroundBottomColor", sprite_window_set_background_bottom_color, data, "red", "green", "blue", NULL);

 data->script_main_op = script_parse_string (script_lib_sprite_string);
 data->background_color_start = 0x000000;
 data->background_color_end   = 0x000000;
 data->full_refresh = true;
 script_return ret = script_execute(state, data->script_main_op);
 script_obj_unref(ret.object);
 return data;
}


void script_lib_sprite_refresh(script_lib_sprite_data_t* data)
{
 ply_list_node_t *node;
 
 node = ply_list_get_first_node (data->sprite_list); 
 
 if (data->full_refresh){
    ply_frame_buffer_area_t screen_area;
    ply_frame_buffer_t *frame_buffer = ply_window_get_frame_buffer (data->window);
    ply_frame_buffer_get_size (frame_buffer, &screen_area);
    draw_area (data, screen_area.x, screen_area.y, screen_area.width, screen_area.height);
    data->full_refresh = false;
    }

 while (node){
    sprite_t* sprite = ply_list_node_get_data (node);
    ply_list_node_t *next_node = ply_list_get_next_node (data->sprite_list, node);
    if (sprite->remove_me) {
        if (sprite->image) {
            int width = ply_image_get_width (sprite->image);
            int height= ply_image_get_height (sprite->image);
            draw_area (data, sprite->old_x, sprite->old_y, width, height);
            }
        ply_list_remove_node (data->sprite_list, node);
        script_obj_unref(sprite->image_obj);
        free(sprite);
        }
    node = next_node;
    }
 
 for(node = ply_list_get_first_node (data->sprite_list); node; node = ply_list_get_next_node (data->sprite_list, node)){
    sprite_t* sprite = ply_list_node_get_data (node);
    if (!sprite->image) continue;
    if (    sprite->x != sprite->old_x || 
            sprite->y != sprite->old_y || 
            sprite->z != sprite->old_z || 
            fabs(sprite->old_opacity - sprite->opacity) > 0.01 ||       // People can't see the difference between 
            sprite->refresh_me ){
        int width = ply_image_get_width (sprite->image);
        int height= ply_image_get_height (sprite->image);
        draw_area (data, sprite->x, sprite->y, width, height);
        draw_area (data, sprite->old_x, sprite->old_y, sprite->old_width, sprite->old_height);
        sprite->old_x = sprite->x;
        sprite->old_y = sprite->y;
        sprite->old_z = sprite->z;
        sprite->old_width = width;
        sprite->old_height = height;        
        sprite->old_opacity = sprite->opacity;
        sprite->refresh_me = false;
        }
    }
}




void script_lib_sprite_destroy(script_lib_sprite_data_t* data)
{
 script_obj_native_class_destroy(data->class);
 ply_list_free(data->sprite_list);
 script_parse_op_free (data->script_main_op);
 free(data);
}



