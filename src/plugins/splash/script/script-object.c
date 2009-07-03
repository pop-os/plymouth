/* script-object.c - functions to work with script objects
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
#define _GNU_SOURCE
#include "ply-scan.h"
#include "ply-hashtable.h"
#include "ply-list.h"
#include "ply-bitarray.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "script.h"
#include "script-object.h"

char *script_obj_print (script_obj *obj);
void script_obj_reset (script_obj *obj);

void script_obj_free (script_obj *obj)
{
  assert (!obj->refcount);
  script_obj_reset (obj);
  free (obj);
}

void script_obj_ref (script_obj *obj)
{
  obj->refcount++;
}

void script_obj_unref (script_obj *obj)
{
  if (!obj) return;
  assert (obj->refcount > 0);
  obj->refcount--;
  if (obj->refcount <= 0)
    script_obj_free (obj);
}

static void foreach_free_vareable (void *key,
                                   void *data,
                                   void *user_data)
{
  script_vareable *vareable = data;

  script_obj_unref (vareable->object);
  free (vareable->name);
  free (vareable);
}

void script_obj_reset (script_obj *obj)
{
  switch (obj->type)
    {
      case SCRIPT_OBJ_TYPE_REF:
        script_obj_unref (obj->data.obj);
        break;

      case SCRIPT_OBJ_TYPE_INT:
        break;

      case SCRIPT_OBJ_TYPE_FLOAT:
        break;

      case SCRIPT_OBJ_TYPE_STRING:
        free (obj->data.string);
        break;

      case SCRIPT_OBJ_TYPE_HASH:                /* FIXME nightmare */
        ply_hashtable_foreach (obj->data.hash, foreach_free_vareable, NULL);
        ply_hashtable_free (obj->data.hash);
        break;

      case SCRIPT_OBJ_TYPE_FUNCTION:
        {
          if (obj->data.function->freeable)
            {
              ply_list_node_t *node;
              for (node =
                     ply_list_get_first_node (obj->data.function->parameters);
                   node;
                   node =
                     ply_list_get_next_node (obj->data.function->parameters,
                                             node))
                {
                  char *operand = ply_list_node_get_data (node);
                  free (operand);
                }
              ply_list_free (obj->data.function->parameters);
              free (obj->data.function);
            }
        }
        break;

      case SCRIPT_OBJ_TYPE_NATIVE:
        if (obj->data.native.class->free_func)
          obj->data.native.class->free_func (obj);
        break;

      case SCRIPT_OBJ_TYPE_NULL:
        break;
    }
  obj->type = SCRIPT_OBJ_TYPE_NULL;
}

script_obj *script_obj_deref_direct (script_obj *obj)
{
  while (obj->type == SCRIPT_OBJ_TYPE_REF)
    obj = obj->data.obj;
  return obj;
}

void script_obj_deref (script_obj **obj_ptr)
{
  script_obj *obj = *obj_ptr;

  obj = script_obj_deref_direct (obj);
  script_obj_ref (obj);
  script_obj_unref (*obj_ptr);
  *obj_ptr = obj;
}

static void foreach_print_vareable (void *key,
                                    void *data,
                                    void *user_data)
{
  script_vareable *vareable = data;
  char *string = script_obj_print (vareable->object);
  char *reply;
  char *prev = *(char **) user_data;

  if (!prev) prev = strdup ("");
  asprintf (&reply, "%s%s = %s\n", prev, vareable->name, string);
  free (string);
  free (prev);
  *(char **) user_data = reply;
}

char *script_obj_print (script_obj *obj)
{
  char *reply;

  switch (obj->type)
    {
      case SCRIPT_OBJ_TYPE_REF:
        {
          char *subobj = script_obj_print (obj->data.obj);
          asprintf (&reply, "->[%d]%s", obj->refcount, subobj);
          free (subobj);
          return reply;
        }

      case SCRIPT_OBJ_TYPE_INT:
        {
          asprintf (&reply, "%d[%d]", obj->data.integer, obj->refcount);
          return reply;
        }

      case SCRIPT_OBJ_TYPE_FLOAT:
        {
          asprintf (&reply, "%f[%d]", obj->data.floatpoint, obj->refcount);
          return reply;
        }

      case SCRIPT_OBJ_TYPE_STRING:
        {
          asprintf (&reply, "\"%s\"[%d]", obj->data.string, obj->refcount);
          return reply;
        }

      case SCRIPT_OBJ_TYPE_NULL:
        {
          asprintf (&reply, "NULL[%d]", obj->refcount);
          return reply;
        }

      case SCRIPT_OBJ_TYPE_HASH:
        {
          char *sub = NULL;
          ply_hashtable_foreach (obj->data.hash, foreach_print_vareable, &sub);
          asprintf (&reply, "HASH{\n%s}[%d]", sub, obj->refcount);
          free (sub);
          return reply;
        }

      case SCRIPT_OBJ_TYPE_FUNCTION:
        {
          asprintf (&reply, "function()[%d]", obj->refcount);
          return reply;
        }

      case SCRIPT_OBJ_TYPE_NATIVE:
        {
          asprintf (&reply,
                    "(%s)[%d]",
                    obj->data.native.class->name,
                    obj->refcount);
          return reply;
        }

      default:
        printf ("unhandeled object type %d\n", obj->type);
        assert (0);
    }
  return NULL;
}

script_obj *script_obj_new_null (void)
{
  script_obj *obj = malloc (sizeof (script_obj));

  obj->type = SCRIPT_OBJ_TYPE_NULL;
  obj->refcount = 1;
  return obj;
}

script_obj *script_obj_new_int (int number)
{
  script_obj *obj = malloc (sizeof (script_obj));

  obj->type = SCRIPT_OBJ_TYPE_INT;
  obj->refcount = 1;
  obj->data.integer = number;
  return obj;
}

script_obj *script_obj_new_float (float number)
{
  if (isnan (number)) return script_obj_new_null ();
  script_obj *obj = malloc (sizeof (script_obj));
  obj->type = SCRIPT_OBJ_TYPE_FLOAT;
  obj->refcount = 1;
  obj->data.floatpoint = number;
  return obj;
}

script_obj *script_obj_new_string (const char *string)
{
  if (!string) return script_obj_new_null ();
  script_obj *obj = malloc (sizeof (script_obj));
  obj->type = SCRIPT_OBJ_TYPE_STRING;
  obj->refcount = 1;
  obj->data.string = strdup (string);
  return obj;
}

script_obj *script_obj_new_hash (void)
{
  script_obj *obj = malloc (sizeof (script_obj));

  obj->type = SCRIPT_OBJ_TYPE_HASH;
  obj->data.hash = ply_hashtable_new (ply_hashtable_string_hash,
                                      ply_hashtable_string_compare);
  obj->refcount = 1;
  return obj;
}

script_obj *script_obj_new_function (script_function *function)
{
  script_obj *obj = malloc (sizeof (script_obj));

  obj->type = SCRIPT_OBJ_TYPE_FUNCTION;
  obj->data.function = function;
  obj->refcount = 1;
  return obj;
}

script_obj *script_obj_new_ref (script_obj *sub_obj)
{
  script_obj *obj = malloc (sizeof (script_obj));

  obj->type = SCRIPT_OBJ_TYPE_REF;
  obj->data.obj = sub_obj;
  obj->refcount = 1;
  return obj;
}

script_obj *script_obj_new_native (void                    *object_data,
                                   script_obj_native_class *class)
{
  if (!object_data) return script_obj_new_null ();
  script_obj *obj = malloc (sizeof (script_obj));
  obj->type = SCRIPT_OBJ_TYPE_NATIVE;
  obj->data.native.class = class;
  obj->data.native.object_data = object_data;
  obj->refcount = 1;
  return obj;
}

int script_obj_as_int (script_obj *obj)
{                                                     /* If in then reply contents, otherwise reply 0 */
  obj = script_obj_deref_direct (obj);
  switch (obj->type)
    {
      case SCRIPT_OBJ_TYPE_INT:
        return obj->data.integer;

      case SCRIPT_OBJ_TYPE_FLOAT:
        return (int) obj->data.floatpoint;

      case SCRIPT_OBJ_TYPE_NULL:
        return 0;

      case SCRIPT_OBJ_TYPE_REF:     /* should have been de-reffed already */
        assert (0);

      case SCRIPT_OBJ_TYPE_HASH:
      case SCRIPT_OBJ_TYPE_FUNCTION:
      case SCRIPT_OBJ_TYPE_NATIVE:
        return 0;

      case SCRIPT_OBJ_TYPE_STRING:
        return 0;
    }

  assert (0);       /* Abort on uncaught */
  return 0;
}

float script_obj_as_float (script_obj *obj)
{                                                     /* If in then reply contents, otherwise reply 0 */
  obj = script_obj_deref_direct (obj);
  switch (obj->type)
    {
      case SCRIPT_OBJ_TYPE_INT:
        return (float) obj->data.integer;

      case SCRIPT_OBJ_TYPE_FLOAT:
        return obj->data.floatpoint;

      case SCRIPT_OBJ_TYPE_NULL:
        return NAN;

      case SCRIPT_OBJ_TYPE_REF:     /* should have been de-reffed already */
        assert (0);

      case SCRIPT_OBJ_TYPE_HASH:
      case SCRIPT_OBJ_TYPE_FUNCTION:
      case SCRIPT_OBJ_TYPE_NATIVE:
        return NAN;

      case SCRIPT_OBJ_TYPE_STRING:
        return NAN;
    }

  assert (0);       /* Abort on uncaught */
  return NAN;
}

bool script_obj_as_bool (script_obj *obj)
{                                                 /* False objects are NULL, 0, "" */
  obj = script_obj_deref_direct (obj);
  switch (obj->type)
    {
      case SCRIPT_OBJ_TYPE_INT:
        if (obj->data.integer) return true;
        return false;

      case SCRIPT_OBJ_TYPE_FLOAT:
        if (obj->data.floatpoint) return true;
        return false;

      case SCRIPT_OBJ_TYPE_NULL:
        return false;

      case SCRIPT_OBJ_TYPE_REF:     /* should have been de-reffed already */
        assert (0);

      case SCRIPT_OBJ_TYPE_HASH:
      case SCRIPT_OBJ_TYPE_FUNCTION:
      case SCRIPT_OBJ_TYPE_NATIVE:
        return true;

      case SCRIPT_OBJ_TYPE_STRING:
        if (*obj->data.string) return true;
        return false;
    }

  assert (0);       /* Abort on uncaught */
  return false;
}

char *script_obj_as_string (script_obj *obj)              /* reply is strdupped and may be NULL */
{
  obj = script_obj_deref_direct (obj);
  char *reply;

  switch (obj->type)
    {
      case SCRIPT_OBJ_TYPE_INT:
        asprintf (&reply, "%d", obj->data.integer);
        return reply;

      case SCRIPT_OBJ_TYPE_FLOAT:
        asprintf (&reply, "%f", obj->data.floatpoint);
        return reply;

      case SCRIPT_OBJ_TYPE_NULL:
        return NULL;

      case SCRIPT_OBJ_TYPE_REF:     /* should have been de-reffed already */
        assert (0);

      case SCRIPT_OBJ_TYPE_HASH:
        return NULL;

      case SCRIPT_OBJ_TYPE_FUNCTION:
        return NULL;

      case SCRIPT_OBJ_TYPE_STRING:
        return strdup (obj->data.string);

      case SCRIPT_OBJ_TYPE_NATIVE:
        return NULL;
    }

  assert (0);       /* Abort on uncaught */
  return false;
}

void *script_obj_as_native_of_class (script_obj              *obj,
                                     script_obj_native_class *class)
{
  obj = script_obj_deref_direct (obj);
  if (script_obj_is_native_of_class (obj, class))
    return obj->data.native.object_data;
  return NULL;
}

void *script_obj_as_native_of_class_name (script_obj *obj,
                                          const char *class_name)
{
  obj = script_obj_deref_direct (obj);
  if (script_obj_is_native_of_class_name (obj, class_name))
    return obj->data.native.object_data;
  return NULL;
}

bool script_obj_is_null (script_obj *obj)
{
  obj = script_obj_deref_direct (obj);
  return obj->type == SCRIPT_OBJ_TYPE_NULL;
}

bool script_obj_is_int (script_obj *obj)
{
  obj = script_obj_deref_direct (obj);
  return obj->type == SCRIPT_OBJ_TYPE_INT;
}

bool script_obj_is_float (script_obj *obj)
{
  obj = script_obj_deref_direct (obj);
  return obj->type == SCRIPT_OBJ_TYPE_FLOAT;
}

bool script_obj_is_number (script_obj *obj)     /* Float or Int */
{
  obj = script_obj_deref_direct (obj);
  return obj->type == SCRIPT_OBJ_TYPE_INT || obj->type == SCRIPT_OBJ_TYPE_FLOAT;
}

bool script_obj_is_string (script_obj *obj)
{
  obj = script_obj_deref_direct (obj);
  return obj->type == SCRIPT_OBJ_TYPE_STRING;
}

bool script_obj_is_hash (script_obj *obj)
{
  obj = script_obj_deref_direct (obj);
  return obj->type == SCRIPT_OBJ_TYPE_HASH;
}

bool script_obj_is_function (script_obj *obj)
{
  obj = script_obj_deref_direct (obj);
  return obj->type == SCRIPT_OBJ_TYPE_FUNCTION;
}

bool script_obj_is_native (script_obj *obj)
{
  obj = script_obj_deref_direct (obj);
  return obj->type == SCRIPT_OBJ_TYPE_NATIVE;
}

bool script_obj_is_native_of_class (script_obj              *obj,
                                    script_obj_native_class *class)
{
  obj = script_obj_deref_direct (obj);
  return obj->type == SCRIPT_OBJ_TYPE_NATIVE && obj->data.native.class == class;
}

bool script_obj_is_native_of_class_name (script_obj *obj,
                                         const char *class_name)
{
  obj = script_obj_deref_direct (obj);
  return obj->type == SCRIPT_OBJ_TYPE_NATIVE && !strcmp (
           obj->data.native.class->name,
           class_name);
}

void script_obj_assign (script_obj *obj_a,
                        script_obj *obj_b)
{
  obj_b = script_obj_deref_direct (obj_b);
  if (obj_a == obj_b) return;                   /* FIXME triple check this */
  script_obj_reset (obj_a);

  switch (obj_b->type)
    {
      case SCRIPT_OBJ_TYPE_NULL:
        obj_a->type = SCRIPT_OBJ_TYPE_NULL;
        break;

      case SCRIPT_OBJ_TYPE_INT:
        obj_a->type = SCRIPT_OBJ_TYPE_INT;
        obj_a->data.integer = obj_b->data.integer;
        break;

      case SCRIPT_OBJ_TYPE_FLOAT:
        obj_a->type = SCRIPT_OBJ_TYPE_FLOAT;
        obj_a->data.floatpoint = obj_b->data.floatpoint;
        break;

      case SCRIPT_OBJ_TYPE_STRING:
        obj_a->type = SCRIPT_OBJ_TYPE_STRING;
        obj_a->data.string = strdup (obj_b->data.string);
        break;

      case SCRIPT_OBJ_TYPE_REF:     /* should have been de-reffed already */
        assert (0);

      case SCRIPT_OBJ_TYPE_HASH:
      case SCRIPT_OBJ_TYPE_FUNCTION:
      case SCRIPT_OBJ_TYPE_NATIVE:
        obj_a->type = SCRIPT_OBJ_TYPE_REF;
        obj_a->data.obj = obj_b;
        script_obj_ref (obj_b);
        break;
    }
}

script_obj *script_obj_hash_get_element (script_obj *hash,
                                         const char *name)
{
  assert (hash->type == SCRIPT_OBJ_TYPE_HASH);
  script_vareable *vareable = ply_hashtable_lookup (hash->data.hash,
                                                    (void *) name);
  script_obj *obj;

  if (vareable)
    obj = vareable->object;
  else
    {
      obj = script_obj_new_null ();
      vareable = malloc (sizeof (script_vareable));
      vareable->name = strdup (name);
      vareable->object = obj;
      ply_hashtable_insert (hash->data.hash, vareable->name, vareable);
    }
  script_obj_ref (obj);
  return obj;
}

int script_obj_hash_get_int (script_obj *hash,
                             const char *name)
{
  script_obj *obj = script_obj_hash_get_element (hash, name);
  int reply = script_obj_as_int (obj);

  script_obj_unref (obj);
  return reply;
}

float script_obj_hash_get_float (script_obj *hash,
                                 const char *name)
{
  script_obj *obj = script_obj_hash_get_element (hash, name);
  float reply = script_obj_as_float (obj);

  script_obj_unref (obj);
  return reply;
}

bool script_obj_hash_get_bool (script_obj *hash,
                               const char *name)
{
  script_obj *obj = script_obj_hash_get_element (hash, name);
  bool reply = script_obj_as_bool (obj);

  script_obj_unref (obj);
  return reply;
}

char *script_obj_hash_get_string (script_obj *hash,
                                  const char *name)
{
  script_obj *obj = script_obj_hash_get_element (hash, name);
  char *reply = script_obj_as_string (obj);

  script_obj_unref (obj);
  return reply;
}

void *script_obj_hash_get_native_of_class (script_obj              *hash,
                                           const char              *name,
                                           script_obj_native_class *class)
{
  script_obj *obj = script_obj_hash_get_element (hash, name);
  void *reply = script_obj_as_native_of_class (obj, class);

  script_obj_unref (obj);
  return reply;
}

void *script_obj_hash_get_native_of_class_name (script_obj *hash,
                                                const char *name,
                                                const char *class_name)
{
  script_obj *obj = script_obj_hash_get_element (hash, name);
  void *reply = script_obj_as_native_of_class_name (obj, class_name);

  script_obj_unref (obj);
  return reply;
}

void script_obj_hash_add_element (script_obj *hash,
                                  script_obj *element,
                                  const char *name)
{
  assert (hash->type == SCRIPT_OBJ_TYPE_HASH);
  script_obj *obj = script_obj_hash_get_element (hash, name);
  script_obj_assign (obj, element);
  script_obj_unref (obj);
}

script_obj *script_obj_plus (script_obj *script_obj_a,
                             script_obj *script_obj_b)
{
  if (script_obj_is_string (script_obj_a) || script_obj_is_string (script_obj_b))
    {
      script_obj *obj;
      char *string_a = script_obj_as_string (script_obj_a);
      char *string_b = script_obj_as_string (script_obj_b);
      if (string_a && string_b)
        {
          char *newstring;
          asprintf (&newstring, "%s%s", string_a, string_b);
          obj = script_obj_new_string (newstring);
          free (newstring);
        }
      else
        obj = script_obj_new_null ();
      free (string_a);
      free (string_b);
      return obj;
    }
  if (script_obj_is_number (script_obj_a) && script_obj_is_number (script_obj_b))
    {
      if (script_obj_is_int (script_obj_a) && script_obj_is_int (script_obj_b))
        {
          int value = script_obj_as_int (script_obj_a) + script_obj_as_int (script_obj_b);
          return script_obj_new_int (value);
        }
      float value = script_obj_as_float (script_obj_a) + script_obj_as_float (script_obj_b);
      return script_obj_new_float (value);
    }
  return script_obj_new_null ();
}

script_obj *script_obj_minus (script_obj *script_obj_a,
                              script_obj *script_obj_b)
{
  if (script_obj_is_number (script_obj_a) && script_obj_is_number (script_obj_b))
    {
      if (script_obj_is_int (script_obj_a) && script_obj_is_int (script_obj_b))
        {
          int value = script_obj_as_int (script_obj_a) - script_obj_as_int (script_obj_b);
          return script_obj_new_int (value);
        }
      float value = script_obj_as_float (script_obj_a) - script_obj_as_float (script_obj_b);
      return script_obj_new_float (value);
    }
  return script_obj_new_null ();
}

script_obj *script_obj_mul (script_obj *script_obj_a,
                            script_obj *script_obj_b)
{
  if (script_obj_is_number (script_obj_a) && script_obj_is_number (script_obj_b))
    {
      if (script_obj_is_int (script_obj_a) && script_obj_is_int (script_obj_b))
        {
          int value = script_obj_as_int (script_obj_a) * script_obj_as_int (script_obj_b);
          return script_obj_new_int (value);
        }
      float value = script_obj_as_float (script_obj_a) * script_obj_as_float (script_obj_b);
      return script_obj_new_float (value);
    }
  return script_obj_new_null ();
}

script_obj *script_obj_div (script_obj *script_obj_a,
                            script_obj *script_obj_b)
{
  if (script_obj_is_number (script_obj_a) && script_obj_is_number (script_obj_b))
    {
      if (script_obj_is_int (script_obj_a) && script_obj_is_int (script_obj_b))
        if (script_obj_as_int (script_obj_a) %
            script_obj_as_int (script_obj_b) == 0)
          {
            int value = script_obj_as_int (script_obj_a) / script_obj_as_int (script_obj_b);
            return script_obj_new_int (value);
          }
      float value = script_obj_as_float (script_obj_a) / script_obj_as_float (script_obj_b);
      return script_obj_new_float (value);
    }
  return script_obj_new_null ();
}

script_obj *script_obj_mod (script_obj *script_obj_a,
                            script_obj *script_obj_b)
{
  if (script_obj_is_number (script_obj_a) && script_obj_is_number (script_obj_b))
    {
      if (script_obj_is_int (script_obj_a) && script_obj_is_int (script_obj_b))
        {
          int value = script_obj_as_int (script_obj_a) % script_obj_as_int (script_obj_b);
          return script_obj_new_int (value);
        }
      float value = fmodf (script_obj_as_float (
                             script_obj_a), script_obj_as_float (script_obj_b));
      return script_obj_new_float (value);
    }
  return script_obj_new_null ();
}

