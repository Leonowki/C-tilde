#include "symbol_table.h"

Symbol symtab[MAX_SYMBOLS];
int symcount = 0;


Symbol *lookup(const char *name) {
    for (int i = 0; i < symcount; i++) {
        if (strcmp(symtab[i].name, name) == 0)
            return &symtab[i];
    }
    return NULL;
}

Symbol *insert(const char *name, VarType type) {
    Symbol *s = lookup(name);
    if (s) return s;

    if (symcount >= MAX_SYMBOLS) {
        fprintf(stderr, "Symbol table full\n");
        return NULL;
    }

    Symbol *newSym = &symtab[symcount++];
    newSym->name = strdup(name);
    newSym->type = type;

    newSym->numVal = 0;
    newSym->chrVal = '\0';
    newSym->strVal = NULL;

    if (type == TYPE_FLEX)
        newSym->flexType = FLEX_NONE;
    else
        newSym->flexType = FLEX_NONE;

    return newSym;
}

const char *type_to_string(VarType type) {
    switch (type) {
        case TYPE_NMBR: return "nmbr";
        case TYPE_CHR:  return "chr";
        case TYPE_FLEX: return "flex";
        default:        return "unknown";
    }
}

void set_number(Symbol *s, int value) {
    if (s->type == TYPE_FLEX && s->strVal) {
        free(s->strVal);
        s->strVal = NULL;
    }
    s->numVal = value;

    if (s->type == TYPE_FLEX)
        s->flexType = FLEX_NUMBER;
}

void set_char(Symbol *s, char value) {
    if (s->type == TYPE_FLEX && s->strVal) {
        free(s->strVal);
        s->strVal = NULL;
    }
    s->chrVal = value;

    if (s->type == TYPE_FLEX)
        s->flexType = FLEX_CHAR;
}



FlexType get_runtime_type(Symbol *s) {
    switch (s->type) {
        case TYPE_NMBR: return FLEX_NUMBER;
        case TYPE_CHR:  return FLEX_CHAR;
        case TYPE_FLEX: return s->flexType;
        default:        return FLEX_NONE;
    }
}