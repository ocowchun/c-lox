//
// Created by ocowchun on 2025/8/11.
//

#ifndef C_LOX_VALUE_H
#define C_LOX_VALUE_H

#include "common.h"

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
} value_type;

typedef struct {
    value_type type;
    union {
        bool boolean;
        double number;
    } as;
} value;

#define BOOL_VAL(val) ((value){VAL_BOOL, {.boolean = val}})
#define NIL_VAL ((value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(val) ((value){VAL_NUMBER, {.number = val}})


#define AS_BOOL(val) ((val).as.boolean)
#define AS_NUMBER(val) ((val).as.number)

#define IS_BOOL(val) ((val).type == VAL_BOOL)
#define IS_NIL(val) ((val).type == VAL_NIL)
#define IS_NUMBER(val) ((val).type == VAL_NUMBER)


typedef struct {
    int capacity;
    int count;
    value *values;
} value_array;

bool values_equal(value a, value b);

void init_value_array(value_array *array);

void write_value_array(value_array *array, value val);

void free_value_array(value_array *array);

void print_value(value val);

#endif //C_LOX_VALUE_H
