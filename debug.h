//
// Created by ocowchun on 2025/8/11.
//

#ifndef C_LOX_DEBUG_H
#define C_LOX_DEBUG_H

#include "chunk.h"

void disassemble_chunk(Chunk *chunk, const char *name);

int disassemble_instruction(Chunk *chunk, int offset);

#endif //C_LOX_DEBUG_H
