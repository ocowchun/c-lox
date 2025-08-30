//
// Created by ocowchun on 2025/8/11.
//

#include <stdlib.h>

#include "memory.h"
#include "object.h"
#include "vm.h"
#include "compiler.h"

#ifdef DEBUG_LOG_GC

#include <stdio.h>
#include "debug.h"

#endif

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(void *pointer, size_t old_size, size_t new_size) {
    vm.bytes_allocated += new_size - old_size;
    if (new_size > old_size) {
#ifdef DEBUG_STRESS_GC
        collect_garbage();
#endif
        if (vm.bytes_allocated > vm.next_gc) {
            collect_garbage();
        }
    }

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

void mark_object(Obj *object) {
    if (object == NULL) {
        return;
    }

    if (object->is_marked) {
        // avoid readd object which might cause infinite loop
        return;
    }

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void *) object);
    print_value(OBJ_VAL(object));
    printf("\n");
#endif

    object->is_marked = true;
    if (vm.gray_capacity < vm.gray_count + 1) {
        vm.gray_capacity = GROW_CAPACITY(vm.gray_capacity);

        // calls the system realloc() function and not our own reallocate() wrapper.
        // The memory for the gray stack itself is not managed by the garbage collector.
        // We don’t want growing the gray stack during a GC to cause the GC to recursively start a new GC.
        Obj **new_gray_stack = (Obj **) realloc(vm.gray_stack, sizeof(Obj *) * vm.gray_capacity);
        if (new_gray_stack == NULL) {
            exit(1);
        }
        vm.gray_stack = new_gray_stack;
    }

    vm.gray_stack[vm.gray_count++] = object;
}

void mark_value(Value value) {
    if (IS_OBJ(value)) {
        mark_object(AS_OBJ(value));
    }
}

static void mark_array(ValueArray *array) {
    for (int i = 0; i < array->count; ++i) {
        mark_value(array->values[i]);
    }
}

static void blacken_object(Obj *object) {
#ifdef  DEBUG_LOG_GC
    printf("%p blacken ", (void *) object);
    print_value(OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type) {
        case OBJ_CLASS: {
            ObjClass *klass = (ObjClass *) object;
            mark_object((Obj *) klass->name);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance *instance = (ObjInstance *) object;
            mark_object((Obj *) instance->klass);
            mark_table(&instance->fields);
            break;
        }
        case OBJ_UPVALUE: {
            // When an upvalue is closed, it contains a reference to the closed-over value.
            // Since the value is no longer on the stack, we need to make sure we trace the reference to it from the upvalue.
            mark_value(((ObjUpvalue *) object)->closed);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction *function = (ObjFunction *) object;
            mark_object((Obj *) function->name);
            mark_array(&function->chunk.constants);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure *closure = (ObjClosure *) object;
            mark_object((Obj *) closure->function);
            for (int i = 0; i < closure->upvalue_count; ++i) {
                mark_object((Obj *) closure->upvalues[i]);
            }
            break;
        }
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;

    }
}

static void free_object(Obj *obj) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void *) obj, obj->type);
#endif
    switch (obj->type) {
        case OBJ_CLASS: {
            FREE(ObjClass, obj);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance *instance = (ObjInstance *) obj;
            free_table(&instance->fields);
            FREE(ObjInstance, obj);
            break;
        }
        case OBJ_UPVALUE: {
            FREE(ObjUpvalue, obj);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure *closure = (ObjClosure *) obj;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalue_count);
            FREE(ObjClosure, obj);
            break;
        }
        case OBJ_FUNCTION: {
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

static void mark_roots() {
    for (Value *slot = vm.stack; slot < vm.stack_top; ++slot) {
        mark_value(*slot);
    }

    for (int i = 0; i < vm.frame_count; ++i) {
        mark_object((Obj *) vm.frames[i].closure);
    }

    for (ObjUpvalue *upvalue = vm.open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        mark_object((Obj *) upvalue);
    }

    mark_table(&vm.globals);
    mark_compiler_roots();
}

static void trace_references() {
    while (vm.gray_count > 0) {
        Obj *object = vm.gray_stack[--vm.gray_count];
        blacken_object(object);
    }
}

static void sweep() {
    Obj *previous = NULL;
    Obj *object = vm.objects;

    while (object != NULL) {
        if (object->is_marked) {
            object->is_marked = false;
            previous = object;
            object = object->next;
        } else {
            Obj *unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm.objects = object;
            }

            free_object(unreached);
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

    free(vm.gray_stack);
}

void collect_garbage() {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytes_allocated;
#endif

    mark_roots();
    trace_references();
    table_remove_white(&vm.strings);
    sweep();

    vm.next_gc = vm.bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
           before - vm.bytes_allocated, before, vm.bytes_allocated,
           vm.next_gc);
#endif
}
