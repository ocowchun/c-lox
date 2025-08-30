//
// Created by ocowchun on 2025/8/13.
//

#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

typedef enum {
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_STRING,
    OBJ_NATIVE,
    OBJ_FUNCTION,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
} ObjType;

struct Obj {
    ObjType type;
    bool is_marked;
    struct Obj *next;
};

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_CLASS(value) isObjType(value, OBJ_CLASS)
#define IS_INSTANCE(value) isObjType(value, OBJ_INSTANCE)
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)

#define AS_CLASS(value) ((ObjClass *)AS_OBJ(value))
#define AS_INSTANCE(value) ((ObjInstance *)AS_OBJ(value))
#define AS_STRING(value) ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)
#define AS_CLOSURE(value) ((ObjClosure *)AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative*)AS_OBJ(value))->function)


struct ObjString {
    Obj obj;
    int length;
    char *chars;
    uint32_t hash;
};

typedef struct ObjUpvalue {
    Obj obj;
    Value *location;
    Value closed;
    struct ObjUpvalue *next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    int arity;
    int upvalue_count;
    Chunk chunk;
    ObjString *name;
} ObjFunction;

typedef struct ObjClosure {
    Obj obj;
    ObjFunction *function;
    // a pointer to a dynamically allocated array of pointers to upvalues
    ObjUpvalue **upvalues;
    int upvalue_count;
} ObjClosure;

typedef struct ObjClass {
    Obj obj;
    ObjString *name;
} ObjClass;

typedef struct ObjInstance {
    Obj obj;
    ObjClass *klass;
    Table fields;
} ObjInstance;

typedef Value (*NativeFn)(int arg_count, Value *args);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

ObjString *copy_string(const char *chars, int length);

ObjUpvalue *new_upvalue(Value *slot);

ObjString *take_string(char *chars, int length);

ObjClosure *new_closure(ObjFunction *function);

ObjFunction *new_function();

ObjNative *new_native(NativeFn function);

ObjClass *new_class(ObjString *name);

ObjInstance *new_instance(ObjClass *klass);

void print_object(Value value);


static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif //CLOX_OBJECT_H
