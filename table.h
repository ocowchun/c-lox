//
// Created by ocowchun on 2025/8/14.
//

#ifndef CLOX_TABLE_H
#define CLOX_TABLE_H

#include "common.h"
#include "value.h"

typedef struct {
    ObjString *key;
    Value value;
} Entry;

typedef struct {
    // the number of entries plus tombstones.
    int count;
    int capacity;
    Entry *entries;
} Table;

void init_table(Table *table);

void free_table(Table *table);

bool table_get(Table *table, ObjString *key, Value *value);

bool table_set(Table *table, ObjString *key, Value value);

bool table_delete(Table *table, ObjString *key);

void table_add_all(Table *from, Table *to);

ObjString *table_find_string(Table *table, const char *chars, int length, uint32_t hash);


void table_remove_white(Table *table);

void mark_table(Table *table);

#endif //CLOX_TABLE_H
