/* script-execute.c - execution of scripts
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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "script.h"
#include "script-execute.h"
#include "script-object.h"

static script_obj* script_evaluate (script_state* state, script_exp* exp);
static script_return script_execute_function_with_parlist (script_state* state, script_function* function, ply_list_t* parameter_data);

static script_obj* script_evaluate_apply_function (script_state* state, script_exp* exp, script_obj* (*function) (script_obj*, script_obj*))
{
 script_obj* script_obj_a = script_evaluate (state, exp->data.dual.sub_a);
 script_obj* script_obj_b = script_evaluate (state, exp->data.dual.sub_b);
 script_obj* obj = function (script_obj_a, script_obj_b); 
 script_obj_unref (script_obj_a);
 script_obj_unref (script_obj_b);
 return obj;
}

static script_obj* script_evaluate_apply_function_and_assign (script_state* state, script_exp* exp, script_obj* (*function) (script_obj*, script_obj*))
{
 script_obj* script_obj_a = script_evaluate (state, exp->data.dual.sub_a);
 script_obj* script_obj_b = script_evaluate (state, exp->data.dual.sub_b);
 script_obj* obj = function (script_obj_a, script_obj_b);
 script_obj_assign (script_obj_a, obj);
 script_obj_unref (script_obj_a);
 script_obj_unref (script_obj_b);
 return obj;
}

static script_obj* script_evaluate_hash (script_state* state, script_exp* exp)
{
 script_obj* hash = script_evaluate (state, exp->data.dual.sub_a);
 script_obj* key  = script_evaluate (state, exp->data.dual.sub_b);
 script_obj* hash_dereffed = script_obj_deref_direct(hash);
 script_obj* obj;
 script_obj_deref (&key);
 
 if (hash_dereffed->type == SCRIPT_OBJ_TYPE_HASH){
    script_obj_deref (&hash);
    }
 else {
    script_obj_reset (hash);
    script_obj* newhash  = script_obj_new_hash ();
    hash->type = SCRIPT_OBJ_TYPE_REF;
    hash->data.obj = newhash;
    script_obj_deref (&hash);
    }
 
 
 char* name = script_obj_as_string(key);
 script_vareable* vareable = ply_hashtable_lookup (hash->data.hash, name);
 
 if (vareable) {
    obj = vareable->object;
    free(name);
    }
 else {
    obj = script_obj_new_null ();
    vareable = malloc(sizeof(script_vareable));
    vareable->name = name;
    vareable->object = obj;
    ply_hashtable_insert (hash->data.hash, vareable->name, vareable);
    }
 
 script_obj_ref (obj);
 script_obj_unref (hash);
 script_obj_unref (key);
 return obj;
}


static script_obj* script_evaluate_var (script_state* state, script_exp* exp)
{
 char* name = exp->data.string;
 script_obj* obj;
 script_obj_deref(&state->global);
 script_obj_deref(&state->local);
 assert (state->global->type == SCRIPT_OBJ_TYPE_HASH);
 assert (state->local->type == SCRIPT_OBJ_TYPE_HASH);
 
 script_vareable* vareable = ply_hashtable_lookup (state->local->data.hash, name);
 if (!vareable)
    vareable = ply_hashtable_lookup (state->global->data.hash, name);
 if (vareable) {
    obj = vareable->object;
    script_obj_ref (obj);
    return obj;
    }
 obj = script_obj_new_null ();

 vareable = malloc(sizeof(script_vareable));
 vareable->name = strdup(name);
 vareable->object = obj;

 ply_hashtable_insert (state->local->data.hash, vareable->name, vareable);
 script_obj_ref (obj);
 return obj;
}


static script_obj* script_evaluate_assign (script_state* state, script_exp* exp)
{
 script_obj* script_obj_a = script_evaluate (state, exp->data.dual.sub_a);
 script_obj* script_obj_b = script_evaluate (state, exp->data.dual.sub_b);
 script_obj_deref (&script_obj_b);
 script_obj_assign (script_obj_a, script_obj_b);
 
 script_obj_unref(script_obj_b);
 return script_obj_a;
}

static script_obj* script_evaluate_cmp (script_state* state, script_exp* exp)
{
 script_obj* script_obj_a = script_evaluate (state, exp->data.dual.sub_a);
 script_obj* script_obj_b = script_evaluate (state, exp->data.dual.sub_b);
 
 script_obj_deref (&script_obj_a);
 script_obj_deref (&script_obj_b);
 
 int gt=0;
 int lt=0;
 int eq=0;
 int ne=0;
 
 float val;
 int valset=0;
 
  switch (script_obj_a->type){
    case SCRIPT_OBJ_TYPE_INT:
        {
        switch (script_obj_b->type){
            case SCRIPT_OBJ_TYPE_INT:
                val = script_obj_a->data.integer - script_obj_b->data.integer;
                valset=1;
                break;
            case SCRIPT_OBJ_TYPE_FLOAT:
                val = script_obj_a->data.integer - script_obj_b->data.floatpoint;
                valset=1;
                break;
            default:
                ne = 1;
                break;
            }
        break;
        }
    case SCRIPT_OBJ_TYPE_FLOAT:
        {
        switch (script_obj_b->type){
            case SCRIPT_OBJ_TYPE_INT:
                val = script_obj_a->data.floatpoint - script_obj_b->data.integer;
                valset=1;
                break;
            case SCRIPT_OBJ_TYPE_FLOAT:
                val = script_obj_a->data.floatpoint - script_obj_b->data.floatpoint;
                valset=1;
                break;
            default:
                ne = 1;
                break;
            }
        break;
        }
    case SCRIPT_OBJ_TYPE_STRING:
        {
        switch (script_obj_b->type){
            case SCRIPT_OBJ_TYPE_STRING:
                val = strcmp(script_obj_a->data.string, script_obj_b->data.string);
                valset=1;
                break;
            default:
                ne = 1;
                break;
            }
        break;
        }
    case SCRIPT_OBJ_TYPE_HASH:
        {
        switch (script_obj_b->type){
            case SCRIPT_OBJ_TYPE_HASH:
                if (script_obj_a == script_obj_b) eq = 1;
                else ne = 1;
                break;
            default:
                ne = 1;
                break;
            }
        break;
        }
    case SCRIPT_OBJ_TYPE_FUNCTION:
        {
        switch (script_obj_b->type){
            case SCRIPT_OBJ_TYPE_FUNCTION:
                if (script_obj_a == script_obj_b) eq = 1;
                else ne = 1;
                break;
            default:
                ne = 1;
                break;
            }
        break;
        }
    case SCRIPT_OBJ_TYPE_NULL:
        {
        switch (script_obj_b->type){
            case SCRIPT_OBJ_TYPE_NULL:
                eq = 1;
                break;
            default:
                ne = 1;
                break;
            }
        break;
        }
    case SCRIPT_OBJ_TYPE_NATIVE:
        {
        switch (script_obj_b->type){
            case SCRIPT_OBJ_TYPE_NATIVE:
                if (script_obj_a->data.native.object_data == script_obj_b->data.native.object_data)
                    eq = 1;
                else ne = 1;
                break;
            default:
                ne = 1;
                break;
            }
        break;
        }
    case SCRIPT_OBJ_TYPE_REF:
        {
        assert(0);
        }
    }
 if(valset){
    if (val < 0) {lt = 1; ne = 1;}
    if (fabsf(val) == 0) eq = 1;
    if (val > 0) {gt = 1; ne = 1;}
    }
 
 int reply = 0;
 
 switch (exp->type){
    case SCRIPT_EXP_TYPE_EQ:
        if (eq) reply=1;
        break;
    case SCRIPT_EXP_TYPE_NE:
        if (ne) reply=1;
        break;
    case SCRIPT_EXP_TYPE_GT:
        if (gt) reply=1;
        break;
    case SCRIPT_EXP_TYPE_GE:
        if (gt || eq) reply=1;
        break;
    case SCRIPT_EXP_TYPE_LT:
        if (lt) reply=1;
        break;
    case SCRIPT_EXP_TYPE_LE:
        if (lt || eq) reply=1;      // CHECKME Errr so "(NULL <= NULL) is true" makes sense?
        break;
    default:
        assert(0);
    }
 
 script_obj_unref (script_obj_a);
 script_obj_unref (script_obj_b);
 
 return script_obj_new_int (reply);
}

static script_obj* script_evaluate_logic (script_state* state, script_exp* exp)
{
 script_obj* obj = script_evaluate (state, exp->data.dual.sub_a);
 if (exp->type == SCRIPT_EXP_TYPE_AND && !script_obj_as_bool(obj))
    return obj;
 else if (exp->type == SCRIPT_EXP_TYPE_OR &&script_obj_as_bool(obj))
    return obj;
 script_obj_unref (obj);
 obj = script_evaluate (state, exp->data.dual.sub_b);
 return obj;
}

static script_obj* script_evaluate_unary (script_state* state, script_exp* exp)
{
 script_obj* obj = script_evaluate (state, exp->data.sub);
 script_obj* new_obj;
 script_obj_deref (&obj);
 if (exp->type == SCRIPT_EXP_TYPE_NOT){
    int reply = !script_obj_as_bool(obj);
    script_obj_unref (obj);
    return script_obj_new_int (reply);
    }
 if (exp->type == SCRIPT_EXP_TYPE_POS){     // FIXME what should happen on non number operands?
    return obj;                             // Does nothing, maybe just remove at parse stage
    }
 if (exp->type == SCRIPT_EXP_TYPE_NEG){
    if (obj->type == SCRIPT_OBJ_TYPE_INT){
        new_obj = script_obj_new_int (-obj->data.integer);
        }
    else if (obj->type == SCRIPT_OBJ_TYPE_FLOAT){
        new_obj = script_obj_new_float (-obj->data.floatpoint);
        }
    else new_obj = script_obj_new_null ();
    script_obj_unref (obj);
    return new_obj;
    }
 
 
 int change_pre = 0;
 int change_post;
 
 if (exp->type == SCRIPT_EXP_TYPE_PRE_INC || SCRIPT_EXP_TYPE_POST_INC)
    change_post = 1;
 else 
    change_post = -1;
    
 if (exp->type == SCRIPT_EXP_TYPE_PRE_INC)
    change_pre = 1;
 else if (exp->type == SCRIPT_EXP_TYPE_PRE_DEC)
    change_pre = -1;
 
 if (obj->type == SCRIPT_OBJ_TYPE_INT){
    new_obj = script_obj_new_int (obj->data.integer + change_pre);
    obj->data.integer += change_post;
    }
 else if (obj->type == SCRIPT_OBJ_TYPE_FLOAT){
    new_obj = script_obj_new_float (obj->data.floatpoint + change_pre);
    obj->data.floatpoint += change_post;
    }
 else {
    new_obj = script_obj_new_null ();   // If performeing something like a=hash++; a and hash become NULL
    script_obj_reset (obj);
    }
 
 script_obj_unref (obj);
 return new_obj;
}

static script_obj* script_evaluate_func (script_state* state, script_exp* exp)
{
 script_obj* func = script_evaluate (state, exp->data.function.name);
 script_obj* obj = NULL;
 script_obj_deref (&func);
 
 if (func->type != SCRIPT_OBJ_TYPE_FUNCTION)
    return script_obj_new_null ();
 
 ply_list_t* parameter_expressions = exp->data.function.parameters;
 ply_list_t* parameter_data = ply_list_new();
 
 ply_list_node_t *node_expression = ply_list_get_first_node (parameter_expressions);
 while (node_expression){
    script_exp* data_exp = ply_list_node_get_data (node_expression);
    script_obj* data_obj = script_evaluate (state, data_exp);
    ply_list_append_data (parameter_data, data_obj);
    node_expression = ply_list_get_next_node (parameter_expressions, node_expression);
    }
 
 script_return reply = script_execute_function_with_parlist (state, func->data.function, parameter_data);
 if (reply.type == SCRIPT_RETURN_TYPE_RETURN)
    obj = reply.object;
 else
    obj = script_obj_new_null();
    
 ply_list_node_t *node_data = ply_list_get_first_node (parameter_data);
 while (node_data){
    script_obj* data_obj = ply_list_node_get_data (node_data);
    script_obj_unref (data_obj);
    node_data = ply_list_get_next_node (parameter_data, node_data);
    }
 ply_list_free(parameter_data);
 
 script_obj_unref (func);
 
 return obj;
}


static script_obj* script_evaluate (script_state* state, script_exp* exp)
{
 switch (exp->type){
    case SCRIPT_EXP_TYPE_PLUS:
        {
        return script_evaluate_apply_function (state, exp, script_obj_plus);
        }
    case SCRIPT_EXP_TYPE_MINUS:
        {
        return script_evaluate_apply_function (state, exp, script_obj_minus);
        }
    case SCRIPT_EXP_TYPE_MUL:
        {
        return script_evaluate_apply_function (state, exp, script_obj_mul);
        }
    case SCRIPT_EXP_TYPE_DIV:
        {
        return script_evaluate_apply_function (state, exp, script_obj_div);
        }
    case SCRIPT_EXP_TYPE_MOD:
        {
        return script_evaluate_apply_function (state, exp, script_obj_mod);
        }
    case SCRIPT_EXP_TYPE_EQ:
    case SCRIPT_EXP_TYPE_NE:
    case SCRIPT_EXP_TYPE_GT:
    case SCRIPT_EXP_TYPE_GE:
    case SCRIPT_EXP_TYPE_LT:
    case SCRIPT_EXP_TYPE_LE:
        {
        return script_evaluate_cmp (state, exp);
        }
    case SCRIPT_EXP_TYPE_AND:
    case SCRIPT_EXP_TYPE_OR:
        {
        return script_evaluate_logic (state, exp);
        }
    case SCRIPT_EXP_TYPE_NOT:
    case SCRIPT_EXP_TYPE_POS:
    case SCRIPT_EXP_TYPE_NEG:
    case SCRIPT_EXP_TYPE_PRE_INC:
    case SCRIPT_EXP_TYPE_PRE_DEC:
    case SCRIPT_EXP_TYPE_POST_INC:
    case SCRIPT_EXP_TYPE_POST_DEC:
        {
        return script_evaluate_unary (state, exp);
        }
    case SCRIPT_EXP_TYPE_TERM_INT:
        {
        return script_obj_new_int (exp->data.integer);
        }
    case SCRIPT_EXP_TYPE_TERM_FLOAT:
        {
        return script_obj_new_float (exp->data.floatpoint);
        }
    case SCRIPT_EXP_TYPE_TERM_STRING:
        {
        return script_obj_new_string (exp->data.string);
        }
    case SCRIPT_EXP_TYPE_TERM_NULL:
        {
        return script_obj_new_null();
        }
    case SCRIPT_EXP_TYPE_TERM_LOCAL:
        {
        script_obj_ref(state->local);
        return state->local;
        }
    case SCRIPT_EXP_TYPE_TERM_GLOBAL:
        {
        script_obj_ref(state->global);
        return state->global;
        }
    case SCRIPT_EXP_TYPE_TERM_VAR:
        {
        return script_evaluate_var (state, exp);
        }
    case SCRIPT_EXP_TYPE_ASSIGN:
        {
        return script_evaluate_assign (state, exp);
        }
    case SCRIPT_EXP_TYPE_ASSIGN_PLUS:
        {
        return script_evaluate_apply_function_and_assign (state, exp, script_obj_plus);
        }
    case SCRIPT_EXP_TYPE_ASSIGN_MINUS:
        {
        return script_evaluate_apply_function_and_assign (state, exp, script_obj_minus);
        }
    case SCRIPT_EXP_TYPE_ASSIGN_MUL:
        {
        return script_evaluate_apply_function_and_assign (state, exp, script_obj_mul);
        }
    case SCRIPT_EXP_TYPE_ASSIGN_DIV:
        {
        return script_evaluate_apply_function_and_assign (state, exp, script_obj_div);
        }
    case SCRIPT_EXP_TYPE_ASSIGN_MOD:
        {
        return script_evaluate_apply_function_and_assign (state, exp, script_obj_mod);
        }
    case SCRIPT_EXP_TYPE_HASH:
        {
        return script_evaluate_hash (state, exp);
        }
    case SCRIPT_EXP_TYPE_FUNCTION:
        {
        return script_evaluate_func (state, exp);
        }
//    default:
//        printf("unhandeled operation type %d\n", exp->type);
//        assert(0);
    }
 assert(0);
}

static script_return script_execute_list (script_state* state, ply_list_t* op_list)     // FIXME script_execute returns the return obj
{
 script_return reply = {SCRIPT_RETURN_TYPE_NORMAL, NULL};
 ply_list_node_t *node = ply_list_get_first_node (op_list);
 for (node = ply_list_get_first_node (op_list); node; node = ply_list_get_next_node (op_list, node)){
    script_op* op = ply_list_node_get_data (node);
    reply = script_execute(state, op);
    switch (reply.type) {
        case SCRIPT_RETURN_TYPE_NORMAL:
            break;
        case SCRIPT_RETURN_TYPE_RETURN:
        case SCRIPT_RETURN_TYPE_BREAK:
        case SCRIPT_RETURN_TYPE_CONTINUE:
            return reply;
        }
    }
 return reply;
}

                                                                                         // parameter_data list should be freed by caller
static script_return script_execute_function_with_parlist (script_state* state, script_function* function, ply_list_t* parameter_data)
{
 script_state* sub_state = script_state_init_sub(state);
 
 ply_list_t* parameter_names = function->parameters;
 
 ply_list_node_t *node_name = ply_list_get_first_node (parameter_names);
 ply_list_node_t *node_data = ply_list_get_first_node (parameter_data);
 while (node_name && node_data){
    script_obj* data_obj = ply_list_node_get_data (node_data);
    char* name = ply_list_node_get_data (node_name);
    
    script_obj_hash_add_element (sub_state->local, data_obj, name);
    
    node_name = ply_list_get_next_node (parameter_names, node_name);
    node_data = ply_list_get_next_node (parameter_data, node_data);
    }
 
 script_return reply;
 switch (function->type){
    case SCRIPT_FUNCTION_TYPE_SCRIPT:
        {
        script_op* op = function->data.script;
        reply = script_execute (sub_state, op);
        break;
        }
    case SCRIPT_FUNCTION_TYPE_NATIVE:
        {
        reply = function->data.native (sub_state, function->user_data);
        break;
        }
    }
 
 script_state_destroy(sub_state);
 return reply;
}


script_return script_execute_function (script_state* state, script_function* function, script_obj* first_arg,  ...)
{
 script_return reply;
 va_list args;
 script_obj* arg;
 ply_list_t *parameter_data = ply_list_new();
 
 arg = first_arg;
 va_start (args, first_arg);
 while (arg){
    ply_list_append_data (parameter_data, arg);
    arg = va_arg (args, script_obj*);
    }
 va_end (args);
 
 reply = script_execute_function_with_parlist (state, function, parameter_data);
 ply_list_free(parameter_data);
 
 return reply;
}


script_return script_execute (script_state* state, script_op* op)
{
 script_return reply = {SCRIPT_RETURN_TYPE_NORMAL, NULL};
 if (!op) return reply;
 switch (op->type){
    case SCRIPT_OP_TYPE_EXPRESSION:
        {
        script_obj* obj = script_evaluate (state, op->data.exp);
        script_obj_unref(obj);     // there is always a reply from all expressions (even assigns) which we chuck away
        break;
        }
    case SCRIPT_OP_TYPE_OP_BLOCK:
        {
        reply = script_execute_list (state, op->data.list);
                // FIXME blocks should normall reply a NULL , but if they replied something else then that was a return
        break;
        }
    case SCRIPT_OP_TYPE_IF:
        {
        script_obj* obj = script_evaluate (state, op->data.cond_op.cond);
        if (script_obj_as_bool(obj)){
            reply = script_execute (state, op->data.cond_op.op1);
            }
        else {
            reply = script_execute (state, op->data.cond_op.op2);
            }
        script_obj_unref(obj);
        break;
        }
    case SCRIPT_OP_TYPE_WHILE:
    case SCRIPT_OP_TYPE_FOR:
        {
        script_obj* obj;
        while (1){
            obj = script_evaluate (state, op->data.cond_op.cond);
            if (script_obj_as_bool(obj)){
                reply = script_execute (state, op->data.cond_op.op1);
                script_obj_unref(obj);
                switch (reply.type) {
                    case SCRIPT_RETURN_TYPE_NORMAL:
                        break;
                    case SCRIPT_RETURN_TYPE_RETURN:
                        return reply;
                    case SCRIPT_RETURN_TYPE_BREAK:
                        return (script_return){SCRIPT_RETURN_TYPE_NORMAL, NULL};
                    case SCRIPT_RETURN_TYPE_CONTINUE:
                        break;
                    }
                script_execute (state, op->data.cond_op.op2);
                }
            else {
                script_obj_unref(obj);
                break;
                }
            }
        break;
        }
    case SCRIPT_OP_TYPE_FUNCTION_DEF:
        {
        script_obj* obj = script_evaluate(state, op->data.function_def.name);
        script_obj_reset (obj);
        obj->type = SCRIPT_OBJ_TYPE_FUNCTION;
        obj->data.function = op->data.function_def.function;
        script_obj_unref(obj);
        break;
        }
    case SCRIPT_OP_TYPE_RETURN:
        {
        script_obj* obj;
        if (op->data.exp) obj = script_evaluate (state, op->data.exp);
        else obj = script_obj_new_null();
        reply = (script_return){SCRIPT_RETURN_TYPE_RETURN, obj};
        break;
        }
    case SCRIPT_OP_TYPE_BREAK:
        {
        reply = (script_return){SCRIPT_RETURN_TYPE_BREAK, NULL};
        break;
        }
    case SCRIPT_OP_TYPE_CONTINUE:
        {
        reply = (script_return){SCRIPT_RETURN_TYPE_CONTINUE, NULL};
        break;
        }
    }
 return reply;
}


 
