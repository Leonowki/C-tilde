#include "symbol_table.h"
#include<stdbool.h>
Symbol symtab[MAX_SYMBOLS];
int symcount = 0;

bool DEBUG_MODE_SYMB = true;

Symbol *lookup(const char *name) {
    for (int i = 0; i < symcount; i++) {
        if (strcmp(symtab[i].name, name) == 0)
            return &symtab[i];
    }
    return NULL;
}

Symbol *insert(const char *name, VarType type, int line, int *error) {
    Symbol *s = lookup(name);
    if (s) {
        // Variable already exists - this is a redeclaration error
        fprintf(stderr, "LINE %d: Variable '%s' is already declared (first declared as %s)\n", 
                line, name, type_to_string(s->type));
        if (error) *error = 1;
        return NULL;  // Return NULL to indicate error
    }

    if (symcount >= MAX_SYMBOLS) {
        fprintf(stderr, "Symbol table full\n");
        if (error) *error = 1;
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

    // Initialize memory info (will be computed later)
    newSym->memOffset = -1;
    newSym->size = get_size_for_type(type);

    if (error) *error = 0;
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
    
    /* FIX: For chr type, store as character but allow integer assignment */
    if (s->type == TYPE_CHR) {
        s->chrVal = (char)value;  // Store as char
        s->numVal = value;         // Also store integer value for consistency
    } else {
        s->numVal = value;
    }

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

// Get size in bytes for a variable type
int get_size_for_type(VarType type) {
    switch (type) {
        case TYPE_NMBR: return 8;  
        case TYPE_CHR:  return 8;  
        case TYPE_FLEX: return 8;  
        default:        return 8;
    }
}

//after parsing is complete, before TAC generation
void compute_symbol_offsets(void) {
    int currentOffset = 0;
    //compute offsets
    for (int i = 0; i < symcount; i++) {
        symtab[i].memOffset = currentOffset;
        currentOffset += symtab[i].size;
    }
    
    if(DEBUG_MODE_SYMB){
        printf("\n=== Memory Layout ===\n");
        for (int i = 0; i < symcount; i++) {
            printf("%s: offset=%d, size=%d bytes\n", 
                symtab[i].name, symtab[i].memOffset, symtab[i].size);
        }
        printf("Total memory required: %d bytes\n\n", currentOffset);
    }
}