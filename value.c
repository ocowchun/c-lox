//
// Created by ocowchun on 2025/8/11.
//

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "value.h"
#include "object.h"


void init_value_array(ValueArray *array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void write_value_array(ValueArray *array, Value val) {
    if (array->capacity < array->count + 1) {
        int old_capacity = array->capacity;
        array->capacity = GROW_CAPACITY(array->capacity);
        array->values = GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
    }

    array->values[array->count] = val;
    array->count++;
}


void free_value_array(ValueArray *array) {
    array->values = FREE_ARRAY(Value, array->values, array->capacity);
    init_value_array(array);
}

void print_value(Value val) {
    switch (val.type) {
        case VAL_BOOL: {
            printf(AS_BOOL(val) ? "true" : "false");
            break;
        }
        case VAL_NIL: {
            printf("nil");
            break;
        }
        case VAL_NUMBER: {
            printf("%g", AS_NUMBER(val));
            break;
        }
        case VAL_OBJ: {
            print_object(val);
            break;
        }
    }
}

bool values_equal(Value a, Value b) {
    if (a.type != b.type) {
        return false;
    }
    switch (a.type) {
        case VAL_BOOL: {
            return AS_BOOL(a) == AS_BOOL(b);
        }
        case VAL_NIL:
            return true;
        case VAL_NUMBER: {
            return AS_NUMBER(a) == AS_NUMBER(b);
        }
        case VAL_OBJ: {
            ObjString *aString = AS_STRING(a);
            ObjString *bString = AS_STRING(b);
            return aString->length == bString->length && memcmp(aString->chars, bString->chars, aString->length) == 0;

        }
        default:
            // unreachable
            return false;
    }

}
