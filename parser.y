%{
// C code to be included at the top of the parser
#include <stdio.h>
// Protos for flex
int yylex(void);
void yyerror(const char *s);
%}

/* * 1. TOKEN DECLARATIONS
 * These are all the terminal symbols (tokens) that 
 * Flex will need to provide to Yacc.
 */
%token TOK_EOF 0 /* 0 signals end-of-file */

/* Punctuation */
%token TOK_TILDE
%token TOK_COMMA
%token TOK_LPAREN
%token TOK_RPAREN

/* Assignment Operators */
%token TOK_ASSIGN
%token TOK_PLUS_ASSIGN
%token TOK_MINUS_ASSIGN
%token TOK_MULT_ASSIGN
%token TOK_DIV_ASSIGN

/* Arithmetic Operators */
%token TOK_PLUS
%token TOK_MINUS
%token TOK_MULT
%token TOK_DIV

/* Keywords */
%token TOK_SHW
%token TOK_NMBR
%token TOK_CHR
%token TOK_FLEX /* Your flexible type */

/* Literals & Identifiers */
%token TOK_IDENT
%token TOK_NUMBER_LITERAL
%token TOK_CHAR_LITERAL
%token TOK_STRING_LITERAL

/* --------------------------------------------------- */
/* The grammar definitions start after the first %% */
%%

/* * 2. GRAMMAR RULES
 * The start symbol is the first rule listed (program).
 */

program:
    stmt_list 
    ;

/*
 * CHANGED: This is now left-recursive.
 * It builds the list of statements from left to right.
 */
stmt_list:
        /* empty */ 
    | stmt_list stmt
    ;

stmt:
    decl_stmt TOK_TILDE
    | assign_stmt TOK_TILDE
    | shw_stmt TOK_TILDE
    | expr_stmt TOK_TILDE
    | TOK_TILDE
    ;

decl_stmt:
    type init_list
    ;

type:
    TOK_NMBR
    | TOK_CHR
    | TOK_FLEX  
    ;


init_list:
    init
    | init_list TOK_COMMA init
    ;

init:
    TOK_IDENT
    | TOK_IDENT TOK_ASSIGN expression
    ;

assign_stmt:
    TOK_IDENT assign_op expression
    ;

assign_op:
    TOK_ASSIGN
    | TOK_PLUS_ASSIGN
    | TOK_MINUS_ASSIGN
    | TOK_MULT_ASSIGN
    | TOK_DIV_ASSIGN
    ;

shw_stmt:
    TOK_SHW TOK_LPAREN arg_list_opt TOK_RPAREN
    ;

/* * This rule simplifies the optional list.
 * A shw() call can have nothing, or it can have a list.
 */
arg_list_opt:
    /* empty */
    | arg_list
    ;

/*
 * CHANGED: This is now left-recursive.
 * Original: expression | expression TOK_COMMA arg_list
 */
arg_list:
    expression
    | arg_list TOK_COMMA expression
    ;

expr_stmt:
    expression
    ;


expression:
    additive_expression
    ;

additive_expression:
    multiplicative_expression
    | additive_expression TOK_PLUS multiplicative_expression
    | additive_expression TOK_MINUS multiplicative_expression
    ;

multiplicative_expression:
    unary_expression
    | multiplicative_expression TOK_MULT unary_expression
    | multiplicative_expression TOK_DIV unary_expression
    ;

unary_expression:
    primary_expression
    | TOK_PLUS unary_expression
    | TOK_MINUS unary_expression
    ;

primary_expression:
    TOK_NUMBER_LITERAL
    | TOK_CHAR_LITERAL
    | TOK_STRING_LITERAL
    | TOK_IDENT
    | TOK_LPAREN expression TOK_RPAREN
    ;

%%

/* * 4. C CODE SECTION
 * This code is copied to the end of the generated C file.
 */

// Your C error-reporting function
void yyerror(const char *s) {
    fprintf(stderr, "Parse error: %s\n", s);
}


int main() {
    // yyparse() returns 0 on success
    if (yyparse() == 0) {
        printf("Parse successful!\n");
    } else {
        printf("Parse failed.\n");
    }
    return 0;
}
