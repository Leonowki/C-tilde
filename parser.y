%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


extern int yylex();
extern int yylineno;
extern char *yytext;


typedef struct{ //Listing lines with error
   int errValue;
}errorList;
errorList list[40];

int checkError(int x, int y){
    if (x == y){
	return 1;
    }else{
	return 0;
    }
}
int lineCount = 1, cntr = 0; // track real line number per input line

void yyerror(const char *s);


%}

%union {
    int num;
    char ch;
    char* str;
}

// Keywords
%token TOK_NMBR
%token TOK_CHR
%token TOK_FLEX
%token TOK_SHW

// Operators
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

// Delimiters
%token TOK_LPAREN
%token TOK_RPAREN
%token TOK_NEWLINE

// Others
%token TOK_COMMA
%token TOK_UNKNOWN

// Literals
%token <num> TOK_NUMBER_LITERAL
%token <ch> TOK_CHAR_LITERAL
%token <str> TOK_STRING_LITERAL
%token <str> TOK_IDENTIFIER


%left '+' '-'

%type <num> exp

%%

program:	line_list
      		;


line_list:	line_list line
    		| line
    		;

line:		exp { 
		  int x, result;
		  for(x = 1; x <= cntr; x++){
		     result = checkError(list[x].errValue,lineCount);
		     if(result>0){ break; }
		  }
		  if(result <= 0){
		     printf("\nLINE %d: (Result: %d)\n", lineCount, $1);} 
		  }
    		| Statement_1
    		| error '\n' { yyerror("Invalid Character"); yyerrok; }
    		| TOK_NEWLINE {++lineCount;}
    		;


exp:	        TOK_NUMBER_LITERAL { $$ = $1; }
    		| exp '+' exp { $$ = $1 + $3; }
		| exp '-' exp { $$ = $1 - $3; }
    		;


Statement_1:    TOK_SHW TOK_STRING_LITERAL { printf("\nLINE %d: %s\n", lineCount, $2); }

%%

void yyerror(const char *s){
    fprintf(stderr, "Error at line %d: %s\n", lineCount, s);
}