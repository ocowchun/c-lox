//
// Created by ocowchun on 2025/8/11.
//

#ifndef C_LOX_CHUNK_H
#define C_LOX_CHUNK_H

#include "common.h"
#include "value.h"


typedef enum {
    OP_CONSTANT,
    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_RETURN,
} OP_CODE;


typedef struct {
    int count;
    int capacity;
    uint8_t *code;
    int *lines;
    value_array constants;
} Chunk;

void init_chunk(Chunk *chunk);

void write_chunk(Chunk *chunk, uint8_t byte, int line);

void free_chunk(Chunk *chunk);

int add_constant(Chunk *chunk, value val);

#endif //C_LOX_CHUNK_H
