#ifndef TAC_H
#define TAC_H

#include "ast.h"
#include "symbol_table.h"

/* TAC Operation Types */
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
    TAC_DECL
} TACOp;

/* Operand Types */
typedef enum {
    OPERAND_NONE,
    OPERAND_TEMP,
    OPERAND_VAR,
    OPERAND_INT,
    OPERAND_STR
} OperandType;

/* TAC Operand */
typedef struct {
    OperandType type;
    union {
        int tempNum;
        char *varName;
        int intVal;
        char *strVal;
    } val;
} TACOperand;

/* TAC Instruction */
typedef struct TACInstr {
    TACOp op;
    TACOperand result;
    TACOperand arg1;
    TACOperand arg2;
    int line;
    struct TACInstr *next;
} TACInstr;

/* TAC Program - linked list of instructions */
typedef struct {
    TACInstr *head;
    TACInstr *tail;
    int tempCount;
} TACProgram;

/* Constructor functions */
TACProgram *tac_create_program(void);
TACOperand tac_operand_none(void);
TACOperand tac_operand_temp(int num);
TACOperand tac_operand_var(const char *name);
TACOperand tac_operand_int(int val);
TACOperand tac_operand_str(const char *val);

/* Instruction emission */
TACInstr *tac_emit(TACProgram *prog, TACOp op, TACOperand res, TACOperand a1, TACOperand a2, int line);
int tac_new_temp(TACProgram *prog);

/* Code generation from AST */
TACProgram *tac_generate(ASTNode *ast);
TACOperand tac_gen_expr(TACProgram *prog, ASTNode *node);
void tac_gen_stmt(TACProgram *prog, ASTNode *node);

/* Utility functions */
void tac_print(TACProgram *prog);
void tac_free(TACProgram *prog);
const char *tac_op_to_string(TACOp op);


//execute tac
void tac_execute(TACProgram *prog);

//assembly code generation
void tac_generate_assembly(TACProgram *prog, const char *filename);

#endif