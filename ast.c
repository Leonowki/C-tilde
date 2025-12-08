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
    if (!left || !right) return NULL; 
    
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
    if (!left || !right) return NULL;  
    ASTNode *node = ast_alloc(NODE_CONCAT, line);
    node->data.shw.left = left;
    node->data.shw.right = right;
    return node;
}
ASTNode *ast_create_type_decl_list(VarType type, ASTNode *nameList, int line) {
    ASTNode *node = ast_alloc(NODE_TYPE_DECL_LIST, line);
    node->data.typeDeclList.varType = type;
    node->data.typeDeclList.nameList = nameList;
    return node;
}

ASTNode *ast_create_name_list(ASTNode *left, ASTNode *right, int line) {
    ASTNode *node = ast_alloc(NODE_NAME_LIST, line);
    node->data.nameList.left = left;
    node->data.nameList.right = right;
    return node;
}

ASTNode *ast_create_name_item(const char *name, ASTNode *init, int line) {
    ASTNode *node = ast_alloc(NODE_NAME_ITEM, line);
    node->data.nameItem.name = strdup(name);
    node->data.nameItem.initExpr = init;
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
/*CHECK SEMANTICS FOR COMMON SEMANTIC ERRORS*/
static int is_constant_zero(ASTNode *node) {
    if (!node) return 0;
    
    if (node->type == NODE_NUM_LIT && node->data.numVal == 0) {
        return 1;
    }

    if (node->type == NODE_BINOP && node->data.binop.op == OP_MUL) {
        /* 0 * anything = 0, anything * 0 = 0 */
        if (is_constant_zero(node->data.binop.left) || 
            is_constant_zero(node->data.binop.right)) {
            return 1;
        }
    }
    return 0;
}

/* Recursively check semantics of expressions */
static int check_expr_semantics(ASTNode *node, int *error_count) {
    if (!node) return 1;
    
    switch (node->type) {
        case NODE_NUM_LIT:
        case NODE_CHR_LIT:
        case NODE_STR_LIT:
            return 1;
            
        case NODE_IDENT: {
            /* Variable is already checked during parsing */
            Symbol *s = lookup(node->data.strVal);
            if (!s) {
                fprintf(stderr, "error at %d: Undefined variable '%s'\n", 
                        node->line, node->data.strVal);
                (*error_count)++;
                return 0;
            }
            return 1;
        }
        
        case NODE_BINOP: {
            int valid = 1;
            
            /* Check left and right operands first */
            valid &= check_expr_semantics(node->data.binop.left, error_count);
            valid &= check_expr_semantics(node->data.binop.right, error_count);
            
            /* Check for division by zero */
            if (node->data.binop.op == OP_DIV) {
                if (is_constant_zero(node->data.binop.right)) {
                    fprintf(stderr, "error %d: Division by zero detected\n", node->line);
                    (*error_count)++;
                    return 0;
                }
            }
            
            return valid;
        }
        
        case NODE_CONCAT:
            check_expr_semantics(node->data.shw.left, error_count);
            check_expr_semantics(node->data.shw.right, error_count);
            return 1;
            
        default:
            return 1;
    }
}

/*Main semantic checks*/
int ast_check_semantics(ASTNode *node, int *error_count) {
    if (!node) return 1;
    
    switch (node->type) {
        case NODE_PROGRAM:
            for (int i = 0; i < node->data.stmtList.count; i++) {
                ast_check_semantics(node->data.stmtList.stmts[i], error_count);
            }
            break;
            
        case NODE_DECL:
            if (node->data.decl.initExpr) {
                check_expr_semantics(node->data.decl.initExpr, error_count);
            }
            break;
            
        case NODE_DECL_LIST:
            ast_check_semantics(node->data.declList.left, error_count);
            ast_check_semantics(node->data.declList.right, error_count);
            break;
            
        case NODE_ASSIGN:
            check_expr_semantics(node->data.assign.expr, error_count);
            break;
            
        case NODE_COMPOUND_ASSIGN:
            check_expr_semantics(node->data.assign.expr, error_count);
            break;
            
        case NODE_SHW:
            check_expr_semantics(node->data.shw.left, error_count);
            break;
            
        default:
            break;
    }
    
    return (*error_count == 0);
}

/*Free AST*/
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