#ifndef AST_H
#define AST_H

#include "symbol_table.h"


typedef struct ASTNode ASTNode;

typedef enum {
    NODE_PROGRAM,
    NODE_STMT_LIST,
    NODE_NUM_LIT,
    NODE_CHR_LIT,
    NODE_STR_LIT,
    NODE_IDENT,
    NODE_BINOP,
    NODE_DECL,
    NODE_DECL_LIST,
    NODE_ASSIGN,
    NODE_COMPOUND_ASSIGN,
    NODE_SHW,
    NODE_CONCAT,
    NODE_TYPE_DECL_LIST,  
    NODE_NAME_LIST,       
    NODE_NAME_ITEM         
} NodeType;

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_ASSIGN,
    OP_PLUS_ASSIGN,
    OP_MINUS_ASSIGN,
    OP_MULT_ASSIGN,
    OP_DIV_ASSIGN,
} OpType;

/* NOW define the actual struct */
struct ASTNode {
    NodeType type;
    int line;
    union {
        int numVal;
        char chrVal;
        char *strVal;
        
        struct {
            OpType op;
            ASTNode *left;
            ASTNode *right;
        } binop;
        
        struct {
            VarType varType;
            char *varName;
            ASTNode *initExpr;
        } decl;
        
        struct {
            ASTNode *left;
            ASTNode *right;
        } declList;
        
        struct {
            char *varName;
            OpType op;
            ASTNode *expr;
        } assign;
        
        struct {
            ASTNode *left;
            ASTNode *right;
        } shw;
        
        struct {
            ASTNode **stmts;
            int count;
            int capacity;
        } stmtList;
        
        struct {
            VarType varType;
            ASTNode *nameList;
        } typeDeclList;
        
        struct {
            ASTNode *left;
            ASTNode *right;
        } nameList;
        
        struct {
            char *name;
            ASTNode *initExpr;
            VarType varType;  
        } nameItem;
    } data;
};

/* Function declarations */
ASTNode *ast_create_program(void);
ASTNode *ast_add_stmt(ASTNode *program, ASTNode *stmt);

ASTNode *ast_create_num_lit(int val, int line);
ASTNode *ast_create_chr_lit(char val, int line);
ASTNode *ast_create_str_lit(const char *val, int line);
ASTNode *ast_create_ident(const char *name, int line);

ASTNode *ast_create_binop(OpType op, ASTNode *left, ASTNode *right, int line);
ASTNode *ast_create_decl(VarType type, const char *name, ASTNode *init, int line);
ASTNode *ast_create_decl_list(ASTNode *left, ASTNode *right, int line);
ASTNode *ast_create_assign(const char *name, ASTNode *expr, int line);
ASTNode *ast_create_compound_assign(const char *name, OpType op, ASTNode *expr, int line);
ASTNode *ast_create_shw(ASTNode *expr, int line);
ASTNode *ast_create_concat(ASTNode *left, ASTNode *right, int line);
ASTNode *ast_create_stmt_list(ASTNode *left, ASTNode *right, int line);

/* New declaration functions */
ASTNode *ast_create_type_decl_list(VarType type, ASTNode *nameList, int line);
ASTNode *ast_create_name_list(ASTNode *left, ASTNode *right, int line);
ASTNode *ast_create_name_item(const char *name, ASTNode *init, int line);
ASTNode *ast_create_name_item_typed(const char *name, ASTNode *init, VarType type, int line);


void ast_build_symbol_table(ASTNode *node, int *error_count);    
void ast_print(ASTNode *node, int indent);
int ast_check_semantics(ASTNode *node, int *error_count);
void ast_free(ASTNode *node);

#endif