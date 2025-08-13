//
// Created by ocowchun on 2025/8/11.
//

#ifndef C_LOX_VALUE_H
#define C_LOX_VALUE_H

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj *obj;
    } as;
} Value;

#define BOOL_VAL(val) ((Value){VAL_BOOL, {.boolean = val}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(val) ((Value){VAL_NUMBER, {.number = val}})
#define OBJ_VAL(val) ((Value){VAL_OBJ, {.obj = (Obj*)val}})


#define AS_BOOL(val) ((val).as.boolean)
#define AS_NUMBER(val) ((val).as.number)
#define AS_OBJ(val) ((val).as.obj)

#define IS_BOOL(val) ((val).type == VAL_BOOL)
#define IS_NIL(val) ((val).type == VAL_NIL)
#define IS_NUMBER(val) ((val).type == VAL_NUMBER)
#define IS_OBJ(val) ((val).type == VAL_OBJ)


typedef struct {
    int capacity;
    int count;
    Value *values;
} ValueArray;

bool values_equal(Value a, Value b);

void init_value_array(ValueArray *array);

void write_value_array(ValueArray *array, Value val);

void free_value_array(ValueArray *array);

void print_value(Value val);

#endif //C_LOX_VALUE_H
