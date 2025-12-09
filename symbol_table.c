#include "symbol_table.h"
#include<stdbool.h>
Symbol symtab[MAX_SYMBOLS];
int symcount = 0;

bool DEBUG_MODE_SYMB = false;

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
        fprintf(stderr, "Error at line %d: Variable '%s' is already declared (first declared as %s)\n", 
                line, name, type_to_string(s->type));
        if (error) *error = 1;
        return NULL;
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

    // Initialize memory info (will be computed later default to -1 muna)
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
    
    /* For chr type, store both representations */
    if (s->type == TYPE_CHR) {
        s->chrVal = (char)value;
        s->numVal = value;
    } else {
        s->numVal = value;
    }

    if (s->type == TYPE_FLEX)
        s->flexType = FLEX_NUMBER;
}

void set_char(Symbol *s, char value) {
    if (s->type == TYPE_FLEX) {
        s->flexType = FLEX_CHAR;
        s->chrVal = value;
        s->numVal = (int)value;  // Store integer representation too
    } else if (s->type == TYPE_CHR) {
        s->chrVal = value;
        s->numVal = (int)value;
    }
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
    
    // Compute offsets with proper alignment
    for (int i = 0; i < symcount; i++) {
        // 
        // if (symtab[i].type == TYPE_NMBR || symtab[i].type == TYPE_FLEX) {
        //     currentOffset = (currentOffset + 3) & ~3; // Align to 4-byte boundary
        // }
        
        symtab[i].memOffset = currentOffset;
        currentOffset += symtab[i].size;
    }
    
    if(DEBUG_MODE_SYMB){
        printf("\n=== Memory Layout ===\n");
        for (int i = 0; i < symcount; i++) {
            printf("%s: type=%s, offset=%d, size=%d bytes\n", 
                symtab[i].name, 
                type_to_string(symtab[i].type),
                symtab[i].memOffset, 
                symtab[i].size);
        }
        printf("Total memory required: %d bytes\n\n", currentOffset);
    }
}