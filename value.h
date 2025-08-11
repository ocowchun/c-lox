//
// Created by ocowchun on 2025/8/11.
//

#ifndef C_LOX_VALUE_H
#define C_LOX_VALUE_H

#include "common.h"

typedef double value;

typedef struct {
    int capacity;
    int count;
    value *values;
} value_array;

void init_value_array(value_array *array);

void write_value_array(value_array *array, value val);

void free_value_array(value_array *array);

void print_value(value val);

#endif //C_LOX_VALUE_H
