/* script-object.h - functions to work with script objects
 *
 * Copyright (C) 2009 Charlie Brej <cbrej@cs.man.ac.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by: Charlie Brej <cbrej@cs.man.ac.uk>
 */
#ifndef SCRIPT_OBJECT
#define SCRIPT_OBJECT

#include "script.h"

void script_obj_free (script_obj *obj);
void script_obj_ref (script_obj *obj);
void script_obj_unref (script_obj *obj);
void script_obj_reset (script_obj *obj);
script_obj *script_obj_deref_direct (script_obj *obj);
void script_obj_deref (script_obj **obj_ptr);
char *script_obj_print (script_obj *obj);
script_obj *script_obj_new_int (int number);
script_obj *script_obj_new_float (float number);
script_obj *script_obj_new_string (const char *string);
script_obj *script_obj_new_null (void);
script_obj *script_obj_new_hash (void);
script_obj *script_obj_new_function (script_function *function);
script_obj *script_obj_new_ref (script_obj *sub_obj);

script_obj *script_obj_new_native (void *object_data,
                                   script_obj_native_class * class );
int script_obj_as_int (script_obj *obj);
float script_obj_as_float (script_obj *obj);
bool script_obj_as_bool (script_obj *obj);
char *script_obj_as_string (script_obj *obj);

void *script_obj_as_native_of_class (script_obj * obj,
                                     script_obj_native_class * class );
void *script_obj_as_native_of_class_name (script_obj *obj,
                                          const char *class_name);
bool script_obj_is_null (script_obj *obj);
bool script_obj_is_int (script_obj *obj);
bool script_obj_is_float (script_obj *obj);
bool script_obj_is_string (script_obj *obj);
bool script_obj_is_hash (script_obj *obj);
bool script_obj_is_function (script_obj *obj);
bool script_obj_is_native (script_obj *obj);

bool script_obj_is_native_of_class (script_obj * obj,
                                    script_obj_native_class * class );
bool script_obj_is_native_of_class_name (script_obj *obj,
                                         const char *class_name);
void script_obj_assign (script_obj *obj_a,
                        script_obj *obj_b);
script_obj *script_obj_hash_get_element (script_obj *hash,
                                         const char *name);
int script_obj_hash_get_int (script_obj *hash,
                             const char *name);
float script_obj_hash_get_float (script_obj *hash,
                                 const char *name);
bool script_obj_hash_get_bool (script_obj *hash,
                               const char *name);
char *script_obj_hash_get_string (script_obj *hash,
                                  const char *name);

void *script_obj_hash_get_native_of_class (script_obj * hash,
                                           const char *name,
                                           script_obj_native_class * class );
void *script_obj_hash_get_native_of_class_name (script_obj *hash,
                                                const char *name,
                                                const char *class_name);
void script_obj_hash_add_element (script_obj *hash,
                                  script_obj *element,
                                  const char *name);
script_obj *script_obj_plus (script_obj *script_obj_a_in,
                             script_obj *script_obj_b_in);
script_obj *script_obj_minus (script_obj *script_obj_a_in,
                              script_obj *script_obj_b_in);
script_obj *script_obj_mul (script_obj *script_obj_a_in,
                            script_obj *script_obj_b_in);
script_obj *script_obj_div (script_obj *script_obj_a_in,
                            script_obj *script_obj_b_in);
script_obj *script_obj_mod (script_obj *script_obj_a_in,
                            script_obj *script_obj_b_in);

#endif /* SCRIPT_OBJECT */
