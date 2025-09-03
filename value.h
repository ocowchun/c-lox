//
// Created by ocowchun on 2025/8/11.
//

#ifndef C_LOX_VALUE_H
#define C_LOX_VALUE_H

#include <string.h>

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

typedef uint64_t Value;


#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN ((uint64_t)0x7ffc000000000000)

#define TAG_NIL 1 // 01
#define TAG_FALSE 2 // 01
#define TAG_TRUE 3 // 01

#define IS_BOOL(value) (((value) | 1) == TRUE_VAL)
#define IS_NIL(value) ((value) == NIL_VAL)
#define IS_NUMBER(value) (((value) & QNAN) != QNAN)
#define IS_OBJ(value) \
    (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value) ((value) == TRUE_VAL)
#define AS_NUMBER(value) value_to_num(value)


// The tilde (~), if you haven’t done enough bit manipulation to encounter it before, is bitwise NOT.
// It toggles all ones and zeroes in its operand.
// By masking the value with the bitwise negation of the quiet NaN and sign bits, we clear those bits and let the pointer bits remain.
#define AS_OBJ(value) \
    ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b) ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL ((Value)(uint64_t)(QNAN | TAG_NIL))
#define NUMBER_VAL(number) num_to_value(number)


// The pointer itself is a full 64 bits,
// and in principle, it could thus overlap with some of those quiet NaN and sign bits.
// But in practice, at least on the architectures I’ve tested, everything above the 48th bit in a pointer is always zero.
#define OBJ_VAL(obj) \
    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

static inline double value_to_num(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

static inline Value num_to_value(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

#else

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

#define IS_BOOL(val) ((val).type == VAL_BOOL)
#define IS_NIL(val) ((val).type == VAL_NIL)
#define IS_NUMBER(val) ((val).type == VAL_NUMBER)
#define IS_OBJ(val) ((val).type == VAL_OBJ)

#define AS_BOOL(val) ((val).as.boolean)
#define AS_NUMBER(val) ((val).as.number)
#define AS_OBJ(val) ((val).as.obj)

#define BOOL_VAL(val) ((Value){VAL_BOOL, {.boolean = val}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(val) ((Value){VAL_NUMBER, {.number = val}})
#define OBJ_VAL(val) ((Value){VAL_OBJ, {.obj = (Obj*)val}})

#endif


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
