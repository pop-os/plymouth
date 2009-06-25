#ifndef SCRIPT_EXECUTE_H
#define SCRIPT_EXECUTE_H

#include "script.h"

script_return script_execute (script_state* state, script_op* op);
script_return script_execute_function (script_state* state, script_function* function, script_obj* first_arg,  ...);

#endif /* SCRIPT_EXECUTE_H */
