%{
/* C prologue - visible to parser and actions */
#include "ast.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Forward from lexer */
int yylex(void);
void yyerror(const char *s);


/* forward declaration */
static void print_ast(AST *n, int indent);

/* constructors */
static AST *ast_new(NodeKind kind) {
    AST *n = malloc(sizeof(AST));
    n->kind = kind;
    n->left = n->right = NULL;
    n->op = 0;
    n->intval = 0;
    n->chval = 0;
    n->strval = NULL;
    return n;
}

static AST *ast_number(int v) {
    AST *n = ast_new(NODE_NUMBER);
    n->intval = v;
    return n;
}
static AST *ast_char(char c) {
    AST *n = ast_new(NODE_CHAR);
    n->chval = c;
    return n;
}
static AST *ast_string(char *s) {
    AST *n = ast_new(NODE_STRING);
    n->strval = s;
    return n;
}
static AST *ast_ident(char *name) {
    AST *n = ast_new(NODE_IDENT);
    n->strval = name;
    return n;
}
static AST *ast_binop(int op, AST *left, AST *right) {
    AST *n = ast_new(NODE_BINOP);
    n->op = op;
    n->left = left;
    n->right = right;
    return n;
}
static AST *ast_unop(int op, AST *expr) {
    AST *n = ast_new(NODE_UNOP);
    n->op = op;
    n->left = expr;
    return n;
}

/* stmt constructors */
static AST *ast_decl(char *type_name, AST *inits) {
    AST *n = ast_new(NODE_DECL);
    n->strval = type_name;
    n->left = inits;
    return n;
}
static AST *ast_init(char *ident, AST *maybe_expr) {
    AST *n = ast_new(NODE_INIT);
    n->strval = ident;
    n->left = maybe_expr;
    return n;
}
static AST *ast_assign(char *ident, int op_token, AST *expr) {
    AST *n = ast_new(NODE_ASSIGN);
    n->strval = ident;
    n->op = op_token;
    n->left = expr;
    return n;
}
static AST *ast_shw(AST *args) {
    AST *n = ast_new(NODE_SHW);
    n->left = args;
    return n;
}
static AST *ast_stmt_list(AST *prev, AST *next) {
    if (!prev) return next;
    AST *t = prev;
    while (t->right) t = t->right;
    t->right = next;
    return prev;
}

/* top-level AST pointer for printing in main */
AST *root_ast = NULL;

%}


%union {
    int num;
    char ch;
    char *str;
    AST *ast;
}

/* token declarations - associate semantic types where needed */
%token TOK_TILDE TOK_COMMA TOK_LPAREN TOK_RPAREN
%token TOK_PLUS TOK_MINUS TOK_MULT TOK_DIV
%token TOK_ASSIGN TOK_PLUS_ASSIGN TOK_MINUS_ASSIGN TOK_MULT_ASSIGN TOK_DIV_ASSIGN
%token TOK_SHW TOK_NMBR TOK_CHR TOK_FLEX
%token <str> TOK_IDENT
%token <num> TOK_NUMBER_LITERAL
%token <ch>  TOK_CHAR_LITERAL
%token <str> TOK_STRING_LITERAL

/* nonterminal types */
%type <ast> program stmt_list stmt decl_stmt init_list init assign_stmt shw_stmt expr_stmt
%type <ast> expression additive_expression multiplicative_expression unary_expression primary_expression arg_list arg_list_opt


%%

/* --- grammar rules with actions building AST --- */

program:
    stmt_list {
        AST *p = ast_new(NODE_PROGRAM);
        p->left = $1;
        root_ast = p;
    }
    ;

stmt_list:
      /* empty */ { $$ = NULL; }
    | stmt_list stmt { $$ = ast_stmt_list($1, $2); }
    ;

stmt:
    decl_stmt TOK_TILDE { $$ = $1; }
    | assign_stmt TOK_TILDE { $$ = $1; }
    | shw_stmt TOK_TILDE { $$ = $1; }
    | expr_stmt TOK_TILDE { $$ = $1; }
    | TOK_TILDE { $$ = NULL; }
    ;

decl_stmt:
    TOK_NMBR init_list { $$ = ast_decl("nmbr", $2); }
    | TOK_CHR init_list  { $$ = ast_decl("chr", $2); }
    | TOK_FLEX init_list { $$ = ast_decl("flex", $2); }
    ;

init_list:
    init { $$ = $1; }
    | init_list TOK_COMMA init {
        AST *t = $1;
        while (t->right) t = t->right;
        t->right = $3;
        $$ = $1;
    }
    ;

init:
    TOK_IDENT {
        $$ = ast_init($1, NULL);
    }
    | TOK_IDENT TOK_ASSIGN expression {
        $$ = ast_init($1, $3);
    }
    ;

assign_stmt:
    TOK_IDENT TOK_ASSIGN expression {
        $$ = ast_assign($1, TOK_ASSIGN, $3);
    }
    | TOK_IDENT TOK_PLUS_ASSIGN expression {
        $$ = ast_assign($1, TOK_PLUS_ASSIGN, $3);
    }
    | TOK_IDENT TOK_MINUS_ASSIGN expression {
        $$ = ast_assign($1, TOK_MINUS_ASSIGN, $3);
    }
    | TOK_IDENT TOK_MULT_ASSIGN expression {
        $$ = ast_assign($1, TOK_MULT_ASSIGN, $3);
    }
    | TOK_IDENT TOK_DIV_ASSIGN expression {
        $$ = ast_assign($1, TOK_DIV_ASSIGN, $3);
    }
    ;

shw_stmt:
    TOK_SHW TOK_LPAREN arg_list_opt TOK_RPAREN {
        $$ = ast_shw($3);
    }
    ;

arg_list_opt:
      /* empty */ { $$ = NULL; }
    | arg_list { $$ = $1; }
    ;

arg_list:
    expression { $$ = $1; }
    | arg_list TOK_COMMA expression {
        AST *t = $1;
        while (t->right) t = t->right;
        t->right = $3;
        $$ = $1;
    }
    ;

expr_stmt:
    expression {
        AST *n = ast_new(NODE_EXPR_STMT);
        n->left = $1;
        $$ = n;
    }
    ;

expression:
    additive_expression { $$ = $1; }
    ;

additive_expression:
    multiplicative_expression { $$ = $1; }
    | additive_expression TOK_PLUS multiplicative_expression {
        $$ = ast_binop(TOK_PLUS, $1, $3);
    }
    | additive_expression TOK_MINUS multiplicative_expression {
        $$ = ast_binop(TOK_MINUS, $1, $3);
    }
    ;

multiplicative_expression:
    unary_expression { $$ = $1; }
    | multiplicative_expression TOK_MULT unary_expression {
        $$ = ast_binop(TOK_MULT, $1, $3);
    }
    | multiplicative_expression TOK_DIV unary_expression {
        $$ = ast_binop(TOK_DIV, $1, $3);
    }
    ;

unary_expression:
    primary_expression { $$ = $1; }
    | TOK_PLUS unary_expression {
        $$ = ast_unop(TOK_PLUS, $2);
    }
    | TOK_MINUS unary_expression {
        $$ = ast_unop(TOK_MINUS, $2);
    }
    ;

primary_expression:
    TOK_NUMBER_LITERAL { $$ = ast_number($1); }
    | TOK_CHAR_LITERAL { $$ = ast_char($1); }
    | TOK_STRING_LITERAL { $$ = ast_string($1); }
    | TOK_IDENT { $$ = ast_ident($1); }
    | TOK_LPAREN expression TOK_RPAREN { $$ = $2; }
    ;

%%

/* C code section - error function, printing, and main */

static void ast_print_indent(int indent) {
    int i;
    for (i=0;i<indent;i++) putchar(' ');
}

static const char *opname(int t) {
    switch(t) {
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_MULT: return "*";
        case TOK_DIV: return "/";
        case TOK_ASSIGN: return "=";
        case TOK_PLUS_ASSIGN: return "+=";
        case TOK_MINUS_ASSIGN: return "-=";
        case TOK_MULT_ASSIGN: return "*=";
        case TOK_DIV_ASSIGN: return "/=";
        default: return "op";
    }
}

static void print_ast(AST *n, int indent) {
    unsigned char uc;
    if (!n) return;
    ast_print_indent(indent);
    switch(n->kind) {
        case NODE_PROGRAM:
            printf("Program\n");
            print_ast(n->left, indent+2);
            break;
        case NODE_STMT_LIST:
            printf("StmtList\n");
            print_ast(n->left, indent+2);
            print_ast(n->right, indent+2);
            break;
        case NODE_DECL:
            printf("Decl type=%s\n", n->strval);
            print_ast(n->left, indent+2);
            break;
        case NODE_INIT:
            printf("Init ident=%s\n", n->strval);
            if (n->left) print_ast(n->left, indent+2);
            break;
        case NODE_ASSIGN:
            printf("Assign %s %s\n", n->strval, opname(n->op));
            print_ast(n->left, indent+2);
            break;
        case NODE_SHW:
            printf("Shw\n");
            print_ast(n->left, indent+2);
            break;
        case NODE_EXPR_STMT:
            printf("ExprStmt\n");
            print_ast(n->left, indent+2);
            break;
        case NODE_BINOP:
            printf("BinOp %s\n", opname(n->op));
            print_ast(n->left, indent+2);
            print_ast(n->right, indent+2);
            break;
        case NODE_UNOP:
            printf("UnOp %s\n", opname(n->op));
            print_ast(n->left, indent+2);
            break;
        case NODE_NUMBER:
            printf("Number %d\n", n->intval);
            break;
        case NODE_CHAR:
            uc = (unsigned char)n->chval;
            if (n->chval >= 32 && n->chval < 127) {
                printf("Char '%c' (0x%02x)\n", n->chval, uc);
            } else {
                printf("Char (0x%02x)\n", uc);
            }
            break;
        case NODE_STRING:
            printf("String \"%s\"\n", n->strval ? n->strval : "");
            break;
        case NODE_IDENT:
            printf("Ident %s\n", n->strval);
            break;
        default:
            printf("Unknown node\n");
    }
    if (n->right && n->kind != NODE_STMT_LIST) {
        print_ast(n->right, indent);
    }
}

void yyerror(const char *s) {
    fprintf(stderr, "Parse error: %s\n", s);
}

int main(void) {
    if (yyparse() == 0) {
        printf("Parse successful!\n\n");
        if (root_ast) {
            print_ast(root_ast, 0);
        } else {
            printf("(empty program)\n");
        }
        return 0;
    } else {
        printf("Parse failed.\n");
        return 1;
    }
}