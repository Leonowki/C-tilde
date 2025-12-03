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
/* Track which register holds which temp/var value */
/* Register allocation tracking */
typedef struct {
    int tempNum;           
    int lastUseDistance;   
    int isDirty;          
} RegisterState;

#define NUM_WORK_REGS 8 
static RegisterState regState[NUM_WORK_REGS];

/* Forward declarations */
static int get_register_number(const char *reg);
static uint32_t encode_i_format(int opcode, int rs, int rt, int16_t immediate);
static uint32_t encode_r_format(int funct, int rs, int rt, int rd, int shamt);
static int find_temp_in_register(int tempNum);

/* Initialize register state */
static void init_register_state(void) {
    for (int i = 0; i < NUM_WORK_REGS; i++) {
        regState[i].tempNum = -1;
        regState[i].lastUseDistance = 0;
        regState[i].isDirty = 0;
    }
}

/* Get register name by index */
static const char* get_reg_name(int idx) {
    static const char* regNames[] = {"r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9"};
    if (idx >= 0 && idx < NUM_WORK_REGS) {
        return regNames[idx];
    }
    return "r2"; // fallback
}

/* Find how many instructions until this temp is used again */
static int find_next_use_distance(TACInstr *current, int tempNum) {
    int distance = 0;
    TACInstr *instr = current->next;
    
    while (instr) {
        distance++;
        
        // Check if temp is used as operand
        if ((instr->arg1.type == OPERAND_TEMP && instr->arg1.val.tempNum == tempNum) ||
            (instr->arg2.type == OPERAND_TEMP && instr->arg2.val.tempNum == tempNum)) {
            return distance;
        }
        
        // If temp is overwritten, it's not used again
        if (instr->result.type == OPERAND_TEMP && instr->result.val.tempNum == tempNum) {
            return 9999; // Never used
        }
        
        instr = instr->next;
    }
    
    return 9999; // Not used again
}

static int temp_is_used_later(TACInstr *current, int tempNum) {
    TACInstr *instr = current->next;
    
    while (instr) {
        if ((instr->arg1.type == OPERAND_TEMP && instr->arg1.val.tempNum == tempNum) ||
            (instr->arg2.type == OPERAND_TEMP && instr->arg2.val.tempNum == tempNum)) {
            return 1;
        }
        
        // If temp is overwritten, stop looking
        if (instr->result.type == OPERAND_TEMP && instr->result.val.tempNum == tempNum) {
            return 0;
        }
        
        instr = instr->next;
    }
    
    return 0;
}

/* Find best register to evict (using LRU - Least Recently Used / Furthest Next Use) */
static int find_register_to_evict(TACInstr *current) {
    int bestReg = 0;
    int maxDistance = regState[0].lastUseDistance;
    
    for (int i = 1; i < NUM_WORK_REGS; i++) {
        if (regState[i].tempNum == -1) {
            return i; // Empty register, use it
        }
        
        if (regState[i].lastUseDistance > maxDistance) {
            maxDistance = regState[i].lastUseDistance;
            bestReg = i;
        }
    }
    
    return bestReg;
}

static void spill_register(int regIdx, char *output, char *hex_out, char *bin_out, int tempStorageOffset) {
    if (regState[regIdx].tempNum == -1 || !regState[regIdx].isDirty) {
        return; // Nothing to spill
    }
    
    int tempNum = regState[regIdx].tempNum;
    int offset = tempStorageOffset + (tempNum * 8);
    const char *regName = get_reg_name(regIdx);
    
    char line[256];
    snprintf(line, sizeof(line), "sd %s, %d(r0)\n", regName, offset);
    strcat(output, line);
    
    // Generate machine code
    int rt = get_register_number(regName);
    uint32_t machine_code = encode_i_format(OPCODE_SD, 0, rt, (int16_t)offset);
    
    char hex_str[32];
    sprintf(hex_str, "0x%08X\n", machine_code);
    strcat(hex_out, hex_str);
    
    char bin_str[40];
    for (int i = 31; i >= 0; i--) {
        sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
    }
    strcat(bin_out, bin_str);
    strcat(bin_out, "\n");
    
    regState[regIdx].isDirty = 0;
}

/* Allocate register for a temp */
static int allocate_register_for_temp(TACInstr *current, int tempNum, 
                                      char *output, char *hex_out, char *bin_out, 
                                      int tempStorageOffset) {
    // Check if already in a register
    int regIdx = find_temp_in_register(tempNum);
    if (regIdx != -1) {
        return regIdx;
    }
    
    // First, try to find an empty register or one with a dead temp
    for (int i = 0; i < NUM_WORK_REGS; i++) {
        if (regState[i].tempNum == -1) {
            // Empty register - use it
            regState[i].tempNum = tempNum;
            regState[i].lastUseDistance = find_next_use_distance(current, tempNum);
            regState[i].isDirty = 0;
            return i;
        }
        
        // Check if this register holds a dead temp (never used again)
        if (find_next_use_distance(current, regState[i].tempNum) == 9999) {
            // This temp is dead - can safely reuse
            regState[i].tempNum = tempNum;
            regState[i].lastUseDistance = find_next_use_distance(current, tempNum);
            regState[i].isDirty = 0;
            return i;
        }
    }
    //HMMMMM
    regIdx = find_register_to_evict(current);
    spill_register(regIdx, output, hex_out, bin_out, tempStorageOffset);
    
    regState[regIdx].tempNum = tempNum;
    regState[regIdx].lastUseDistance = find_next_use_distance(current, tempNum);
    regState[regIdx].isDirty = 0;
    
    return regIdx;
}
/* Load operand into register, with optional exclusion list */
static int load_operand_ex(TACOperand op, TACInstr *current,
                       char *output, char *hex_out, char *bin_out,
                       int tempStorageOffset, int excludeReg) {
    
    if (op.type == OPERAND_TEMP) {
        // Check if already in register
        int regIdx = find_temp_in_register(op.val.tempNum);
        if (regIdx != -1) {
            return regIdx; // Already loaded
        }
        
        // ERROR: Temp should be in a register but isn't
        // This means register allocation failed
        fprintf(stderr, "ERROR: Temp t%d not in register\n", op.val.tempNum);
        return 0;
        
    } else if (op.type == OPERAND_VAR) {
        // Variables are loaded from their memory location (NOT from tempStorage!)
        // Find best register for loading variable
        int regIdx = -1;
        
        // 1. Try empty register first
        for (int i = 0; i < NUM_WORK_REGS; i++) {
            if (i != excludeReg && regState[i].tempNum == -1) {
                regIdx = i;
                break;
            }
        }
        
        // 2. Try register with dead temp
        if (regIdx == -1) {
            for (int i = 0; i < NUM_WORK_REGS; i++) {
                if (i != excludeReg && regState[i].tempNum != -1) {
                    if (find_next_use_distance(current, regState[i].tempNum) == 9999) {
                        regIdx = i;
                        break;
                    }
                }
            }
        }
        
        // 3. Use any register except excluded
        if (regIdx == -1) {
            for (int i = 0; i < NUM_WORK_REGS; i++) {
                if (i != excludeReg) {
                    regIdx = i;
                    break;
                }
            }
        }
        
        if (regIdx == -1) regIdx = 0; // Fallback
        
        // Clear register state - we're loading a variable, not tracking it as a temp
        regState[regIdx].tempNum = -1;
        regState[regIdx].isDirty = 0;
        
        // Load variable from its actual memory location
        Symbol *s = lookup(op.val.varName);
        if (!s) return regIdx;
        
        const char *regName = get_reg_name(regIdx);
        
        char line[256];
        snprintf(line, sizeof(line), "ld %s, %d(r0)\n", regName, s->memOffset);
        strcat(output, line);
        
        int rt = get_register_number(regName);
        uint32_t machine_code = encode_i_format(OPCODE_LD, 0, rt, (int16_t)s->memOffset);
        
        char hex_str[32];
        sprintf(hex_str, "0x%08X\n", machine_code);
        strcat(hex_out, hex_str);
        
        char bin_str[40];
        for (int i = 31; i >= 0; i--) {
            sprintf(bin_str + (31 - i), "%d", (machine_code >> i) & 1);
        }
        strcat(bin_out, bin_str);
        strcat(bin_out, "\n");
        
        return regIdx;
    }
    
    return 0;
}


/* Load operand into register */
static int load_operand(TACOperand op, TACInstr *current,
                       char *output, char *hex_out, char *bin_out,
                       int tempStorageOffset) {
    return load_operand_ex(op, current, output, hex_out, bin_out, tempStorageOffset, -1);
}

/* Find register holding a temp, or -1 if not in register */
static int find_temp_in_register(int tempNum) {
    for (int i = 0; i < NUM_WORK_REGS; i++) {
        if (regState[i].tempNum == tempNum) {
            return i;
        }
    }
    return -1;
}


static TACInstr *tac_emit_shw(TACProgram *prog, TACOp op, TACOperand res, TACOperand a1, TACOperand a2, int line) {
    TACInstr *instr = tac_emit(prog, op, res, a1, a2, line);
    instr->inShwContext = 1;  /* Mark as shw-only */
    return instr;
}

/* Check if we need to store this temp to memory (it's used later) */
static int temp_needs_memory(TACProgram *prog, TACInstr *current, int tempNum) {
    // If result is a variable, it always needs memory
    if (current->result.type == OPERAND_VAR) {
        return 1;
    }
    
    // Count how many instructions ahead this temp is used
    int useCount = 0;
    int instructionsAhead = 0;
    TACInstr *check = current->next;
    
    while (check) {
        instructionsAhead++;
        
        // Check if this temp is used as an operand
        if ((check->arg1.type == OPERAND_TEMP && check->arg1.val.tempNum == tempNum) ||
            (check->arg2.type == OPERAND_TEMP && check->arg2.val.tempNum == tempNum)) {
            useCount++;
            
            // If used only once and within the next few instructions, keep in register
            if (useCount == 1 && instructionsAhead <= 3) {
                // Check if it's used again after this
                TACInstr *furtherCheck = check->next;
                int usedAgain = 0;
                while (furtherCheck) {
                    if ((furtherCheck->arg1.type == OPERAND_TEMP && furtherCheck->arg1.val.tempNum == tempNum) ||
                        (furtherCheck->arg2.type == OPERAND_TEMP && furtherCheck->arg2.val.tempNum == tempNum)) {
                        usedAgain = 1;
                        break;
                    }
                    furtherCheck = furtherCheck->next;
                }
                
                if (!usedAgain) {
                    return 0;  // Don't need memory - single use within register window
                }
            }
            // If used multiple times or far away, needs memory
            return 1;
        }
        // Don't look too far ahead for register-only temps
        if (instructionsAhead > 10) {
            break;
        }
        check = check->next;
    }
    
    // If temp is never used after creation, don't store it
    // (This handles dead code that optimizations might have missed)
    return useCount > 0;
}

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
    instr->inShwContext = 0;
    instr->resultIsChar = 0;  // ← ADD THIS LINE
    instr->next = NULL;

    if (!prog->head) {
        prog->head = prog->tail = instr;
    } else {
        prog->tail->next = instr;
        prog->tail = instr;
    }
    return instr;
}

/* Generate TAC for expressions, returns the operand holding the result helper for cases using shw context */
TACOperand tac_gen_expr_ctx(TACProgram *prog, ASTNode *node, int inShwContext) {
    if (!node) return tac_operand_none();

    switch (node->type) {
        case NODE_NUM_LIT: {
            int t = tac_new_temp(prog);
            TACOperand res = tac_operand_temp(t);
            if (inShwContext) {
                tac_emit_shw(prog, TAC_LOAD_INT, res, tac_operand_int(node->data.numVal), tac_operand_none(), node->line);
            } else {
                tac_emit(prog, TAC_LOAD_INT, res, tac_operand_int(node->data.numVal), tac_operand_none(), node->line);
            }
            return res;
        }
        
        case NODE_STR_LIT: {
            // For string literals, return directly
            return tac_operand_str(node->data.strVal);
        }
        
        case NODE_CHR_LIT: {
            int t = tac_new_temp(prog);
            TACOperand res = tac_operand_temp(t);
            res.isCharType = 1;
            TACInstr *instr;
            if (inShwContext) {
                instr = tac_emit_shw(prog, TAC_LOAD_INT, res, tac_operand_int((int)node->data.chrVal), tac_operand_none(), node->line);
            } else {
                instr = tac_emit(prog, TAC_LOAD_INT, res, tac_operand_int((int)node->data.chrVal), tac_operand_none(), node->line);
            }
            instr->resultIsChar = 1;
            return res;
        }
        
        case NODE_IDENT: {
            TACOperand op = tac_operand_var(node->data.strVal);
            Symbol *s = lookup(node->data.strVal);
            if (s && s->type == TYPE_CHR) {
                op.isCharType = 1;
            }
            return op;
        }
        
        case NODE_BINOP: {
            TACOperand left = tac_gen_expr_ctx(prog, node->data.binop.left, inShwContext);
            TACOperand right = tac_gen_expr_ctx(prog, node->data.binop.right, inShwContext);
            int t = tac_new_temp(prog);
            TACOperand res = tac_operand_temp(t);
            
            res.isCharType = left.isCharType;
            
            TACOp op;
            switch (node->data.binop.op) {
                case OP_ADD: op = TAC_ADD; break;
                case OP_SUB: op = TAC_SUB; break;
                case OP_MUL: op = TAC_MUL; break;
                case OP_DIV: op = TAC_DIV; break;
                default: op = TAC_ADD; break;
            }
            
            TACInstr *instr;
            if (inShwContext) {
                instr = tac_emit_shw(prog, op, res, left, right, node->line);
            } else {
                instr = tac_emit(prog, op, res, left, right, node->line);
            }
            
            instr->resultIsChar = res.isCharType;
            
            return res;
        }
        
        case NODE_CONCAT: {
            TACOperand left = tac_gen_expr_ctx(prog, node->data.shw.left, inShwContext);
            TACOperand right = tac_gen_expr_ctx(prog, node->data.shw.right, inShwContext);
            tac_emit_shw(prog, TAC_CONCAT, tac_operand_none(), left, right, node->line);
            return tac_operand_none();
        }
        
        default:
            return tac_operand_none();
    }
}


TACOperand tac_gen_expr(TACProgram *prog, ASTNode *node) {
    return tac_gen_expr_ctx(prog, node, 0);  /* Default: not in shw context */
}

/* Generate TAC for shw expressions - handles concatenation */
void tac_gen_shw_expr(TACProgram *prog, ASTNode *node, int line) {
    if (!node) return;
    
    if (node->type == NODE_CONCAT) {
        // Recursively process left and right
        tac_gen_shw_expr(prog, node->data.shw.left, line);
        tac_gen_shw_expr(prog, node->data.shw.right, line);
    } else {
        // Leaf node - generate expression with shw context flag
        TACOperand val = tac_gen_expr_ctx(prog, node, 1);  
        tac_emit_shw(prog, TAC_CONCAT, tac_operand_none(), val, tac_operand_none(), line);
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


//Optimizer
static void optimize_simple_assignments(TACProgram *prog) {
    TACInstr *curr = prog->head;
    
    while (curr && curr->next) {
        TACInstr *next = curr->next;
        
        // Pattern: LOAD_INT into temp, immediately COPY temp to var
        if (curr->op == TAC_LOAD_INT && 
            curr->result.type == OPERAND_TEMP &&
            next->op == TAC_COPY &&
            next->arg1.type == OPERAND_TEMP &&
            next->arg1.val.tempNum == curr->result.val.tempNum) {
            
            // Check if this temp is used anywhere else
            int tempNum = curr->result.val.tempNum;
            int usedElsewhere = 0;
            
            TACInstr *check = next->next;
            while (check) {
                if ((check->result.type == OPERAND_TEMP && check->result.val.tempNum == tempNum) ||
                    (check->arg1.type == OPERAND_TEMP && check->arg1.val.tempNum == tempNum) ||
                    (check->arg2.type == OPERAND_TEMP && check->arg2.val.tempNum == tempNum)) {
                    usedElsewhere = 1;
                    break;
                }
                check = check->next;
            }
            
            // If temp is only used once, merge the operations
            if (!usedElsewhere) {
                // Change LOAD_INT to directly target the variable
                curr->result = next->result;
                
                // Remove the COPY instruction
                curr->next = next->next;
                free(next);
                
                continue; // Don't advance curr, check for more patterns
            }
        }
        
        curr = curr->next;
    }
}
//Optimizer 2
static void optimize_arithmetic_assignments(TACProgram *prog) {
    TACInstr *curr = prog->head;
    
    while (curr && curr->next) {
        TACInstr *next = curr->next;
        
        // Pattern: Arithmetic op into temp, immediately COPY temp to var
        if ((curr->op == TAC_ADD || curr->op == TAC_SUB || 
            curr->op == TAC_MUL || curr->op == TAC_DIV) &&
            curr->result.type == OPERAND_TEMP &&
            next->op == TAC_COPY &&
            next->arg1.type == OPERAND_TEMP &&
            next->arg1.val.tempNum == curr->result.val.tempNum) {
            
            // Check if this temp is used anywhere else
            int tempNum = curr->result.val.tempNum;
            int usedElsewhere = 0;
            
            TACInstr *check = next->next;
            while (check) {
                if ((check->result.type == OPERAND_TEMP && check->result.val.tempNum == tempNum) ||
                    (check->arg1.type == OPERAND_TEMP && check->arg1.val.tempNum == tempNum) ||
                    (check->arg2.type == OPERAND_TEMP && check->arg2.val.tempNum == tempNum)) {
                    usedElsewhere = 1;
                    break;
                }
                check = check->next;
            }
            
            // If temp is only used once, merge the operations
            if (!usedElsewhere) {
                // Change arithmetic result to directly target the variable
                curr->result = next->result;
                
                // Remove the COPY instruction
                curr->next = next->next;
                free(next);
                
                continue;
            }
        }
        
        curr = curr->next;
    }
}


TACProgram *tac_generate(ASTNode *ast) {
    TACProgram *prog = tac_create_program();
    tac_gen_stmt(prog, ast);

    optimize_simple_assignments(prog);
    optimize_arithmetic_assignments(prog); 

    return prog;
}

//handles printing with char type support
static void print_operand_value(TACOperand op, int *tempValues, TACProgram *prog, int isCharContext) {
    if (op.isCharType) {
        isCharContext = 1;
    }


    switch (op.type) {
        case OPERAND_INT:
            if (isCharContext) {
                printf("%c", op.val.intVal);
            } else {
                printf("%d", op.val.intVal);
            }
            break;
        
        case OPERAND_STR:  
            printf("%s", op.val.strVal);
            break;
            
        case OPERAND_TEMP: {
            int value = tempValues[op.val.tempNum];
            
            // Find the instruction that produced this temp to check its type
            TACInstr *instr = prog->head;
            int shouldPrintAsChar = 0;
            
            while (instr) {
                if (instr->result.type == OPERAND_TEMP && 
                    instr->result.val.tempNum == op.val.tempNum) {
                    shouldPrintAsChar = instr->resultIsChar;
                    break;
                }
                instr = instr->next;
            }
            
            if (shouldPrintAsChar) {
                printf("%c", value);
            } else {
                printf("%d", value);
            }
            break;
        }
            
        case OPERAND_VAR: {
            Symbol *s = lookup(op.val.varName);
            if (s) {
                if (s->type == TYPE_CHR || (s->type == TYPE_FLEX && s->flexType == FLEX_CHAR)) {
                    printf("%c", s->chrVal);
                } else {
                    printf("%d", s->numVal);
                }
            }
            break;
        }
        default:
            break;
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
static void print_operand(TACOperand op) {
    switch (op.type) {
        case OPERAND_NONE: break;
        case OPERAND_TEMP: printf("t%d", op.val.tempNum); break;
        case OPERAND_VAR:  printf("%s", op.val.varName); break;
        case OPERAND_INT:  printf("%d", op.val.intVal); break;
        case OPERAND_STR:  printf("\"%s\"", op.val.strVal); break;
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
                /* FIX: For chr type, return ASCII value as integer for arithmetic */
                if (s->type == TYPE_CHR) {
                    return (int)s->chrVal;
                }
                /* For flex, check runtime type */
                else if (s->type == TYPE_FLEX) {
                    if (s->flexType == FLEX_CHAR) {
                        return (int)s->chrVal;
                    } else {
                        return s->numVal;
                    }
                }
                /* For nmbr */
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


int tac_execute(TACProgram *prog) {
    if (!prog || !prog->head) return 1;
    
    /* Allocate temporary storage for all temps */
    int *tempValues = calloc(prog->tempCount, sizeof(int));
    if (!tempValues) {
        fprintf(stderr, "Failed to allocate temp storage\n");
        return 1;
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
                    return 1;
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
                // Check if arg1 is a char type
                int isCharContext = 0;
                if (instr->arg1.type == OPERAND_TEMP) {
                    // Find the instruction that created this temp to check if it's char
                    TACInstr *check = prog->head;
                    while (check) {
                        if (check->result.type == OPERAND_TEMP && 
                            check->result.val.tempNum == instr->arg1.val.tempNum) {
                            isCharContext = check->resultIsChar;
                            break;
                        }
                        check = check->next;
                    }
                } else if (instr->arg1.type == OPERAND_VAR) {
                    Symbol *s = lookup(instr->arg1.val.varName);
                    if (s && (s->type == TYPE_CHR || (s->type == TYPE_FLEX && s->flexType == FLEX_CHAR))) {
                        isCharContext = 1;
                    }
                } else {
                    // Use the isCharType flag on the operand itself
                    isCharContext = instr->arg1.isCharType;
                }
                
                print_operand_value(instr->arg1, tempValues, prog, isCharContext);
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
    return 0;
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
    
    // r2-r11 mapping (registers 2-11)
    if (reg[0] == '$' && reg[1] == 'r') {
        int num = atoi(reg + 2);
        if (num >= 2 && num <= 11) {
            return num;  // Direct mapping: r2=2, r3=3, ..., r11=11
        }
    }
    
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

/* Optimize consecutive LOAD_INT -> COPY sequences */

/* Generate EduMIPS64 assembly and binary code */
/* Improved assembly generation with register allocation */
void tac_generate_assembly(TACProgram *prog) {
    char assembly_output[99999] = "";
    char hex_output[99999] = "";
    char binary_output[99999] = "";
    
    strcat(assembly_output, ".data\n\n.code\n\n");
    
    int tempStorageOffset = 1000;
    
    init_register_state();
    
    for (TACInstr *instr = prog->head; instr; instr = instr->next) {
        if (instr->inShwContext) {
            continue;
        }
        
        char asm_line[256] = "";
        
        switch (instr->op) {
            case TAC_LOAD_INT: {
                if (instr->result.type == OPERAND_VAR) {
                    // Load directly to variable in memory
                    Symbol *s = lookup(instr->result.val.varName);
                    if (s) {
                        snprintf(asm_line, sizeof(asm_line), "daddiu r2, r0, %d\n", instr->arg1.val.intVal);
                        strcat(assembly_output, asm_line);
                        
                        uint32_t mc = encode_i_format(OPCODE_DADDIU, 0, 2, (int16_t)instr->arg1.val.intVal);
                        char hex_str[32];
                        sprintf(hex_str, "0x%08X\n", mc);
                        strcat(hex_output, hex_str);
                        
                        char bin_str[40];
                        for (int i = 31; i >= 0; i--) {
                            sprintf(bin_str + (31 - i), "%d", (mc >> i) & 1);
                        }
                        strcat(binary_output, bin_str);
                        strcat(binary_output, "\n");
                        
                        snprintf(asm_line, sizeof(asm_line), "sd r2, %d(r0)\n", s->memOffset);
                        strcat(assembly_output, asm_line);
                        
                        mc = encode_i_format(OPCODE_SD, 0, 2, (int16_t)s->memOffset);
                        sprintf(hex_str, "0x%08X\n", mc);
                        strcat(hex_output, hex_str);
                        
                        for (int i = 31; i >= 0; i--) {
                            sprintf(bin_str + (31 - i), "%d", (mc >> i) & 1);
                        }
                        strcat(binary_output, bin_str);
                        strcat(binary_output, "\n");
                    }
                } else if (instr->result.type == OPERAND_TEMP) {
                    // Load immediate into register
                    int regIdx = allocate_register_for_temp(instr, instr->result.val.tempNum, 
                                                           assembly_output, hex_output, binary_output, 
                                                           tempStorageOffset);
                    const char *regName = get_reg_name(regIdx);
                    
                    snprintf(asm_line, sizeof(asm_line), "daddiu %s, r0, %d\n", regName, instr->arg1.val.intVal);
                    strcat(assembly_output, asm_line);
                    
                    int rt = get_register_number(regName);
                    uint32_t mc = encode_i_format(OPCODE_DADDIU, 0, rt, (int16_t)instr->arg1.val.intVal);
                    
                    char hex_str[32];
                    sprintf(hex_str, "0x%08X\n", mc);
                    strcat(hex_output, hex_str);
                    
                    char bin_str[40];
                    for (int i = 31; i >= 0; i--) {
                        sprintf(bin_str + (31 - i), "%d", (mc >> i) & 1);
                    }
                    strcat(binary_output, bin_str);
                    strcat(binary_output, "\n");
                    
                    regState[regIdx].isDirty = 1;
                    
                    // Only store if this temp is used later or if it's the last instruction
                    if (!temp_is_used_later(instr, instr->result.val.tempNum) || !instr->next) {
                        // Will be stored at end if needed
                    }
                }
                break;
            }
            
            case TAC_ADD:
            case TAC_SUB:
            case TAC_MUL:
            case TAC_DIV: {
                // Load left operand into register
                int leftReg = load_operand(instr->arg1, instr, assembly_output, hex_output, binary_output, tempStorageOffset);
                
                // Mark left register as "in use" temporarily to prevent it from being evicted
                int savedLeftTemp = regState[leftReg].tempNum;
                int savedLeftDirty = regState[leftReg].isDirty;
                
                // Load right operand (this won't evict leftReg now)
                int rightReg = load_operand_ex(instr->arg2, instr, assembly_output, hex_output, binary_output, tempStorageOffset, leftReg);
                
                // If both operands ended up in the same register (shouldn't happen but just in case)
                if (leftReg == rightReg && instr->arg1.type != instr->arg2.type) {
                    // Move one to a different register
                    int newRightReg = find_register_to_evict(instr);
                    if (newRightReg == leftReg) {
                        // Find another register
                        for (int i = 0; i < NUM_WORK_REGS; i++) {
                            if (i != leftReg) {
                                newRightReg = i;
                                break;
                            }
                        }
                    }
                    spill_register(newRightReg, assembly_output, hex_output, binary_output, tempStorageOffset);
                    
                    const char *srcRegName = get_reg_name(rightReg);
                    const char *destRegName = get_reg_name(newRightReg);
                    
                    char move_line[256];
                    snprintf(move_line, sizeof(move_line), "daddu %s, %s, r0  ; move to avoid conflict\n", destRegName, srcRegName);
                    strcat(assembly_output, move_line);
                    
                    int rs = get_register_number(srcRegName);
                    int rd = get_register_number(destRegName);
                    uint32_t mc = encode_r_format(FUNCT_DADDU, rs, 0, rd, 0);
                    
                    char hex_str[32];
                    sprintf(hex_str, "0x%08X\n", mc);
                    strcat(hex_output, hex_str);
                    
                    char bin_str[40];
                    for (int i = 31; i >= 0; i--) {
                        sprintf(bin_str + (31 - i), "%d", (mc >> i) & 1);
                    }
                    strcat(binary_output, bin_str);
                    strcat(binary_output, "\n");
                    
                    rightReg = newRightReg;
                }
                
                // Allocate destination register
                int destReg;
                if (instr->result.type == OPERAND_TEMP) {
                    // OPTIMIZATION: Try to reuse one of the source registers if possible
                    // This is safe if the source temp is dead after this instruction
                    
                    // Check if left operand is a temp that dies here
                    if (instr->arg1.type == OPERAND_TEMP && 
                        find_next_use_distance(instr, instr->arg1.val.tempNum) == 9999) {
                        destReg = leftReg;  // Reuse left register!
                        regState[destReg].tempNum = instr->result.val.tempNum;
                        regState[destReg].lastUseDistance = find_next_use_distance(instr, instr->result.val.tempNum);
                    }
                    // Check if right operand is a temp that dies here
                    else if (instr->arg2.type == OPERAND_TEMP && 
                            find_next_use_distance(instr, instr->arg2.val.tempNum) == 9999) {
                        destReg = rightReg;  // Reuse right register!
                        regState[destReg].tempNum = instr->result.val.tempNum;
                        regState[destReg].lastUseDistance = find_next_use_distance(instr, instr->result.val.tempNum);
                    }
                    else {
                        // Need a new register
                        destReg = allocate_register_for_temp(instr, instr->result.val.tempNum,
                                                            assembly_output, hex_output, binary_output,
                                                            tempStorageOffset);
                    }
                } else {
                    // Result is a variable, use any register
                    destReg = find_register_to_evict(instr);
                    if (destReg == leftReg || destReg == rightReg) {
                        for (int i = 0; i < NUM_WORK_REGS; i++) {
                            if (i != leftReg && i != rightReg) {
                                destReg = i;
                                break;
                            }
                        }
                    }
                }
                                
                const char *leftRegName = get_reg_name(leftReg);
                const char *rightRegName = get_reg_name(rightReg);
                const char *destRegName = get_reg_name(destReg);
                
                // Perform operation
                const char *opName;
                int funct;
                int needsMflo = 0;
                
                switch (instr->op) {
                    case TAC_ADD: opName = "daddu"; funct = FUNCT_DADDU; break;
                    case TAC_SUB: opName = "dsubu"; funct = FUNCT_DSUBU; break;
                    case TAC_MUL: opName = "dmult"; funct = FUNCT_DMULT; needsMflo = 1; break;
                    case TAC_DIV: opName = "ddiv"; funct = FUNCT_DDIV; needsMflo = 1; break;
                    default: opName = "daddu"; funct = FUNCT_DADDU;
                }
                
                if (needsMflo) {
                    snprintf(asm_line, sizeof(asm_line), "%s %s, %s\n", opName, leftRegName, rightRegName);
                } else {
                    snprintf(asm_line, sizeof(asm_line), "%s %s, %s, %s\n", opName, destRegName, leftRegName, rightRegName);
                }
                strcat(assembly_output, asm_line);
                
                int rs = get_register_number(leftRegName);
                int rt = get_register_number(rightRegName);
                int rd = get_register_number(destRegName);
                uint32_t mc;
                
                if (needsMflo) {
                    mc = encode_r_format(funct, rs, rt, 0, 0);
                } else {
                    mc = encode_r_format(funct, rs, rt, rd, 0);
                }
                
                char hex_str[32];
                sprintf(hex_str, "0x%08X\n", mc);
                strcat(hex_output, hex_str);
                
                char bin_str[40];
                for (int i = 31; i >= 0; i--) {
                    sprintf(bin_str + (31 - i), "%d", (mc >> i) & 1);
                }
                strcat(binary_output, bin_str);
                strcat(binary_output, "\n");
                
                if (needsMflo) {
                    snprintf(asm_line, sizeof(asm_line), "mflo %s\n", destRegName);
                    strcat(assembly_output, asm_line);
                    
                    mc = encode_r_format(FUNCT_MFLO, 0, 0, rd, 0);
                    sprintf(hex_str, "0x%08X\n", mc);
                    strcat(hex_output, hex_str);
                    
                    for (int i = 31; i >= 0; i--) {
                        sprintf(bin_str + (31 - i), "%d", (mc >> i) & 1);
                    }
                    strcat(binary_output, bin_str);
                    strcat(binary_output, "\n");
                }
                
                // Mark result as dirty
                regState[destReg].isDirty = 1;
                
                // ALWAYS store result if it's a variable
                if (instr->result.type == OPERAND_VAR) {
                    Symbol *s = lookup(instr->result.val.varName);
                    if (s) {
                        snprintf(asm_line, sizeof(asm_line), "sd %s, %d(r0)\n", destRegName, s->memOffset);
                        strcat(assembly_output, asm_line);
                        
                        mc = encode_i_format(OPCODE_SD, 0, rd, (int16_t)s->memOffset);
                        sprintf(hex_str, "0x%08X\n", mc);
                        strcat(hex_output, hex_str);
                        
                        for (int i = 31; i >= 0; i--) {
                            sprintf(bin_str + (31 - i), "%d", (mc >> i) & 1);
                        }
                        strcat(binary_output, bin_str);
                        strcat(binary_output, "\n");
                        
                        regState[destReg].isDirty = 0; // No longer dirty after storing
                    }
                }
                
                break;
            }
            
            case TAC_COPY: {
                // Similar pattern - load source, store to destination
                int srcReg = load_operand(instr->arg1, instr, assembly_output, hex_output, binary_output, tempStorageOffset);
                
                if (instr->result.type == OPERAND_VAR) {
                    Symbol *s = lookup(instr->result.val.varName);
                    if (s) {
                        const char *regName = get_reg_name(srcReg);
                        snprintf(asm_line, sizeof(asm_line), "sd %s, %d(r0)\n", regName, s->memOffset);
                        strcat(assembly_output, asm_line);
                        
                        int rt = get_register_number(regName);
                        uint32_t mc = encode_i_format(OPCODE_SD, 0, rt, (int16_t)s->memOffset);
                        
                        char hex_str[32];
                        sprintf(hex_str, "0x%08X\n", mc);
                        strcat(hex_output, hex_str);
                        
                        char bin_str[40];
                        for (int i = 31; i >= 0; i--) {
                            sprintf(bin_str + (31 - i), "%d", (mc >> i) & 1);
                        }
                        strcat(binary_output, bin_str);
                        strcat(binary_output, "\n");
                    }
                } else if (instr->result.type == OPERAND_TEMP) {
                    int destReg = allocate_register_for_temp(instr, instr->result.val.tempNum,
                                                            assembly_output, hex_output, binary_output,
                                                            tempStorageOffset);
                    
                    if (srcReg != destReg) {
                        const char *srcRegName = get_reg_name(srcReg);
                        const char *destRegName = get_reg_name(destReg);
                        
                        snprintf(asm_line, sizeof(asm_line), "daddu %s, %s, r0\n", destRegName, srcRegName);
                        strcat(assembly_output, asm_line);
                        
                        int rs = get_register_number(srcRegName);
                        int rd = get_register_number(destRegName);
                        uint32_t mc = encode_r_format(FUNCT_DADDU, rs, 0, rd, 0);
                        
                        char hex_str[32];
                        sprintf(hex_str, "0x%08X\n", mc);
                        strcat(hex_output, hex_str);
                        
                        char bin_str[40];
                        for (int i = 31; i >= 0; i--) {
                            sprintf(bin_str + (31 - i), "%d", (mc >> i) & 1);
                        }
                        strcat(binary_output, bin_str);
                        strcat(binary_output, "\n");
                    }
                    
                    regState[destReg].isDirty = 1;
                }
                break;
            }
            
            default:
                break;
        }
    }
    

    
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