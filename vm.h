//
// Created by ocowchun on 2025/8/11.
//

#ifndef C_LOX_VM_H
#define C_LOX_VM_H

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256
typedef struct {
    Chunk *chunk;
    // instruction pointer
    // We use an actual real C pointer pointing right into the middle of the bytecode array instead of something
    // like an integer index because it’s faster to dereference a pointer than look up an element in an array by index.
    uint8_t *ip;
    value stack[STACK_MAX];
    value *stack_top;

} virtual_machine;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} interpret_result;

void init_virtual_machine();

void free_virtual_machine();

interpret_result interpret(const char *source);

void push(value val);

value pop();


#endif //C_LOX_VM_H
