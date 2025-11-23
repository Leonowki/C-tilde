%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<stdbool.h>
#include "symbol_table.h"
#include "ast.h"
#include "tac.h"

extern int yylex();
extern int yylineno;
extern char *yytext;

void yyerror(const char *s);

int lineCount = 1;
ASTNode *root = NULL;
int error_count = 0;
bool DEBUG_MODE = true;

%}

%union {
    int num;
    char ch;
    char *str;
    struct ASTNode *node;
}

/* Keywords */
%token TOK_NMBR
%token TOK_CHR
%token TOK_FLEX
%token TOK_SHW

/* Operators */
%token TOK_PLUS
%token TOK_MINUS
%token TOK_MULT
%token TOK_DIV

%token TOK_PLUS_ASSIGN
%token TOK_MINUS_ASSIGN
%token TOK_MULT_ASSIGN
%token TOK_DIV_ASSIGN
%token TOK_ASSIGN
%token TOK_CONCAT

/* Delimiters */
%token TOK_LPAREN
%token TOK_RPAREN
%token TOK_NEWLINE
%token TOK_COMMA




/* Literals */
%token <num> TOK_NUMBER_LITERAL
%token <ch>  TOK_CHAR_LITERAL
%token <str> TOK_STRING_LITERAL
%token <str> TOK_IDENTIFIER

/* Non-terminal types */
%type <node> program line_list line statement 
%type <node> declaration decl_list decl_item
%type <node> assignment compound_assign shw_statement
%type <node> shw_expr shw_item expr term factor

%%

program:
        line_list { 
            root = $1; 
            $$ = $1;
        }
    | /* empty */ { 
            root = ast_create_program(); 
            $$ = root; 
        }
    ;

line_list:
        line_list line {
            $$ = ast_add_stmt($1, $2);
            lineCount++;
        }
        | line {
            $$ = ast_create_program();
            $$ = ast_add_stmt($$, $1);
            lineCount++;
        }
        ;

line:
        statement TOK_NEWLINE   { $$ = $1; }
        | TOK_NEWLINE             { $$ = NULL; }
        | error TOK_NEWLINE       { error_count++; yyerrok; $$ = NULL; }
        ;

statement:
        declaration       { $$ = $1; }
        | assignment        { $$ = $1; }
        | compound_assign   { $$ = $1; }
        | shw_statement     { $$ = $1; }
        ;

/* Multi-declaration support */
declaration:
        decl_list { $$ = $1; }
        ;

decl_list:
        decl_list TOK_COMMA decl_item {
            /* Create a declaration list node that holds multiple declarations */
            $$ = ast_create_decl_list($1, $3, lineCount);
        }
        | decl_item { $$ = $1; }
        ;

decl_item:
        /* Initialized declarations */
        TOK_NMBR TOK_IDENTIFIER TOK_ASSIGN expr {
            Symbol *s = insert($2, TYPE_NMBR);
            $$ = ast_create_decl(TYPE_NMBR, $2, $4, lineCount);
        }
        | TOK_CHR TOK_IDENTIFIER TOK_ASSIGN TOK_CHAR_LITERAL {
            Symbol *s = insert($2, TYPE_CHR);
            ASTNode *init = ast_create_chr_lit($4, lineCount);
            $$ = ast_create_decl(TYPE_CHR, $2, init, lineCount);
        }
        | TOK_FLEX TOK_IDENTIFIER TOK_ASSIGN expr {
            Symbol *s = insert($2, TYPE_FLEX);
            $$ = ast_create_decl(TYPE_FLEX, $2, $4, lineCount);
        }
        | TOK_FLEX TOK_IDENTIFIER TOK_ASSIGN TOK_CHAR_LITERAL {
            Symbol *s = insert($2, TYPE_FLEX);
            ASTNode *init = ast_create_chr_lit($4, lineCount);
            $$ = ast_create_decl(TYPE_FLEX, $2, init, lineCount);
        }
        /* Uninitialized declarations with default values */
        | TOK_NMBR TOK_IDENTIFIER {
            Symbol *s = insert($2, TYPE_NMBR);
            ASTNode *init = ast_create_num_lit(0, lineCount);
            $$ = ast_create_decl(TYPE_NMBR, $2, init, lineCount);
        }
        | TOK_CHR TOK_IDENTIFIER {
            Symbol *s = insert($2, TYPE_CHR);
            ASTNode *init = ast_create_chr_lit('\0', lineCount);
            $$ = ast_create_decl(TYPE_CHR, $2, init, lineCount);
        }
        | TOK_FLEX TOK_IDENTIFIER {
            Symbol *s = insert($2, TYPE_FLEX);
            ASTNode *init = ast_create_str_lit("", lineCount);
            $$ = ast_create_decl(TYPE_FLEX, $2, init, lineCount);
        }
        ;

assignment:
        TOK_IDENTIFIER TOK_ASSIGN expr {
            Symbol *s = lookup($1);
            if (!s) {
                    error_count++;
                    fprintf(stderr, "LINE %d: Undefined variable '%s'\n", lineCount, $1);
            }
            $$ = ast_create_assign($1, $3, lineCount);
        }
        ;

compound_assign:
        TOK_IDENTIFIER TOK_PLUS_ASSIGN expr {
            Symbol *s = lookup($1);
            if (!s) {
                fprintf(stderr, "LINE %d: Undefined variable '%s'\n", lineCount, $1);
                error_count++;
            }
            $$ = ast_create_compound_assign($1, OP_PLUS_ASSIGN, $3, lineCount);
        }
        | TOK_IDENTIFIER TOK_MINUS_ASSIGN expr {
            Symbol *s = lookup($1);
            if (!s) {
                fprintf(stderr, "LINE %d: Undefined variable '%s'\n", lineCount, $1);
                error_count++;
            }
            $$ = ast_create_compound_assign($1, OP_MINUS_ASSIGN, $3, lineCount);
        }
        | TOK_IDENTIFIER TOK_MULT_ASSIGN expr {
            Symbol *s = lookup($1);
            if (!s) {
                fprintf(stderr, "LINE %d: Undefined variable '%s'\n", lineCount, $1);
                error_count++;
            }
            $$ = ast_create_compound_assign($1, OP_MULT_ASSIGN, $3, lineCount);
        }
        | TOK_IDENTIFIER TOK_DIV_ASSIGN expr {
            Symbol *s = lookup($1);
            if (!s) {
                fprintf(stderr, "LINE %d: Undefined variable '%s'\n", lineCount, $1);
                error_count++;
            }
            $$ = ast_create_compound_assign($1, OP_DIV_ASSIGN, $3, lineCount);
        }
    ;

shw_statement:
        TOK_SHW shw_expr {
            $$ = ast_create_shw($2, lineCount);
        }
        ;

shw_expr:
        shw_item                      { $$ = $1; }
        | shw_expr TOK_CONCAT shw_item  {
            $$ = ast_create_concat($1, $3, lineCount);
        }
    ;

shw_item:
        TOK_STRING_LITERAL  { $$ = ast_create_str_lit($1, lineCount); }
        | TOK_IDENTIFIER      { $$ = ast_create_ident($1, lineCount); }
        | TOK_NUMBER_LITERAL  { $$ = ast_create_num_lit($1, lineCount); }
        ;

expr:
        expr TOK_PLUS term      { $$ = ast_create_binop(OP_ADD, $1, $3, lineCount); }
        | expr TOK_MINUS term     { $$ = ast_create_binop(OP_SUB, $1, $3, lineCount); }
        | term
    ;

term:
        term TOK_MULT factor    { $$ = ast_create_binop(OP_MUL, $1, $3, lineCount); }
        | term TOK_DIV factor     { $$ = ast_create_binop(OP_DIV, $1, $3, lineCount); }
        | factor
    ;

factor:
        TOK_NUMBER_LITERAL        { $$ = ast_create_num_lit($1, lineCount); }
        | TOK_IDENTIFIER            { $$ = ast_create_ident($1, lineCount); }
        | TOK_LPAREN expr TOK_RPAREN { $$ = $2; }
        | TOK_MINUS factor          {
            /* Unary minus: 0 - factor */
            ASTNode *zero = ast_create_num_lit(0, lineCount);
            $$ = ast_create_binop(OP_SUB, zero, $2, lineCount);
        }
        | TOK_PLUS factor           {
            /* Unary plus: just return factor */
            $$ = $2;
        }
    ;

%%


void yyerror(const char *s) {
    fprintf(stderr, "Error at line %d: %s\n", lineCount, s);
    error_count++;
}



int Semantic_analysis(){

    printf("\n=== Semantic Analysis ===\n\n");
            int semantic_errors = 0;
            if (ast_check_semantics(root, &semantic_errors)) {
                printf("Semantic analysis passed.\n");
            } else {
                printf("Semantic analysis failed with %d error(s).\n", semantic_errors);
                ast_free(root);
                printf("\n=== Compilation Failed ===\n");
                return 1;
            }
    return 0;//success

}

void print_symbol_table() {
    printf("\n=== Symbol Table after Execution ===\n\n");
    printf("%-15s %-10s %-10s %-10s %s\n", "Name", "Type", "Offset", "Size", "Value");
    printf("%-15s %-10s %-10s %-10s %s\n", "----", "----", "------", "----", "-----");
    
    for (int i = 0; i < symcount; i++) {
        Symbol *s = &symtab[i];
        
        // Print name, type, offset, and size
        printf("%-15s %-10s %-10d %-10d ", 
                s->name, 
                type_to_string(s->type),
                s->memOffset,
                s->size);
        
        // Print value
        if (s->type == TYPE_NMBR || (s->type == TYPE_FLEX && s->flexType == FLEX_NUMBER)) {
            printf("%d\n", s->numVal);
        } else if (s->type == TYPE_CHR || (s->type == TYPE_FLEX && s->flexType == FLEX_CHAR)) {
            // Check if character is printable, otherwise show it differently
            if (s->chrVal >= 32 && s->chrVal <= 126) {
                printf("'%c'\n", s->chrVal, (int)s->chrVal);
            } else if (s->chrVal == '\0') {
                printf("'\\0' (null)\n");
            } else {
                printf("(ASCII %d)\n", (int)s->chrVal);
            }
        } else {
            printf("(uninitialized)\n");
        }
    }
    printf("\n");
}
int main(int argc, char **argv) {
    int result = yyparse();

    if (result == 0 && root && error_count == 0) {
        printf("\n=== Abstract Syntax Tree ===\n\n");
        ast_print(root, 0);

        // Semantic Analysis
        if(Semantic_analysis()!=0){
            return 1; //exit on semantic error
        }

        //TAC Generation and Execution
        printf("\n=== Three-Address Code ===\n\n");
        TACProgram *tac = tac_generate(root);  
        tac_print(tac);
        tac_execute(tac); 

        //check symbol table values after execution
        if(DEBUG_MODE) {
            //Compute memory offsets for all variables
            compute_symbol_offsets();
            print_symbol_table();
        }

        // Assembly Code Generation
        printf("\n=== Generating Assembly Code ===\n\n");
        tac_generate_assembly(tac, "output.s");

        tac_free(tac);
        ast_free(root);
    }

    return 0;
}