%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"
#include "ast.h"


extern int yylex();
extern int yylineno;
extern char *yytext;

void yyerror(const char *s);

int lineCount = 1;
ASTNode *root = NULL;
int error_count = 0;

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
%token TOK_UNKNOWN

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
      TOK_NUMBER_LITERAL      { $$ = ast_create_num_lit($1, lineCount); }
    | TOK_IDENTIFIER          { $$ = ast_create_ident($1, lineCount); }
    | TOK_LPAREN expr TOK_RPAREN { $$ = $2; }
    ;

%%


void yyerror(const char *s) {
    fprintf(stderr, "Error at line %d: %s\n", lineCount, s);
    error_count++;
}

int main(int argc, char **argv) {
    int result = yyparse();
    if (result == 0 && root && error_count == 0) {
        printf("\n=== Abstract Syntax Tree ===\n\n");
        ast_print(root, 0);
        ast_free(root);
    }
    
    printf("\n=== Parsing Complete ===\n");
    return 0;
}