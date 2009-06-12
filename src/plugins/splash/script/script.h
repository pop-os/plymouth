#ifndef SCRIPT_H
#define SCRIPT_H


#include "ply-hashtable.h"
#include "ply-list.h"
#include "ply-scan.h"


typedef enum                        // FIXME add _t to all types 
{
 SCRIPT_RETURN_TYPE_NORMAL,
 SCRIPT_RETURN_TYPE_RETURN,
 SCRIPT_RETURN_TYPE_BREAK,
 SCRIPT_RETURN_TYPE_CONTINUE,
} script_return_type;

struct script_obj;

typedef struct 
{
 script_return_type type;
 struct script_obj* object;
} script_return;

typedef struct 
{
 void* user_data;
 struct script_obj* global;
 struct script_obj* local;
} script_state;


typedef enum
{
 SCRIPT_FUNCTION_TYPE_SCRIPT,
 SCRIPT_FUNCTION_TYPE_NATIVE,
} script_function_type;


typedef script_return (*script_native_function) (script_state*, void*);

typedef struct script_function
{
 script_function_type type;
 ply_list_t* parameters;            //  list of char* typedef names
 void * user_data;
 union
 {
    script_native_function native;
    struct script_op* script;
 }data;
 bool freeable;
} script_function;

typedef void (*script_obj_function) (struct script_obj*);

typedef struct
{
 script_obj_function free_func;
 char* name;
 void* user_data;
} script_obj_native_class;

typedef struct
{
 void* object_data;
 script_obj_native_class* class;
} script_obj_native;

typedef enum
{
 SCRIPT_OBJ_TYPE_NULL,
 SCRIPT_OBJ_TYPE_REF,
 SCRIPT_OBJ_TYPE_INT,
 SCRIPT_OBJ_TYPE_STRING,
 SCRIPT_OBJ_TYPE_HASH,
 SCRIPT_OBJ_TYPE_FUNCTION,
 SCRIPT_OBJ_TYPE_NATIVE,
} script_obj_type;

typedef struct script_obj
{
 script_obj_type type;
 int refcount;
 union
 {
    int integer;
    char* string;
    struct script_obj* obj;
    script_function* function;
    ply_hashtable_t* hash;
    script_obj_native native;
 } data;
} script_obj;



typedef enum
{
 SCRIPT_EXP_TYPE_TERM_NULL,
 SCRIPT_EXP_TYPE_TERM_INT,
 SCRIPT_EXP_TYPE_TERM_STRING,
 SCRIPT_EXP_TYPE_TERM_VAR,
 SCRIPT_EXP_TYPE_TERM_LOCAL,
 SCRIPT_EXP_TYPE_TERM_GLOBAL,
 SCRIPT_EXP_TYPE_PLUS,
 SCRIPT_EXP_TYPE_MINUS,
 SCRIPT_EXP_TYPE_MUL,
 SCRIPT_EXP_TYPE_DIV,
 SCRIPT_EXP_TYPE_MOD,
 SCRIPT_EXP_TYPE_GT,
 SCRIPT_EXP_TYPE_GE,
 SCRIPT_EXP_TYPE_LT,
 SCRIPT_EXP_TYPE_LE,
 SCRIPT_EXP_TYPE_EQ,
 SCRIPT_EXP_TYPE_NE,
 SCRIPT_EXP_TYPE_HASH,
 SCRIPT_EXP_TYPE_FUNCTION,
 SCRIPT_EXP_TYPE_ASSIGN,
} script_exp_type;


typedef struct script_exp
{
 script_exp_type type;
 union
 {
    struct {
        struct script_exp* sub_a;
        struct script_exp* sub_b;
        } dual;
    struct script_exp *sub;
    char* string;
    int integer; 
    struct {
        struct script_exp* name;
        ply_list_t* parameters;
        } function;
 } data;
} script_exp;




typedef enum
{
 SCRIPT_OP_TYPE_EXPRESSION,
 SCRIPT_OP_TYPE_OP_BLOCK,
 SCRIPT_OP_TYPE_IF,
 SCRIPT_OP_TYPE_WHILE,
 SCRIPT_OP_TYPE_FUNCTION_DEF,
 SCRIPT_OP_TYPE_RETURN,
 SCRIPT_OP_TYPE_BREAK,
 SCRIPT_OP_TYPE_CONTINUE,
} script_op_type;

typedef struct script_op
{
 script_op_type type;
 union
 {
    script_exp* exp;
    ply_list_t* list;
    struct {
        script_exp* cond;
        struct script_op* op;
        } cond_op;
    struct {
        script_exp* name;
        script_function* function;
        } function_def;
 } data;
} script_op;


typedef struct 
{
 char* name;
 script_obj* object;
} script_vareable;

script_function* script_function_script_new(script_op* script, void* user_data, ply_list_t* parameter_list);
script_function* script_function_native_new(script_native_function native_function, void* user_data, ply_list_t* parameter_list);
void script_add_native_function (script_obj *hash, const char* name, script_native_function native_function, void* user_data, const char* first_arg, ...);
script_obj_native_class* script_obj_native_class_new(script_obj_function free_func, const char* name, void* user_data);
void script_obj_native_class_destroy(script_obj_native_class* class);
script_state* script_state_new(void* user_data);
script_state* script_state_init_sub(script_state* oldstate);
void script_state_destroy(script_state* state);

#endif /* SCRIPT_H */
