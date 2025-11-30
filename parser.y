%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<stdbool.h>
#include "symbol_table.h"
#include "ast.h"
#include "tac.h"
#include <windows.h>


extern int yylex();
extern int yylineno;
extern char *yytext;

void yyerror(const char *s);
extern int yychar;

int lineCount = 1;
ASTNode *root = NULL;
int error_count = 0;
bool DEBUG_MODE = false;

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
    | line_list statement {  /* Allow final statement without newline at EOF */
            root = ast_add_stmt($1, $2);  /* ADD: Set root here too */
            $$ = root;
        }
    | statement {  /* Allow single statement without newline */
            root = ast_create_program();
            root = ast_add_stmt(root, $1);
            $$ = root;
        }
    | /* empty */ { 
            root = ast_create_program(); 
            $$ = root; 
        }
    ;


line_list:
        line_list line {
            if ($2 != NULL) {
                $$ = ast_add_stmt($1, $2);
            } else {
                $$ = $1;
            }
            lineCount++;
        }
        | line {
            $$ = ast_create_program();
            if ($1 != NULL) {
                $$ = ast_add_stmt($$, $1);
            }
            lineCount++;
        }
        ;
line:
        statement TOK_NEWLINE   { $$ = $1; }
        | TOK_NEWLINE           { $$ = NULL; }
        | error TOK_NEWLINE     { error_count++; yyerrok; $$ = NULL; }
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
        /* ===== Initialized declarations ===== */
        TOK_NMBR TOK_IDENTIFIER TOK_ASSIGN expr {
            Symbol *s = insert($2, TYPE_NMBR, lineCount, &error_count);
            if (!s) {
                $$ = NULL;  // Error already reported
            } else {
                $$ = ast_create_decl(TYPE_NMBR, $2, $4, lineCount);
            }
        }
        | TOK_CHR TOK_IDENTIFIER TOK_ASSIGN expr {
            Symbol *s = insert($2, TYPE_CHR, lineCount, &error_count);
            if (!s) {
                $$ = NULL;
            } else {
                $$ = ast_create_decl(TYPE_CHR, $2, $4, lineCount);
            }
        }
        | TOK_FLEX TOK_IDENTIFIER TOK_ASSIGN expr {
            Symbol *s = insert($2, TYPE_FLEX, lineCount, &error_count);
            if (!s) {
                $$ = NULL;
            } else {
                $$ = ast_create_decl(TYPE_FLEX, $2, $4, lineCount);
            }
        }
        
        /* ===== Uninitialized declarations ===== */
        | TOK_NMBR TOK_IDENTIFIER {
            Symbol *s = insert($2, TYPE_NMBR, lineCount, &error_count);
            if (!s) {
                $$ = NULL;
            } else {
                ASTNode *init = ast_create_num_lit(0, lineCount);
                $$ = ast_create_decl(TYPE_NMBR, $2, init, lineCount);
            }
        }
        | TOK_CHR TOK_IDENTIFIER {
            Symbol *s = insert($2, TYPE_CHR, lineCount, &error_count);
            if (!s) {
                $$ = NULL;
            } else {
                ASTNode *init = ast_create_chr_lit('\0', lineCount);
                $$ = ast_create_decl(TYPE_CHR, $2, init, lineCount);
            }
        }
        | TOK_FLEX TOK_IDENTIFIER {
            Symbol *s = insert($2, TYPE_FLEX, lineCount, &error_count);
            if (!s) {
                $$ = NULL;
            } else {
                ASTNode *init = ast_create_str_lit("", lineCount);
                $$ = ast_create_decl(TYPE_FLEX, $2, init, lineCount);
            }
        }
        
        /* ===== ERROR HANDLING ===== */
        
        /* Error: Missing identifier after type keyword */
        | TOK_NMBR error {
            fprintf(stderr, "Expected identifier after 'nmbr', got '%s'\n",yytext);
            error_count++;
            yyerrok;
            $$ = NULL;
        }
        | TOK_CHR error {
            fprintf(stderr, "Expected identifier after 'chr', got '%s'\n",yytext);
            error_count++;
            yyerrok;
            $$ = NULL;
        }
        | TOK_FLEX error {
            fprintf(stderr, "Expected identifier after 'flex', got '%s'\n", yytext);
            error_count++;
            yyerrok;
            $$ = NULL;
        }
        
        /* Error: Wrong token after identifier (expected ':' or newline/comma) */
        | TOK_NMBR TOK_IDENTIFIER error {
            fprintf(stderr, "Expected ':' or end of declaration after 'nmbr %s', got '%s'\n", $2, yytext);
            error_count++;
            yyerrok;
            $$ = NULL;
        }
        | TOK_CHR TOK_IDENTIFIER error {
            fprintf(stderr, "Error at line %d: Expected ':' or end of declaration after 'chr %s', got '%s'\n", lineCount, $2, yytext);
            error_count++;
            yyerrok;
            $$ = NULL;
        }
        | TOK_FLEX TOK_IDENTIFIER error {
            fprintf(stderr, "Expected ':' or end of declaration after 'flex %s', got '%s'\n", $2, yytext);
            error_count++;
            yyerrok;
            $$ = NULL;
        }
        
        /* Error: Missing or invalid expression after ':' */
        | TOK_NMBR TOK_IDENTIFIER TOK_ASSIGN error {
            fprintf(stderr, "Expected expression after 'nmbr %s =', got '%s'\n ", $2, yytext);
            error_count++;
            yyerrok;
            $$ = NULL;
        }
        | TOK_CHR TOK_IDENTIFIER TOK_ASSIGN error {
            fprintf(stderr, "Expected expression after 'chr %s =', got '%s'\n ", $2, yytext);
            error_count++;
            yyerrok;
            $$ = NULL;
        }
        | TOK_FLEX TOK_IDENTIFIER TOK_ASSIGN error {
            fprintf(stderr,"Expected expression after 'flex %s =', got '%s'\n ", $2, yytext);
            error_count++;
            yyerrok;
            $$ = NULL;
        }
        ;

assignment:
        TOK_IDENTIFIER TOK_ASSIGN expr {
            Symbol *s = lookup($1);
            if (!s) {
                    error_count++;
                    fprintf(stderr, "Error at line %d: Undefined variable '%s'\n ", lineCount, $1);
            }
            $$ = ast_create_assign($1, $3, lineCount);
        }
        ;

compound_assign:
        TOK_IDENTIFIER TOK_PLUS_ASSIGN expr {
            Symbol *s = lookup($1);
            if (!s) {
                error_count++;
                fprintf(stderr, "Error at line %d: Undefined variable '%s'\n", lineCount, $1);
                
            }
            $$ = ast_create_compound_assign($1, OP_PLUS_ASSIGN, $3, lineCount);
        }
        | TOK_IDENTIFIER TOK_MINUS_ASSIGN expr {
            Symbol *s = lookup($1);
            if (!s) {
                error_count++;
                fprintf(stderr, "Error at line %d: Undefined variable '%s'\n", lineCount, $1);
                
            }
            $$ = ast_create_compound_assign($1, OP_MINUS_ASSIGN, $3, lineCount);
        }
        | TOK_IDENTIFIER TOK_MULT_ASSIGN expr {
            Symbol *s = lookup($1);
            if (!s) {
                error_count++;
                fprintf(stderr, "Error at line %d: Undefined variable '%s'\n", lineCount, $1);
                    
            }
            $$ = ast_create_compound_assign($1, OP_MULT_ASSIGN, $3, lineCount);
        }
        | TOK_IDENTIFIER TOK_DIV_ASSIGN expr {
            Symbol *s = lookup($1);
            if (!s) {
                error_count++;
                fprintf(stderr, "Error at line %d: Undefined variable '%s'\n", lineCount, $1);
            }
            $$ = ast_create_compound_assign($1, OP_DIV_ASSIGN, $3, lineCount);
        }
    ;

//with explicit arithmetic in shw
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
        | expr              { $$ = $1; }
    ;

expr:
        expr TOK_PLUS term      { $$ = ast_create_binop(OP_ADD, $1, $3, lineCount); }
        | expr TOK_MINUS term   { $$ = ast_create_binop(OP_SUB, $1, $3, lineCount); }
        | term                  { $$ = $1; }
    ;

term:
        term TOK_MULT factor    { $$ = ast_create_binop(OP_MUL, $1, $3, lineCount); }
        | term TOK_DIV factor   { $$ = ast_create_binop(OP_DIV, $1, $3, lineCount); }
        | factor                { $$ = $1; }
    ;

factor:
        TOK_NUMBER_LITERAL        { $$ = ast_create_num_lit($1, lineCount); }
        | TOK_CHAR_LITERAL        { $$ = ast_create_chr_lit($1, lineCount); }
        | TOK_IDENTIFIER          { 
            Symbol *s = lookup($1);
            if (!s) {
                error_count++;
                fprintf(stderr, "\nError at line %d: Undefined variable '%s'\n", lineCount, $1);
                $$ = NULL;  // Return NULL to indicate error
            } else {
                $$ = ast_create_ident($1, lineCount);
            }
        }
        | TOK_LPAREN expr TOK_RPAREN { $$ = $2; }
        | TOK_MINUS factor        {
            /* Unary minus: 0 - factor */
            if (!$2) {
                $$ = NULL;
            } else {
                ASTNode *zero = ast_create_num_lit(0, lineCount);
                $$ = ast_create_binop(OP_SUB, zero, $2, lineCount);
            }
        }
        | TOK_PLUS factor         {
            /* Unary plus: just return factor */
            $$ = $2;
        }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Error at line %d: %s Unexpected: '%s'\n", lineCount, s,yytext);
    error_count++;
}


int Semantic_analysis(){
    if(DEBUG_MODE)  printf("\n=== Semantic Analysis ===\n\n");

    if(error_count > 0){
        if(DEBUG_MODE) printf("Skipping semantic analysis due to %d previous error(s).\n", error_count);
        return 1;
    }

    int semantic_errors = 0;
    if (ast_check_semantics(root, &semantic_errors)) {
        if(DEBUG_MODE) printf("Semantic analysis passed.\n");
    } else {
        if(DEBUG_MODE) printf("Semantic analysis failed with %d error(s).\n", semantic_errors);
        ast_free(root);
        if(DEBUG_MODE) printf("\n=== Compilation Failed ===\n");
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
    // Check for parsing errors first

    LARGE_INTEGER frequency, start, end;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    printf("console:\"");
    int result = yyparse();
        
    if (result != 0 || !root) {

        printf("\",\n");
        if (root) ast_free(root);
        return 1;
    }
    
    if (DEBUG_MODE) ast_print(root, 0);

    // Semantic Analysis
    int sem_result = Semantic_analysis();
    if(sem_result != 0){
        printf("\",\n");
        return 1; //exit on semantic error
    }

    //TAC Generation and Execution
    TACProgram *tac = tac_generate(root);
    //Compute memory offsets for all variables
    compute_symbol_offsets();
    
    int result_execute = tac_execute(tac);
    QueryPerformanceCounter(&end);
    double elapsed = (double)(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;
    printf("\nExecution Time: %.3f ms\n", elapsed);
    printf("\",\n");

    if(result_execute == 0){

        if(DEBUG_MODE) {
            printf("\n=== Three-Address Code ===\n\n");
            tac_print(tac);
            printf("Parse result: %d, root: %p, error_count: %d\n", result, (void*)root, error_count);
            print_symbol_table();
        }
        tac_generate_assembly(tac);
    }
    

    tac_free(tac);
    ast_free(root);
    

    return 0;
}