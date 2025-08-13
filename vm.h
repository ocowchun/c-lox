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
    Value stack[STACK_MAX];
    Value *stack_top;
    Obj *objects;

} VirtualMachine;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern VirtualMachine vm;

void init_virtual_machine();

void free_virtual_machine();

InterpretResult interpret(const char *source);

void push(Value val);

Value pop();


#endif //C_LOX_VM_H
