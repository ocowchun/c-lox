//
// Created by ocowchun on 2025/8/11.
//

#include <stdlib.h>

#include "memory.h"
#include "object.h"
#include "vm.h"

void *reallocate(void *pointer, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, new_size);
    if (result == NULL) {
        exit(1);
    }

    return result;
}

static void free_object(Obj *obj) {
    switch (obj->type) {
        case OBJ_UPVALUE: {
            FREE(ObjUpvalue, obj);
            break;
        }
        case OBJ_CLOSURE: {
            // TODO: study why??
            ObjClosure *closure = (ObjClosure *) obj;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalue_count);
            FREE(ObjClosure, obj);
            break;
        }
        case OBJ_FUNCTION: {
            // TODO: remove it since we only use OBJ_CLOSURE
            ObjFunction *function = (ObjFunction *) obj;
            free_chunk(&function->chunk);
            FREE(ObjFunction, obj);
            break;
        }
        case OBJ_NATIVE: {
            FREE(ObjNative, obj);
            break;
        }
        case OBJ_STRING: {
            ObjString *string = (ObjString *) obj;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, obj);
            break;
        }

    }

}

void free_objects() {
    Obj *object = vm.objects;

    while (object != NULL) {
        Obj *next = object->next;
        free_object(object);
        object = next;
    }
}

