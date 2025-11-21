#ifndef AST_H
#define AST_H

typedef enum
{
    NODE_PROGRAM,
    NODE_STMT_LIST,
    NODE_DECL,
    NODE_INIT,
    NODE_ASSIGN,
    NODE_SHW,
    NODE_EXPR_STMT,
    NODE_BINOP,
    NODE_UNOP,
    NODE_NUMBER,
    NODE_CHAR,
    NODE_STRING,
    NODE_IDENT
} NodeKind;

typedef struct AST
{
    NodeKind kind;
    struct AST *left;
    struct AST *right;
    int op;
    int intval;
    char chval;
    char *strval;
} AST;

#endif