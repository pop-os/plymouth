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

#include "script.h"
#include "script-object.h"
 

char* script_obj_print (script_obj* obj);
void script_obj_reset (script_obj* obj);


void script_obj_free (script_obj* obj)
{
 assert(!obj->refcount);
 script_obj_reset (obj);
 free(obj);
}


void script_obj_ref (script_obj* obj)
{
 obj->refcount++;
}

void script_obj_unref (script_obj* obj)
{
 if (!obj) return;
 assert(obj->refcount>0);
 obj->refcount--;
 if (obj->refcount<=0){
    script_obj_free (obj);
    }
}

static void foreach_free_vareable (void *key,
                                   void *data,
                                   void *user_data)
{
 script_vareable* vareable = data;
 script_obj_unref(vareable->object);
 free(vareable->name);
 free(vareable);
}



void script_obj_reset (script_obj* obj)
{
 switch (obj->type){
    case SCRIPT_OBJ_TYPE_REF:
        script_obj_unref (obj->data.obj);
        break;
    case SCRIPT_OBJ_TYPE_INT:
        break;
    case SCRIPT_OBJ_TYPE_FLOAT:
        break;
    case SCRIPT_OBJ_TYPE_STRING:
        free(obj->data.string);
        break;
    case SCRIPT_OBJ_TYPE_HASH:                  // FIXME nightmare
        ply_hashtable_foreach (obj->data.hash, foreach_free_vareable, NULL);
        ply_hashtable_free(obj->data.hash);
        break;
    case SCRIPT_OBJ_TYPE_FUNCTION:
        {
        if (obj->data.function->freeable){
            ply_list_node_t *node;
            for (node = ply_list_get_first_node (obj->data.function->parameters);
                 node;
                 node = ply_list_get_next_node (obj->data.function->parameters, node)){
                char* operand = ply_list_node_get_data (node);
                free(operand);
                }
            ply_list_free(obj->data.function->parameters);
            free(obj->data.function);
            }
        }
        break;
    case SCRIPT_OBJ_TYPE_NATIVE:
        if (obj->data.native.class->free_func)
            obj->data.native.class->free_func(obj);
        
        break;
    case SCRIPT_OBJ_TYPE_NULL:
        break;
    }
 obj->type = SCRIPT_OBJ_TYPE_NULL;
}

script_obj* script_obj_deref_direct (script_obj* obj)
{
 while (obj->type == SCRIPT_OBJ_TYPE_REF){
    obj = obj->data.obj;
    }
 return obj;
}

void script_obj_deref (script_obj** obj_ptr)
{
 script_obj* obj = *obj_ptr;
 obj = script_obj_deref_direct(obj);
 script_obj_ref (obj);
 script_obj_unref (*obj_ptr);
 *obj_ptr = obj;
}


static void foreach_print_vareable (void *key,
                                    void *data,
                                    void *user_data)
{
 script_vareable* vareable = data;
 char* string = script_obj_print (vareable->object);
 char* reply;
 char* prev = *(char**)user_data;
 if (!prev) prev = strdup("");
 asprintf(&reply, "%s%s = %s\n", prev, vareable->name, string);
 free(string);
 free(prev);
 *(char**)user_data = reply;
}




char* script_obj_print (script_obj* obj)
{
 char* reply;
 switch (obj->type){
    case SCRIPT_OBJ_TYPE_REF:
        {
        char* subobj = script_obj_print (obj->data.obj);
        asprintf(&reply, "->[%d]%s", obj->refcount, subobj);
        free(subobj);
        return reply;
        }
    
    case SCRIPT_OBJ_TYPE_INT:
        {
        asprintf(&reply, "%d[%d]", obj->data.integer, obj->refcount);
        return reply;
        }
    case SCRIPT_OBJ_TYPE_FLOAT:
        {
        asprintf(&reply, "%f[%d]", obj->data.floatpoint, obj->refcount);
        return reply;
        }
    case SCRIPT_OBJ_TYPE_STRING:
        {
        asprintf(&reply, "\"%s\"[%d]", obj->data.string, obj->refcount);
        return reply;
        }
    case SCRIPT_OBJ_TYPE_NULL:
        {
        asprintf(&reply, "NULL[%d]", obj->refcount);
        return reply;
        }
    case SCRIPT_OBJ_TYPE_HASH:
        {
        char* sub = NULL;
        ply_hashtable_foreach (obj->data.hash, foreach_print_vareable, &sub);
        asprintf(&reply, "HASH{\n%s}[%d]", sub, obj->refcount);
        free(sub);
        return reply;
        }
    case SCRIPT_OBJ_TYPE_FUNCTION:
        {
        asprintf(&reply, "function()[%d]", obj->refcount);
        return reply;
        }
    case SCRIPT_OBJ_TYPE_NATIVE:
        {
        asprintf(&reply, "(%s)[%d]", obj->data.native.class->name, obj->refcount);
        return reply;
        }
    
    default:
        printf("unhandeled object type %d\n", obj->type);
        assert(0);
    }
 return NULL;
}


script_obj* script_obj_new_null (void)
{
 script_obj* obj = malloc(sizeof(script_obj));
 obj->type = SCRIPT_OBJ_TYPE_NULL;
 obj->refcount = 1;
 return obj;
}

script_obj* script_obj_new_int (int number)
{
 script_obj* obj = malloc(sizeof(script_obj));
 obj->type = SCRIPT_OBJ_TYPE_INT;
 obj->refcount = 1;
 obj->data.integer = number;
 return obj;
}

script_obj* script_obj_new_float (float number)
{
 script_obj* obj = malloc(sizeof(script_obj));
 obj->type = SCRIPT_OBJ_TYPE_FLOAT;
 obj->refcount = 1;
 obj->data.floatpoint = number;
 return obj;
}

script_obj* script_obj_new_string (const char* string)
{
 if (!string) return script_obj_new_null ();
 script_obj* obj = malloc(sizeof(script_obj));
 obj->type = SCRIPT_OBJ_TYPE_STRING;
 obj->refcount = 1;
 obj->data.string = strdup(string);
 return obj;
}

script_obj* script_obj_new_hash (void)
{
 script_obj* obj = malloc(sizeof(script_obj));
 obj->type = SCRIPT_OBJ_TYPE_HASH;
 obj->data.hash = ply_hashtable_new (ply_hashtable_string_hash, ply_hashtable_string_compare);
 obj->refcount = 1;
 return obj;
}

script_obj* script_obj_new_function (script_function* function)
{
 script_obj* obj = malloc(sizeof(script_obj));
 obj->type = SCRIPT_OBJ_TYPE_FUNCTION;
 obj->data.function = function;
 obj->refcount = 1;
 return obj;
}

script_obj* script_obj_new_ref (script_obj* sub_obj)
{
 script_obj* obj = malloc(sizeof(script_obj));
 obj->type = SCRIPT_OBJ_TYPE_REF;
 obj->data.obj = sub_obj;
 obj->refcount = 1;
 return obj;
}


script_obj* script_obj_new_native (void* object_data, script_obj_native_class* class)
{
 script_obj* obj = malloc(sizeof(script_obj));
 obj->type = SCRIPT_OBJ_TYPE_NATIVE;
 obj->data.native.class = class;
 obj->data.native.object_data = object_data;
 obj->refcount = 1;
 return obj;
}

int script_obj_as_int (script_obj* obj)
{                                                     // If in then reply contents, otherwise reply 0
 obj = script_obj_deref_direct(obj);
 switch (obj->type){
    case SCRIPT_OBJ_TYPE_INT:
        return obj->data.integer;
    case SCRIPT_OBJ_TYPE_FLOAT:
        return (int) obj->data.floatpoint;
    case SCRIPT_OBJ_TYPE_NULL:
        return 0;
    case SCRIPT_OBJ_TYPE_REF:       // should have been de-reffed already
        assert(0);
    case SCRIPT_OBJ_TYPE_HASH:
    case SCRIPT_OBJ_TYPE_FUNCTION:
    case SCRIPT_OBJ_TYPE_NATIVE:
        return 0;
    case SCRIPT_OBJ_TYPE_STRING:
        return 0;
    }
    
 assert(0);         // Abort on uncaught
 return false;
}

float script_obj_as_float (script_obj* obj)
{                                                     // If in then reply contents, otherwise reply 0
 obj = script_obj_deref_direct(obj);
 switch (obj->type){
    case SCRIPT_OBJ_TYPE_INT:
        return (float) obj->data.integer;
    case SCRIPT_OBJ_TYPE_FLOAT:
        return obj->data.floatpoint;
    case SCRIPT_OBJ_TYPE_NULL:
        return 0;
    case SCRIPT_OBJ_TYPE_REF:       // should have been de-reffed already
        assert(0);
    case SCRIPT_OBJ_TYPE_HASH:
    case SCRIPT_OBJ_TYPE_FUNCTION:
    case SCRIPT_OBJ_TYPE_NATIVE:
        return 0;
    case SCRIPT_OBJ_TYPE_STRING:
        return 0;
    }
    
 assert(0);         // Abort on uncaught
 return false;
}

bool script_obj_as_bool (script_obj* obj)
{                                                 // False objects are NULL, 0, ""
 obj = script_obj_deref_direct(obj);
 switch (obj->type){
    case SCRIPT_OBJ_TYPE_INT:
        if (obj->data.integer) return true;
        return false;
    case SCRIPT_OBJ_TYPE_FLOAT:
        if (obj->data.floatpoint) return true;
        return false;
    case SCRIPT_OBJ_TYPE_NULL:
        return false;
    case SCRIPT_OBJ_TYPE_REF:       // should have been de-reffed already
        assert(0);
    case SCRIPT_OBJ_TYPE_HASH:
    case SCRIPT_OBJ_TYPE_FUNCTION:
    case SCRIPT_OBJ_TYPE_NATIVE:
        return true;
    case SCRIPT_OBJ_TYPE_STRING:
        if (*obj->data.string) return true;
        return false;
    }
    
 assert(0);         // Abort on uncaught
 return false;
}


char* script_obj_as_string (script_obj* obj)              // reply is strdupped
{
 obj = script_obj_deref_direct(obj);
 char* reply;
  
 switch (obj->type){
    case SCRIPT_OBJ_TYPE_INT:
        asprintf(&reply, "%d", obj->data.integer);
        return reply;
    case SCRIPT_OBJ_TYPE_FLOAT:
        asprintf(&reply, "%f", obj->data.floatpoint);
        return reply;
    case SCRIPT_OBJ_TYPE_NULL:
        asprintf(&reply, "NULL");
        return reply;
    case SCRIPT_OBJ_TYPE_REF:       // should have been de-reffed already
        assert(0);
    case SCRIPT_OBJ_TYPE_HASH:
        assert(0);                  // FIXME decide
    case SCRIPT_OBJ_TYPE_FUNCTION:
        assert(0);                  // FIXME decide
    case SCRIPT_OBJ_TYPE_STRING:
        return strdup(obj->data.string);
    case SCRIPT_OBJ_TYPE_NATIVE:
        assert(0);                  // FIXME decide
    }
    
 assert(0);         // Abort on uncaught
 return false;
}

void script_obj_assign (script_obj* obj_a, script_obj* obj_b)
{
 script_obj_reset (obj_a);
 obj_b = script_obj_deref_direct (obj_b);
 
 switch (obj_b->type){
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
        obj_a->data.string = strdup(obj_b->data.string);
        break;
    case SCRIPT_OBJ_TYPE_REF:       // should have been de-reffed already
        assert(0);
    case SCRIPT_OBJ_TYPE_HASH:
    case SCRIPT_OBJ_TYPE_FUNCTION:
    case SCRIPT_OBJ_TYPE_NATIVE:
        obj_a->type = SCRIPT_OBJ_TYPE_REF;
        obj_a->data.obj = obj_b;
        script_obj_ref(obj_b);
        break;
    }
}

script_obj* script_obj_hash_get_element (script_obj* hash, const char* name)
{
 assert(hash->type == SCRIPT_OBJ_TYPE_HASH);
 script_vareable* vareable = ply_hashtable_lookup (hash->data.hash, (void*)name);
 script_obj* obj;
 
 if (vareable) {
    obj = vareable->object;
    }
 else {
    obj = script_obj_new_null ();
    vareable = malloc(sizeof(script_vareable));
    vareable->name = strdup(name);
    vareable->object = obj;
    ply_hashtable_insert (hash->data.hash, vareable->name, vareable);
    }
 script_obj_ref (obj);
 return obj;
}


void script_obj_hash_add_element (script_obj* hash, script_obj* element, const char* name)
{
 assert(hash->type == SCRIPT_OBJ_TYPE_HASH);
 script_obj* obj = script_obj_hash_get_element (hash, name);
 script_obj_assign (obj, element);
 script_obj_unref (obj);
}

