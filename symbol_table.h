#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SYMBOLS 999

typedef enum { 
    TYPE_NMBR, 
    TYPE_CHR, 
    TYPE_FLEX 
} VarType;

typedef enum {
    FLEX_NONE,
    FLEX_NUMBER,
    FLEX_CHAR,
} FlexType;

typedef struct {
    char *name;
    VarType type;     
    FlexType flexType;

    int numVal;
    char chrVal;
    char *strVal;
} Symbol;

extern Symbol symtab[MAX_SYMBOLS];
extern int symcount;

Symbol *lookup(const char *name);
Symbol *insert(const char *name, VarType type);
const char *type_to_string(VarType type);

// New functions
void set_number(Symbol *s, int value);
void set_char(Symbol *s, char value);
FlexType get_runtime_type(Symbol *s);

#endif