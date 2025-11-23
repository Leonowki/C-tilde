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

    // NEW: Initialize memory info (will be computed later)
    newSym->memOffset = -1;
    newSym->size = get_size_for_type(type);

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

// NEW: Get size in bytes for a variable type
int get_size_for_type(VarType type) {
    switch (type) {
        case TYPE_NMBR: return 4;  
        case TYPE_CHR:  return 1;  
        case TYPE_FLEX: return 8;  
        default:        return 8;
    }
}

// NEW: Compute memory offsets for all symbols
// Call this after parsing is complete, before TAC generation
void compute_symbol_offsets(void) {
    int currentOffset = 0;
    
    for (int i = 0; i < symcount; i++) {
        symtab[i].memOffset = currentOffset;
        currentOffset += symtab[i].size;
    }
    
    printf("\n=== Memory Layout ===\n");
    for (int i = 0; i < symcount; i++) {
        printf("%s: offset=%d, size=%d bytes\n", 
               symtab[i].name, symtab[i].memOffset, symtab[i].size);
    }
    printf("Total memory required: %d bytes\n\n", currentOffset);
}