#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

static ASTNode *ast_alloc(NodeType type, int line) {
    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    memset(node, 0, sizeof(ASTNode));
    node->type = type;
    node->line = line;
    return node;
}

ASTNode *ast_create_program(void) {
    ASTNode *node = ast_alloc(NODE_PROGRAM, 0);
    node->data.stmtList.stmts = NULL;
    node->data.stmtList.count = 0;
    node->data.stmtList.capacity = 0;
    return node;
}

ASTNode *ast_add_stmt(ASTNode *program, ASTNode *stmt) {
    if (!stmt) return program;
    if (program->data.stmtList.count >= program->data.stmtList.capacity) {
        int newCap = program->data.stmtList.capacity == 0 ? 8 : program->data.stmtList.capacity * 2;
        program->data.stmtList.stmts = realloc(program->data.stmtList.stmts, newCap * sizeof(ASTNode *));
        program->data.stmtList.capacity = newCap;
    }
    program->data.stmtList.stmts[program->data.stmtList.count++] = stmt;
    return program;
}

ASTNode *ast_create_num_lit(int val, int line) {
    ASTNode *node = ast_alloc(NODE_NUM_LIT, line);
    node->data.numVal = val;
    return node;
}

ASTNode *ast_create_chr_lit(char val, int line) {
    ASTNode *node = ast_alloc(NODE_CHR_LIT, line);
    node->data.chrVal = val;
    return node;
}

ASTNode *ast_create_str_lit(const char *val, int line) {
    ASTNode *node = ast_alloc(NODE_STR_LIT, line);
    node->data.strVal = strdup(val);
    return node;
}

ASTNode *ast_create_ident(const char *name, int line) {
    ASTNode *node = ast_alloc(NODE_IDENT, line);
    node->data.strVal = strdup(name);
    return node;
}

ASTNode *ast_create_binop(OpType op, ASTNode *left, ASTNode *right, int line) {
    ASTNode *node = ast_alloc(NODE_BINOP, line);
    node->data.binop.op = op;
    node->data.binop.left = left;
    node->data.binop.right = right;
    return node;
}

ASTNode *ast_create_decl_list(ASTNode *left, ASTNode *right, int line) {
    ASTNode *node = ast_alloc(NODE_DECL_LIST, line);
    node->data.declList.left = left;
    node->data.declList.right = right;
    return node;
}

ASTNode *ast_create_decl(VarType type, const char *name, ASTNode *init, int line) {
    ASTNode *node = ast_alloc(NODE_DECL, line);
    node->data.decl.varType = type;
    node->data.decl.varName = strdup(name);
    node->data.decl.initExpr = init;
    return node;
}

ASTNode *ast_create_assign(const char *name, ASTNode *expr, int line) {
    ASTNode *node = ast_alloc(NODE_ASSIGN, line);
    node->data.assign.varName = strdup(name);
    node->data.assign.op = OP_ASSIGN;
    node->data.assign.expr = expr;
    return node;
}

ASTNode *ast_create_compound_assign(const char *name, OpType op, ASTNode *expr, int line) {
    ASTNode *node = ast_alloc(NODE_COMPOUND_ASSIGN, line);
    node->data.assign.varName = strdup(name);
    node->data.assign.op = op;
    node->data.assign.expr = expr;
    return node;
}

ASTNode *ast_create_shw(ASTNode *expr, int line) {
    ASTNode *node = ast_alloc(NODE_SHW, line);
    node->data.shw.left = expr;
    node->data.shw.right = NULL;
    return node;
}

ASTNode *ast_create_concat(ASTNode *left, ASTNode *right, int line) {
    ASTNode *node = ast_alloc(NODE_CONCAT, line);
    node->data.shw.left = left;
    node->data.shw.right = right;
    return node;
}

/* NOTE: This function is not used in the current CFG. 
   Remove it or fix it if you plan to use NODE_STMT_LIST */
ASTNode *ast_create_stmt_list(ASTNode *left, ASTNode *right, int line) {
    ASTNode *node = ast_alloc(NODE_STMT_LIST, line);
    node->data.stmtList.stmts = NULL;
    node->data.stmtList.count = 0;
    node->data.stmtList.capacity = 0;
    /* Add left and right as statements if needed */
    if (left) ast_add_stmt(node, left);
    if (right) ast_add_stmt(node, right);
    return node;
}

static const char *op_to_string(OpType op) {
    switch (op) {
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        case OP_ASSIGN: return ":";
        case OP_PLUS_ASSIGN: return "+:";
        case OP_MINUS_ASSIGN: return "-:";
        case OP_MULT_ASSIGN: return "*:";
        case OP_DIV_ASSIGN: return "/:";
        default: return "?";
    }
}

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

void ast_print(ASTNode *node, int indent) {
    if (!node) return;
    print_indent(indent);
    
    switch (node->type) {
        case NODE_PROGRAM:
            printf("Program\n");
            for (int i = 0; i < node->data.stmtList.count; i++)
                ast_print(node->data.stmtList.stmts[i], indent + 1);
            break;
        case NODE_STMT_LIST:
            printf("StmtList\n");
            for (int i = 0; i < node->data.stmtList.count; i++)
                ast_print(node->data.stmtList.stmts[i], indent + 1);
            break;
        case NODE_NUM_LIT:
            printf("NumLit(%d)\n", node->data.numVal);
            break;
        case NODE_CHR_LIT:
            printf("ChrLit('%c')\n", node->data.chrVal);
            break;
        case NODE_STR_LIT:
            printf("StrLit(\"%s\")\n", node->data.strVal);
            break;
        case NODE_IDENT:
            printf("Ident(%s)\n", node->data.strVal);
            break;
        case NODE_BINOP:
            printf("BinOp(%s)\n", op_to_string(node->data.binop.op));
            ast_print(node->data.binop.left, indent + 1);
            ast_print(node->data.binop.right, indent + 1);
            break;
        case NODE_DECL:
            printf("Decl(%s %s)\n", type_to_string(node->data.decl.varType), node->data.decl.varName);
            ast_print(node->data.decl.initExpr, indent + 1);
            break;
        case NODE_DECL_LIST:
            /* FIX: Removed duplicate print_indent call */
            printf("DeclList (line %d)\n", node->line);
            ast_print(node->data.declList.left, indent + 1);
            ast_print(node->data.declList.right, indent + 1);
            break;
        case NODE_ASSIGN:
            printf("Assign(%s)\n", node->data.assign.varName);
            ast_print(node->data.assign.expr, indent + 1);
            break;
        case NODE_COMPOUND_ASSIGN:
            printf("CompoundAssign(%s %s)\n", node->data.assign.varName, op_to_string(node->data.assign.op));
            ast_print(node->data.assign.expr, indent + 1);
            break;
        case NODE_SHW:
            printf("Shw\n");
            ast_print(node->data.shw.left, indent + 1);
            break;
        case NODE_CONCAT:
            printf("Concat\n");
            ast_print(node->data.shw.left, indent + 1);
            ast_print(node->data.shw.right, indent + 1);
            break;
        default:
            printf("Unknown node type %d\n", node->type);
    }
}

void ast_free(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case NODE_PROGRAM:
        case NODE_STMT_LIST:
            for (int i = 0; i < node->data.stmtList.count; i++)
                ast_free(node->data.stmtList.stmts[i]);
            free(node->data.stmtList.stmts);
            break;
        case NODE_STR_LIT:
        case NODE_IDENT:
            free(node->data.strVal);
            break;
        case NODE_BINOP:
            ast_free(node->data.binop.left);
            ast_free(node->data.binop.right);
            break;
        case NODE_DECL:
            free(node->data.decl.varName);
            ast_free(node->data.decl.initExpr);
            break;
        case NODE_DECL_LIST:
            ast_free(node->data.declList.left);
            ast_free(node->data.declList.right);
            break;
        case NODE_ASSIGN:
        case NODE_COMPOUND_ASSIGN:
            free(node->data.assign.varName);
            ast_free(node->data.assign.expr);
            break;
        case NODE_SHW:
            ast_free(node->data.shw.left);
            break;
        case NODE_CONCAT:
            ast_free(node->data.shw.left);
            ast_free(node->data.shw.right);
            break;
        case NODE_NUM_LIT:
        case NODE_CHR_LIT:
            break;
    }
    free(node);
}