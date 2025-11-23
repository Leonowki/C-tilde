#ifndef AST_H
#define AST_H

#include "symbol_table.h"

/* AST Node Types */
typedef enum {
    /* Program structure */
    NODE_PROGRAM,
    NODE_STMT_LIST,
    NODE_DECL_LIST,    /* NEW: For multi-declarations */
    
    /* Statements */
    NODE_DECL,
    NODE_ASSIGN,
    NODE_COMPOUND_ASSIGN,
    NODE_SHW,
    
    /* Expressions */
    NODE_BINOP,
    NODE_NUM_LIT,
    NODE_CHR_LIT,
    NODE_STR_LIT,
    NODE_IDENT,
    NODE_CONCAT
} NodeType;

/* Binary operators */
typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_ASSIGN,
    OP_PLUS_ASSIGN,
    OP_MINUS_ASSIGN,
    OP_MULT_ASSIGN,
    OP_DIV_ASSIGN
} OpType;

/* AST Node structure */
typedef struct ASTNode {
    NodeType type;
    int line;
    
    union {
        /* For NODE_NUM_LIT */
        int numVal;
        
        /* For NODE_CHR_LIT */
        char chrVal;
        
        /* For NODE_STR_LIT, NODE_IDENT */
        char *strVal;
        
        /* For NODE_DECL */
        struct {
            VarType varType;
            char *varName;
            struct ASTNode *initExpr;
        } decl;
        
        /* For NODE_ASSIGN, NODE_COMPOUND_ASSIGN */
        struct {
            char *varName;
            OpType op;
            struct ASTNode *expr;
        } assign;
        
        /* For NODE_BINOP */
        struct {
            OpType op;
            struct ASTNode *left;
            struct ASTNode *right;
        } binop;
        
        /* For NODE_SHW, NODE_CONCAT */
        struct {
            struct ASTNode *left;
            struct ASTNode *right;
        } shw;
        
        /* For NODE_STMT_LIST, NODE_PROGRAM */
        struct {
            struct ASTNode **stmts;
            int count;
            int capacity;
        } stmtList;
        
        /* NEW: For NODE_DECL_LIST (multi-declarations) */
        struct {
            struct ASTNode *left;   /* Previous declarations */
            struct ASTNode *right;  /* Current declaration */
        } declList;
    } data;
} ASTNode;



/* Constructor functions */
ASTNode *ast_create_program(void);
ASTNode *ast_add_stmt(ASTNode *program, ASTNode *stmt);

ASTNode *ast_create_num_lit(int val, int line);
ASTNode *ast_create_chr_lit(char val, int line);
ASTNode *ast_create_str_lit(const char *val, int line);
ASTNode *ast_create_ident(const char *name, int line);

ASTNode *ast_create_binop(OpType op, ASTNode *left, ASTNode *right, int line);

ASTNode *ast_create_decl(VarType type, const char *name, ASTNode *init, int line);
ASTNode *ast_create_decl_list(ASTNode *left, ASTNode *right, int line);  /* NEW */
ASTNode *ast_create_assign(const char *name, ASTNode *expr, int line);
ASTNode *ast_create_compound_assign(const char *name, OpType op, ASTNode *expr, int line);

ASTNode *ast_create_shw(ASTNode *expr, int line);
ASTNode *ast_create_concat(ASTNode *left, ASTNode *right, int line);
ASTNode *ast_create_stmt_list(ASTNode *left, ASTNode *right, int line);

/* Utility functions */
void ast_print(ASTNode *node, int indent);
void ast_free(ASTNode *node);

int ast_check_semantics(ASTNode *node, int *error_count);


#endif