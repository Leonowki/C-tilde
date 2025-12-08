#ifndef TAC_H
#define TAC_H

#include "ast.h"

typedef enum {
    TAC_ADD,
    TAC_SUB,
    TAC_MUL,
    TAC_DIV,
    TAC_COPY,
    TAC_LOAD_INT,
    TAC_LOAD_STR,
    TAC_PRINT,
    TAC_CONCAT,
    TAC_DECL,
} TACOp;

typedef enum {
    OPERAND_NONE,
    OPERAND_TEMP,
    OPERAND_VAR,
    OPERAND_INT,
    OPERAND_STR,
} OperandType;

typedef struct {
    OperandType type;
    int isCharType;
    union {
        int tempNum;
        char *varName;
        int intVal;
        char *strVal;
    } val;
} TACOperand;

typedef struct TACInstr {
    TACOp op;
    TACOperand result;
    TACOperand arg1;
    TACOperand arg2;
    int line;
    int inShwContext;
    int resultIsChar;
    struct TACInstr *next;
} TACInstr;

typedef struct {
    TACInstr *head;
    TACInstr *tail;
    int tempCount;
} TACProgram;

/* Function declarations */
TACProgram *tac_create_program(void);
TACOperand tac_operand_none(void);
TACOperand tac_operand_temp(int num);
TACOperand tac_operand_var(const char *name);
TACOperand tac_operand_int(int val);
TACOperand tac_operand_str(const char *val);
int tac_new_temp(TACProgram *prog);
TACInstr *tac_emit(TACProgram *prog, TACOp op, TACOperand res, TACOperand a1, TACOperand a2, int line);
TACOperand tac_gen_expr(TACProgram *prog, ASTNode *node);
void tac_gen_stmt(TACProgram *prog, ASTNode *node);
TACProgram *tac_generate(ASTNode *ast);
void tac_print(TACProgram *prog);
int tac_execute(TACProgram *prog);
void tac_generate_assembly(TACProgram *prog);
void tac_free(TACProgram *prog);
const char *tac_op_to_string(TACOp op);


#endif