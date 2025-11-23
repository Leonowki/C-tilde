#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tac.h"

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
            tac_gen_expr(prog, node->data.shw.left);
            tac_gen_expr(prog, node->data.shw.right);
            return tac_operand_none();
        }
        default:
            return tac_operand_none();
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
            // TACOperand val = tac_gen_expr(prog, node->data.shw.left);
            // tac_emit(prog, TAC_PRINT, tac_operand_none(), val, tac_operand_none(), node->line);
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
                return s->numVal;
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
                set_number(s, value);
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
            
            default:
                break;
        }
    }
    
    free(tempValues);
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