#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "tac.h"


/* MIPS64 Opcodes and Function Codes */
#define OPCODE_DADDIU   0x19
#define OPCODE_LD       0x37
#define OPCODE_SD       0x3F

/* Function codes for R-format instructions */
#define FUNCT_DADDU     0x2D
#define FUNCT_DSUBU     0x2F
#define FUNCT_DMULT     0x1C
#define FUNCT_DDIV      0x1E
#define FUNCT_MFLO      0x12


/* MIPS64 Instruction Formats */
typedef enum {
    FORMAT_R,  // Register format
    FORMAT_I,  // Immediate format
} InstrFormat;


static int next_register = 0;

/* Track register allocation for temporaries and variables */
typedef struct {
    char name[64];
    const char *reg;
    int memOffset;
    int isVariable;  /* Only variables get memory, not temps */
} RegAlloc;

static RegAlloc regMap[100];
static int regMapCount = 0;
static int nextMemOffset = 0;

TACProgram *tac_create_program(void) {
    TACProgram *prog = malloc(sizeof(TACProgram));
    prog->head = NULL;
    prog->tail = NULL;
    prog->tempCount = 0;
    return prog;
}

TACOperand tac_operand_none(void) {
    TACOperand op = {.type = OPERAND_NONE};
    return op;
}

TACOperand tac_operand_temp(int num) {
    TACOperand op = {.type = OPERAND_TEMP, .val.tempNum = num};
    return op;
}

TACOperand tac_operand_var(const char *name) {
    TACOperand op = {.type = OPERAND_VAR, .val.varName = strdup(name)};
    return op;
}

TACOperand tac_operand_int(int val) {
    TACOperand op = {.type = OPERAND_INT, .val.intVal = val};
    return op;
}

TACOperand tac_operand_str(const char *val) {
    TACOperand op = {.type = OPERAND_STR, .val.strVal = strdup(val)};
    return op;
}

int tac_new_temp(TACProgram *prog) {
    return prog->tempCount++;
}

TACInstr *tac_emit(TACProgram *prog, TACOp op, TACOperand res, TACOperand a1, TACOperand a2, int line) {
    TACInstr *instr = malloc(sizeof(TACInstr));
    instr->op = op;
    instr->result = res;
    instr->arg1 = a1;
    instr->arg2 = a2;
    instr->line = line;
    instr->next = NULL;

    if (!prog->head) {
        prog->head = prog->tail = instr;
    } else {
        prog->tail->next = instr;
        prog->tail = instr;
    }
    return instr;
}

/* Generate TAC for expressions, returns the operand holding the result */
TACOperand tac_gen_expr(TACProgram *prog, ASTNode *node) {
    if (!node) return tac_operand_none();

    switch (node->type) {
        case NODE_NUM_LIT: {
            int t = tac_new_temp(prog);
            TACOperand res = tac_operand_temp(t);
            tac_emit(prog, TAC_LOAD_INT, res, tac_operand_int(node->data.numVal), tac_operand_none(), node->line);
            return res;
        }
        case NODE_CHR_LIT: {
            /* Convert character to ASCII value */
            int t = tac_new_temp(prog);
            TACOperand res = tac_operand_temp(t);
            tac_emit(prog, TAC_LOAD_INT, res, tac_operand_int((int)node->data.chrVal), tac_operand_none(), node->line);
            return res;
        }
        case NODE_STR_LIT: {
            int t = tac_new_temp(prog);
            TACOperand res = tac_operand_temp(t);
            tac_emit(prog, TAC_LOAD_STR, res, tac_operand_str(node->data.strVal), tac_operand_none(), node->line);
            return res;
        }
        case NODE_IDENT: {
            return tac_operand_var(node->data.strVal);
        }
        case NODE_BINOP: {
            TACOperand left = tac_gen_expr(prog, node->data.binop.left);
            TACOperand right = tac_gen_expr(prog, node->data.binop.right);
            int t = tac_new_temp(prog);
            TACOperand res = tac_operand_temp(t);
            TACOp op;
            switch (node->data.binop.op) {
                case OP_ADD: op = TAC_ADD; break;
                case OP_SUB: op = TAC_SUB; break;
                case OP_MUL: op = TAC_MUL; break;
                case OP_DIV: op = TAC_DIV; break;
                default: op = TAC_ADD; break;
            }
            tac_emit(prog, op, res, left, right, node->line);
            return res;
        }
        case NODE_CONCAT: {
            //emit instructions for both sides
            // and track them for printing
            TACOperand left = tac_gen_expr(prog, node->data.shw.left);
            TACOperand right = tac_gen_expr(prog, node->data.shw.right);
            
            // Emit CONCAT instruction to mark these should be printed together
            tac_emit(prog, TAC_CONCAT, tac_operand_none(), left, right, node->line);
            
            return tac_operand_none();
        }
        default:
            return tac_operand_none();
    }
}

/* Generate TAC for shw expressions - handles concatenation */
void tac_gen_shw_expr(TACProgram *prog, ASTNode *node, int line) {
    if (!node) return;
    
    if (node->type == NODE_CONCAT) {
        // Recursively process left and right
        tac_gen_shw_expr(prog, node->data.shw.left, line);
        tac_gen_shw_expr(prog, node->data.shw.right, line);
    } else {
        // Leaf node - generate expression and emit print
        TACOperand val = tac_gen_expr(prog, node);
        tac_emit(prog, TAC_CONCAT, tac_operand_none(), val, tac_operand_none(), line);
    }
}

/* Generate TAC for statements */
void tac_gen_stmt(TACProgram *prog, ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM:
            for (int i = 0; i < node->data.stmtList.count; i++) {
                tac_gen_stmt(prog, node->data.stmtList.stmts[i]);
            }
            break;

        case NODE_DECL: {
            if (node->data.decl.initExpr) {
                TACOperand val = tac_gen_expr(prog, node->data.decl.initExpr);
                tac_emit(prog, TAC_COPY, tac_operand_var(node->data.decl.varName), val, tac_operand_none(), node->line);
            }
            break;
        }
        case NODE_DECL_LIST: {
            tac_gen_stmt(prog, node->data.declList.left);
            tac_gen_stmt(prog, node->data.declList.right);
            break;
        }
        case NODE_ASSIGN: {
            TACOperand val = tac_gen_expr(prog, node->data.assign.expr);
            tac_emit(prog, TAC_COPY, tac_operand_var(node->data.assign.varName), val, tac_operand_none(), node->line);
            break;
        }
        case NODE_COMPOUND_ASSIGN: {
            TACOperand var = tac_operand_var(node->data.assign.varName);
            TACOperand val = tac_gen_expr(prog, node->data.assign.expr);
            int t = tac_new_temp(prog);
            TACOperand res = tac_operand_temp(t);
            TACOp op;
            switch (node->data.assign.op) {
                case OP_PLUS_ASSIGN:  op = TAC_ADD; break;
                case OP_MINUS_ASSIGN: op = TAC_SUB; break;
                case OP_MULT_ASSIGN:  op = TAC_MUL; break;
                case OP_DIV_ASSIGN:   op = TAC_DIV; break;
                default: op = TAC_ADD; break;
            }
            tac_emit(prog, op, res, var, val, node->line);
            tac_emit(prog, TAC_COPY, tac_operand_var(node->data.assign.varName), res, tac_operand_none(), node->line);
            break;
        }
        case NODE_SHW: {
            // Handle the entire shw expression tree
            tac_gen_shw_expr(prog, node->data.shw.left, node->line);
            // Emit a newline at the end
            tac_emit(prog, TAC_PRINT, tac_operand_none(), tac_operand_str("\n"), tac_operand_none(), node->line);
            break;
        }
        default:
            break;
    }
}

TACProgram *tac_generate(ASTNode *ast) {
    TACProgram *prog = tac_create_program();
    tac_gen_stmt(prog, ast);
    return prog;
}

static void print_operand(TACOperand op) {
    switch (op.type) {
        case OPERAND_NONE: break;
        case OPERAND_TEMP: printf("t%d", op.val.tempNum); break;
        case OPERAND_VAR:  printf("%s", op.val.varName); break;
        case OPERAND_INT:  printf("%d", op.val.intVal); break;
    }
}

const char *tac_op_to_string(TACOp op) {
    switch (op) {
        case TAC_ADD:      return "+";
        case TAC_SUB:      return "-";
        case TAC_MUL:      return "*";
        case TAC_DIV:      return "/";
        case TAC_COPY:     return "=";
        case TAC_LOAD_INT: return "load_int";
        case TAC_LOAD_STR: return "load_str";
        case TAC_PRINT:    return "print";
        case TAC_CONCAT:   return "concat";
        case TAC_DECL:     return "decl";
        default:           return "?";
    }
}

void tac_print(TACProgram *prog) {
    int instrNum = 1;
    for (TACInstr *i = prog->head; i; i = i->next) {
        
        if (i->op == TAC_PRINT || i->op == TAC_CONCAT) {
            continue;
        }
        
        printf("%3d: ", instrNum++);
        switch (i->op) {
            case TAC_ADD:
            case TAC_SUB:
            case TAC_MUL:
            case TAC_DIV:
                print_operand(i->result);
                printf(" = ");
                print_operand(i->arg1);
                printf(" %s ", tac_op_to_string(i->op));
                print_operand(i->arg2);
                break;
            case TAC_COPY:
                print_operand(i->result);
                printf(" = ");
                print_operand(i->arg1);
                break;
            case TAC_LOAD_INT:
            case TAC_LOAD_STR:
                print_operand(i->result);
                printf(" = ");
                print_operand(i->arg1);
                break;
            default:
                break;
        }
        printf("\n");
    }
}
/* Helper to get operand value */
static int get_operand_value(TACOperand op, int *tempValues) {
    switch (op.type) {
        case OPERAND_INT:
            return op.val.intVal;
        case OPERAND_TEMP:
            return tempValues[op.val.tempNum];
        case OPERAND_VAR: {
            Symbol *s = lookup(op.val.varName);
            if (s) {
                // Return ASCII value for chr type
                if (s->type == TYPE_CHR) {
                    return (int)s->chrVal;
                }
                // For flex, check runtime type
                else if (s->type == TYPE_FLEX) {
                    if (s->flexType == FLEX_CHAR) {
                        return (int)s->chrVal;
                    } else {
                        return s->numVal;
                    }
                }
                // For nmbr
                else {
                    return s->numVal;
                }
            }
            return 0;
        }
        default:
            return 0;
    }
}

/* Helper to set operand value */
static void set_operand_value(TACOperand op, int value, int *tempValues) {
    switch (op.type) {
        case OPERAND_TEMP:
            tempValues[op.val.tempNum] = value;
            break;
        case OPERAND_VAR: {
            Symbol *s = lookup(op.val.varName);
            if (s) {
                // For flex types, determine the type based on the value being set
                if (s->type == TYPE_FLEX) {
                    // If the value looks like an ASCII printable char, store as char
                    // Otherwise store as number
                    if (value >= 0 && value <= 127) {
                        // Could be either - we'll default to number for arithmetic results
                        set_number(s, value);
                    } else {
                        set_number(s, value);
                    }
                }
                // Use appropriate setter based on variable type
                else if (s->type == TYPE_CHR) {
                    set_char(s, (char)value);
                } else {
                    set_number(s, value);
                }
            }
            break;
        }
        default:
            break;
    }
}

/* Helper function to print an operand value */
/* Helper function to print an operand value */
static void print_operand_value(TACOperand op, int *tempValues, TACProgram *prog) {
    switch (op.type) {
        case OPERAND_INT:
            printf("%d", op.val.intVal);
            break;
            
        case OPERAND_STR:
            printf("%s", op.val.strVal);
            break;
            
        case OPERAND_TEMP: {
            // For temps that hold strings, we need to look back at the LOAD_STR instruction
            // For now, just print the numeric value
            int value = tempValues[op.val.tempNum];
            
            // Check if this temp was loaded from a string
            // We'll store a pointer to the string instruction
            TACInstr *instr = prog->head;
            while (instr) {
                if (instr->op == TAC_LOAD_STR && 
                    instr->result.type == OPERAND_TEMP && 
                    instr->result.val.tempNum == op.val.tempNum) {
                    // Found the string load instruction
                    printf("%s", instr->arg1.val.strVal);
                    return;
                }
                instr = instr->next;
            }
            
            // Otherwise print as number
            printf("%d", value);
            break;
        }
            
        case OPERAND_VAR: {
            Symbol *s = lookup(op.val.varName);
            if (s) {
                if (s->type == TYPE_CHR || (s->type == TYPE_FLEX && s->flexType == FLEX_CHAR)) {
                    printf("%c", s->chrVal);
                } else if (s->type == TYPE_NMBR || (s->type == TYPE_FLEX && s->flexType == FLEX_NUMBER)) {
                    printf("%d", s->numVal);
                }
            }
            break;
        }
            
        default:
            break;
    }
}

void tac_execute(TACProgram *prog) {
    if (!prog || !prog->head) return;
    
    /* Allocate temporary storage for all temps */
    int *tempValues = calloc(prog->tempCount, sizeof(int));
    if (!tempValues) {
        fprintf(stderr, "Failed to allocate temp storage\n");
        return;
    }
    
    /* Execute each instruction */
    for (TACInstr *instr = prog->head; instr; instr = instr->next) {
        switch (instr->op) {
            case TAC_LOAD_INT: {
                int value = get_operand_value(instr->arg1, tempValues);
                set_operand_value(instr->result, value, tempValues);
                break;
            }
            
            case TAC_LOAD_STR: {
                // For strings, we don't store them in tempValues
                // They'll be retrieved directly from the instruction when needed
                break;
            }
            
            case TAC_ADD: {
                int left = get_operand_value(instr->arg1, tempValues);
                int right = get_operand_value(instr->arg2, tempValues);
                set_operand_value(instr->result, left + right, tempValues);
                break;
            }
            
            case TAC_SUB: {
                int left = get_operand_value(instr->arg1, tempValues);
                int right = get_operand_value(instr->arg2, tempValues);
                set_operand_value(instr->result, left - right, tempValues);
                break;
            }
            
            case TAC_MUL: {
                int left = get_operand_value(instr->arg1, tempValues);
                int right = get_operand_value(instr->arg2, tempValues);
                set_operand_value(instr->result, left * right, tempValues);
                break;
            }
            
            case TAC_DIV: {
                int left = get_operand_value(instr->arg1, tempValues);
                int right = get_operand_value(instr->arg2, tempValues);
                if (right == 0) {
                    fprintf(stderr, "Runtime error at line %d: Division by zero\n", instr->line);
                    free(tempValues);
                    return;
                }
                set_operand_value(instr->result, left / right, tempValues);
                break;
            }
            
            case TAC_COPY: {
                int value = get_operand_value(instr->arg1, tempValues);
                set_operand_value(instr->result, value, tempValues);
                break;
            }

            case TAC_CONCAT: {
                print_operand_value(instr->arg1, tempValues, prog);
                break;
            }

            case TAC_PRINT: {
                // This is just for the final newline
                if (instr->arg1.type == OPERAND_STR) {
                    printf("%s", instr->arg1.val.strVal);
                }
                break;
            }
            
            default:
                break;
        }
    }
    
    free(tempValues);
}

static const char* get_next_register(void) {
    static const char* registers[] = {
        "$t0", "$t1", "$t2", "$t3", "$t4", 
        "$t5", "$t6", "$t7", "$t8", "$t9"
    };
    const char* reg = registers[next_register % 10];
    next_register++;
    return reg;
}

static const char* get_or_alloc_register(TACOperand op, RegAlloc **allocOut) {
    char key[64];
    int isVar = 0;
    
    switch (op.type) {
        case OPERAND_TEMP:
            sprintf(key, "t%d", op.val.tempNum);
            isVar = 0;
            break;
        case OPERAND_VAR:
            sprintf(key, "%s", op.val.varName);
            isVar = 1;
            break;
        default:
            return NULL;
    }
    
    /* Check if already allocated */
    for (int i = 0; i < regMapCount; i++) {
        if (strcmp(regMap[i].name, key) == 0) {
            if (allocOut) *allocOut = &regMap[i];
            return regMap[i].reg;
        }
    }
    
    /* Allocate new register */
    RegAlloc *alloc = &regMap[regMapCount++];
    strcpy(alloc->name, key);
    alloc->reg = get_next_register();
    alloc->isVariable = isVar;
    
    /* Only variables get memory offsets, not temporaries */
    if (isVar) {
        alloc->memOffset = nextMemOffset;
        nextMemOffset += 8; /* 8 bytes per word in MIPS64 */
    } else {
        alloc->memOffset = -1; /* Temps don't have memory */
    }
    
    if (allocOut) *allocOut = alloc;
    return alloc->reg;
}

/* Get memory offset for variable/temp */
static int get_memory_offset(TACOperand op) {
    if (op.type == OPERAND_VAR) {
        Symbol *s = lookup(op.val.varName);
        if (s) {
            return s->memOffset;
        }
    }
    return -1;  /* Temps don't have memory */
}

/* Register mappings */
static int get_register_number(const char *reg) {
    if (strcmp(reg, "$zero") == 0) return 0;
    if (strcmp(reg, "$at") == 0) return 1;
    
    // $t0-$t9 (temporaries)
    if (reg[0] == '$' && reg[1] == 't') {
        int num = atoi(reg + 2);
        if (num >= 0 && num <= 7) return 8 + num;   // $t0-$t7
        if (num >= 8 && num <= 9) return 24 + (num - 8); // $t8-$t9
    }
    
    // $s0-$s7 (saved)
    if (reg[0] == '$' && reg[1] == 's') {
        int num = atoi(reg + 2);
        if (num >= 0 && num <= 7) return 16 + num;
    }
    
    return 0; // default to $zero
}

/* Encode R-format instruction: op rs rt rd shamt funct */
static uint32_t encode_r_format(int funct, int rs, int rt, int rd, int shamt) {
    uint32_t instr = 0;
    instr |= (rs & 0x1F) << 21;
    instr |= (rt & 0x1F) << 16;
    instr |= (rd & 0x1F) << 11;
    instr |= (shamt & 0x1F) << 6;
    instr |= (funct & 0x3F);
    return instr;
}

static uint32_t encode_i_format(int opcode, int rs, int rt, int16_t immediate) {
    uint32_t instr = 0;
    instr |= (opcode & 0x3F) << 26;
    instr |= (rs & 0x1F) << 21;
    instr |= (rt & 0x1F) << 16;
    instr |= (immediate & 0xFFFF);
    return instr;
}

/* Print 32-bit binary representation */
static void print_binary(FILE *fp, uint32_t value) {
    for (int i = 31; i >= 0; i--) {
        fprintf(fp, "%d", (value >> i) & 1);
    }
}

/* Print hexadecimal representation */
static void print_hex(FILE *fp, uint32_t value) {
    fprintf(fp, "0x%08X", value);
}


/* Generate EduMIPS64 assembly and binary code */
/* Generate EduMIPS64 assembly and binary code - Print to console */
void tac_generate_assembly(TACProgram *prog, const char *output_file) {
    /* Reset allocator state */
    next_register = 0;
    
    /* Buffers to store output */
    char assembly_output[99999] = "";
    char hex_output[99999] = "";
    char binary_output[99999] = "";
    
    /* Add assembly header */
    strcat(assembly_output, ".data\n\n.code\n\n");
    
    /* Generate code for each TAC instruction */
    for (TACInstr *instr = prog->head; instr; instr = instr->next) {
        char asm_line[256] = "";
        uint32_t machine_code = 0;
        int has_machine_code = 1;
        
        switch (instr->op) {
            case TAC_LOAD_INT: {
                const char *destReg = get_or_alloc_register(instr->result, NULL);
                int value = instr->arg1.val.intVal;
                
                snprintf(asm_line, sizeof(asm_line), "daddiu %s, $zero, %d\n", destReg, value);
                strcat(assembly_output, asm_line);
                
                // Encode: daddiu rt, rs, immediate
                int rt = get_register_number(destReg);
                int rs = 0; // $zero
                machine_code = encode_i_format(OPCODE_DADDIU, rs, rt, (int16_t)value);
                break;
            }
            
            case TAC_ADD: {
                const char *leftReg = get_next_register();
                const char *rightReg = get_next_register();
                const char *destReg = get_or_alloc_register(instr->result, NULL);
                
                // Load left operand
                if (instr->arg1.type == OPERAND_VAR) {
                    int offset = get_memory_offset(instr->arg1);
                    snprintf(asm_line, sizeof(asm_line), "ld %s, %d($zero)\n", leftReg, offset);
                    strcat(assembly_output, asm_line);
                    
                    int rt = get_register_number(leftReg);
                    machine_code = encode_i_format(OPCODE_LD, 0, rt, (int16_t)offset);
                    
                    // Add to hex output
                    char hex_str[32];
                    sprintf(hex_str, "0x%08X\n", machine_code);
                    strcat(hex_output, hex_str);
                    
                    // Add to binary output
                    char bin_str[40];
                    for (int i = 31; i >= 0; i--) {
                        sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
                    }
                    strcat(binary_output, bin_str);
                    strcat(binary_output, "\n");
                } else {
                    leftReg = get_or_alloc_register(instr->arg1, NULL);
                    has_machine_code = 0;
                }
                
                // Load right operand
                if (instr->arg2.type == OPERAND_VAR) {
                    int offset = get_memory_offset(instr->arg2);
                    snprintf(asm_line, sizeof(asm_line), "ld %s, %d($zero)\n", rightReg, offset);
                    strcat(assembly_output, asm_line);
                    
                    int rt = get_register_number(rightReg);
                    machine_code = encode_i_format(OPCODE_LD, 0, rt, (int16_t)offset);
                    
                    char hex_str[32];
                    sprintf(hex_str, "0x%08X\n", machine_code);
                    strcat(hex_output, hex_str);
                    
                    char bin_str[40];
                    for (int i = 31; i >= 0; i--) {
                        sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
                    }
                    strcat(binary_output, bin_str);
                    strcat(binary_output, "\n");
                } else {
                    rightReg = get_or_alloc_register(instr->arg2, NULL);
                }
                
                // Perform addition
                snprintf(asm_line, sizeof(asm_line), "daddu %s, %s, %s\n", destReg, leftReg, rightReg);
                strcat(assembly_output, asm_line);
                
                // Encode: daddu rd, rs, rt
                int rd = get_register_number(destReg);
                int rs = get_register_number(leftReg);
                int rt = get_register_number(rightReg);
                machine_code = encode_r_format(FUNCT_DADDU, rs, rt, rd, 0);
                break;
            }
            
            case TAC_SUB: {
                const char *leftReg = get_next_register();
                const char *rightReg = get_next_register();
                const char *destReg = get_or_alloc_register(instr->result, NULL);
                
                if (instr->arg1.type == OPERAND_VAR) {
                    int offset = get_memory_offset(instr->arg1);
                    snprintf(asm_line, sizeof(asm_line), "ld %s, %d($zero)\n", leftReg, offset);
                    strcat(assembly_output, asm_line);
                    
                    int rt = get_register_number(leftReg);
                    machine_code = encode_i_format(OPCODE_LD, 0, rt, (int16_t)offset);
                    
                    char hex_str[32];
                    sprintf(hex_str, "0x%08X\n", machine_code);
                    strcat(hex_output, hex_str);
                    
                    char bin_str[40];
                    for (int i = 31; i >= 0; i--) {
                        sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
                    }
                    strcat(binary_output, bin_str);
                    strcat(binary_output, "\n");
                } else {
                    leftReg = get_or_alloc_register(instr->arg1, NULL);
                }
                
                if (instr->arg2.type == OPERAND_VAR) {
                    int offset = get_memory_offset(instr->arg2);
                    snprintf(asm_line, sizeof(asm_line), "ld %s, %d($zero)\n", rightReg, offset);
                    strcat(assembly_output, asm_line);
                    
                    int rt = get_register_number(rightReg);
                    machine_code = encode_i_format(OPCODE_LD, 0, rt, (int16_t)offset);
                    
                    char hex_str[32];
                    sprintf(hex_str, "0x%08X\n", machine_code);
                    strcat(hex_output, hex_str);
                    
                    char bin_str[40];
                    for (int i = 31; i >= 0; i--) {
                        sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
                    }
                    strcat(binary_output, bin_str);
                    strcat(binary_output, "\n");
                } else {
                    rightReg = get_or_alloc_register(instr->arg2, NULL);
                }
                
                snprintf(asm_line, sizeof(asm_line), "dsubu %s, %s, %s\n", destReg, leftReg, rightReg);
                strcat(assembly_output, asm_line);
                
                int rd = get_register_number(destReg);
                int rs = get_register_number(leftReg);
                int rt = get_register_number(rightReg);
                machine_code = encode_r_format(FUNCT_DSUBU, rs, rt, rd, 0);
                break;
            }
            
            case TAC_MUL: {
                const char *leftReg = get_next_register();
                const char *rightReg = get_next_register();
                const char *destReg = get_or_alloc_register(instr->result, NULL);
                
                if (instr->arg1.type == OPERAND_VAR) {
                    int offset = get_memory_offset(instr->arg1);
                    snprintf(asm_line, sizeof(asm_line), "ld %s, %d($zero)\n", leftReg, offset);
                    strcat(assembly_output, asm_line);
                    
                    int rt = get_register_number(leftReg);
                    machine_code = encode_i_format(OPCODE_LD, 0, rt, (int16_t)offset);
                    
                    char hex_str[32];
                    sprintf(hex_str, "0x%08X\n", machine_code);
                    strcat(hex_output, hex_str);
                    
                    char bin_str[40];
                    for (int i = 31; i >= 0; i--) {
                        sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
                    }
                    strcat(binary_output, bin_str);
                    strcat(binary_output, "\n");
                } else {
                    leftReg = get_or_alloc_register(instr->arg1, NULL);
                }
                
                if (instr->arg2.type == OPERAND_VAR) {
                    int offset = get_memory_offset(instr->arg2);
                    snprintf(asm_line, sizeof(asm_line), "ld %s, %d($zero)\n", rightReg, offset);
                    strcat(assembly_output, asm_line);
                    
                    int rt = get_register_number(rightReg);
                    machine_code = encode_i_format(OPCODE_LD, 0, rt, (int16_t)offset);
                    
                    char hex_str[32];
                    sprintf(hex_str, "0x%08X\n", machine_code);
                    strcat(hex_output, hex_str);
                    
                    char bin_str[40];
                    for (int i = 31; i >= 0; i--) {
                        sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
                    }
                    strcat(binary_output, bin_str);
                    strcat(binary_output, "\n");
                } else {
                    rightReg = get_or_alloc_register(instr->arg2, NULL);
                }
                
                snprintf(asm_line, sizeof(asm_line), "dmult %s, %s\n", leftReg, rightReg);
                strcat(assembly_output, asm_line);
                
                int rs = get_register_number(leftReg);
                int rt = get_register_number(rightReg);
                machine_code = encode_r_format(FUNCT_DMULT, rs, rt, 0, 0);
                
                char hex_str[32];
                sprintf(hex_str, "0x%08X\n", machine_code);
                strcat(hex_output, hex_str);
                
                char bin_str[40];
                for (int i = 31; i >= 0; i--) {
                    sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
                }
                strcat(binary_output, bin_str);
                strcat(binary_output, "\n");
                
                snprintf(asm_line, sizeof(asm_line), "mflo %s\n", destReg);
                strcat(assembly_output, asm_line);
                
                int rd = get_register_number(destReg);
                machine_code = encode_r_format(FUNCT_MFLO, 0, 0, rd, 0);
                break;
            }
            
            case TAC_DIV: {
                const char *leftReg = get_next_register();
                const char *rightReg = get_next_register();
                const char *destReg = get_or_alloc_register(instr->result, NULL);
                
                if (instr->arg1.type == OPERAND_VAR) {
                    int offset = get_memory_offset(instr->arg1);
                    snprintf(asm_line, sizeof(asm_line), "ld %s, %d($zero)\n", leftReg, offset);
                    strcat(assembly_output, asm_line);
                    
                    int rt = get_register_number(leftReg);
                    machine_code = encode_i_format(OPCODE_LD, 0, rt, (int16_t)offset);
                    
                    char hex_str[32];
                    sprintf(hex_str, "0x%08X\n", machine_code);
                    strcat(hex_output, hex_str);
                    
                    char bin_str[40];
                    for (int i = 31; i >= 0; i--) {
                        sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
                    }
                    strcat(binary_output, bin_str);
                    strcat(binary_output, "\n");
                } else {
                    leftReg = get_or_alloc_register(instr->arg1, NULL);
                }
                
                if (instr->arg2.type == OPERAND_VAR) {
                    int offset = get_memory_offset(instr->arg2);
                    snprintf(asm_line, sizeof(asm_line), "ld %s, %d($zero)\n", rightReg, offset);
                    strcat(assembly_output, asm_line);
                    
                    int rt = get_register_number(rightReg);
                    machine_code = encode_i_format(OPCODE_LD, 0, rt, (int16_t)offset);
                    
                    char hex_str[32];
                    sprintf(hex_str, "0x%08X\n", machine_code);
                    strcat(hex_output, hex_str);
                    
                    char bin_str[40];
                    for (int i = 31; i >= 0; i--) {
                        sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
                    }
                    strcat(binary_output, bin_str);
                    strcat(binary_output, "\n");
                } else {
                    rightReg = get_or_alloc_register(instr->arg2, NULL);
                }
                
                snprintf(asm_line, sizeof(asm_line), "ddiv %s, %s\n", leftReg, rightReg);
                strcat(assembly_output, asm_line);
                
                int rs = get_register_number(leftReg);
                int rt = get_register_number(rightReg);
                machine_code = encode_r_format(FUNCT_DDIV, rs, rt, 0, 0);
                
                char hex_str[32];
                sprintf(hex_str, "0x%08X\n", machine_code);
                strcat(hex_output, hex_str);
                
                char bin_str[40];
                for (int i = 31; i >= 0; i--) {
                    sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
                }
                strcat(binary_output, bin_str);
                strcat(binary_output, "\n");
                
                snprintf(asm_line, sizeof(asm_line), "mflo %s\n", destReg);
                strcat(assembly_output, asm_line);
                
                int rd = get_register_number(destReg);
                machine_code = encode_r_format(FUNCT_MFLO, 0, 0, rd, 0);
                break;
            }
            
            case TAC_COPY: {
                if (instr->arg1.type == OPERAND_INT) {
                    const char *tempReg = get_next_register();
                    int value = instr->arg1.val.intVal;
                    int destOffset = get_memory_offset(instr->result);
                    
                    if (destOffset >= 0) {
                        snprintf(asm_line, sizeof(asm_line), "daddiu %s, $zero, %d\n", tempReg, value);
                        strcat(assembly_output, asm_line);
                        
                        int rt = get_register_number(tempReg);
                        machine_code = encode_i_format(OPCODE_DADDIU, 0, rt, (int16_t)value);
                        
                        char hex_str[32];
                        sprintf(hex_str, "0x%08X\n", machine_code);
                        strcat(hex_output, hex_str);
                        
                        char bin_str[40];
                        for (int i = 31; i >= 0; i--) {
                            sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
                        }
                        strcat(binary_output, bin_str);
                        strcat(binary_output, "\n");
                        
                        snprintf(asm_line, sizeof(asm_line), "sd %s, %d($zero)\n", tempReg, destOffset);
                        strcat(assembly_output, asm_line);
                        
                        machine_code = encode_i_format(OPCODE_SD, 0, rt, (int16_t)destOffset);
                    } else {
                        has_machine_code = 0;
                    }
                } else if (instr->arg1.type == OPERAND_VAR) {
                    const char *tempReg = get_next_register();
                    int srcOffset = get_memory_offset(instr->arg1);
                    int destOffset = get_memory_offset(instr->result);
                    
                    if (srcOffset >= 0 && destOffset >= 0) {
                        snprintf(asm_line, sizeof(asm_line), "ld %s, %d($zero)\n", tempReg, srcOffset);
                        strcat(assembly_output, asm_line);
                        
                        int rt = get_register_number(tempReg);
                        machine_code = encode_i_format(OPCODE_LD, 0, rt, (int16_t)srcOffset);
                        
                        char hex_str[32];
                        sprintf(hex_str, "0x%08X\n", machine_code);
                        strcat(hex_output, hex_str);
                        
                        char bin_str[40];
                        for (int i = 31; i >= 0; i--) {
                            sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
                        }
                        strcat(binary_output, bin_str);
                        strcat(binary_output, "\n");
                        
                        snprintf(asm_line, sizeof(asm_line), "sd %s, %d($zero)\n", tempReg, destOffset);
                        strcat(assembly_output, asm_line);
                        
                        machine_code = encode_i_format(OPCODE_SD, 0, rt, (int16_t)destOffset);
                    } else {
                        has_machine_code = 0;
                    }
                } else {
                    const char *srcReg = get_or_alloc_register(instr->arg1, NULL);
                    int destOffset = get_memory_offset(instr->result);
                    
                    if (destOffset >= 0) {
                        snprintf(asm_line, sizeof(asm_line), "sd %s, %d($zero)\n", srcReg, destOffset);
                        strcat(assembly_output, asm_line);
                        
                        int rt = get_register_number(srcReg);
                        machine_code = encode_i_format(OPCODE_SD, 0, rt, (int16_t)destOffset);
                    } else {
                        has_machine_code = 0;
                    }
                }
                break;
            }
            
            default:
                has_machine_code = 0;
                break;
        }
        
        // Add hex and binary for the current instruction
        if (has_machine_code && strlen(asm_line) > 0) {
            char hex_str[32];
            sprintf(hex_str, "0x%08X\n", machine_code);
            strcat(hex_output, hex_str);
            
            char bin_str[40];
            for (int i = 31; i >= 0; i--) {
                sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
            }
            strcat(binary_output, bin_str);
            strcat(binary_output, "\n");
        }
    }
    
    // Print all outputs to console
    printf("assembly:\n\"%s\",", assembly_output);
    printf("\nbinary:\n\"%s\",", binary_output);
    printf("\nhex:\n\"%s\"", hex_output);
}
//free function
void tac_free(TACProgram *prog) {
    TACInstr *curr = prog->head;
    while (curr) {
        TACInstr *next = curr->next;
        if (curr->result.type == OPERAND_VAR) free(curr->result.val.varName);
        if (curr->result.type == OPERAND_STR) free(curr->result.val.strVal);
        if (curr->arg1.type == OPERAND_VAR) free(curr->arg1.val.varName);
        if (curr->arg1.type == OPERAND_STR) free(curr->arg1.val.strVal);
        if (curr->arg2.type == OPERAND_VAR) free(curr->arg2.val.varName);
        if (curr->arg2.type == OPERAND_STR) free(curr->arg2.val.strVal);
        free(curr);
        curr = next;
    }
    free(prog);
}