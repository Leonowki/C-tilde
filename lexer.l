%{

#include "parser.tab.h" 
#include <stdio.h>


%}

DIGIT     [0-9]
ID        [a-zA-Z_][a-zA-Z0-9_]*

%%

"nmbr"    { return TOK_NMBR; }
"chr"     { return TOK_CHR; }
"flex"    { return TOK_FLEX; }
"shw"     { return TOK_SHW; }


{DIGIT}+                   { return TOK_NUMBER_LITERAL; }

\'(\\.|[^'\\])\'           { return TOK_CHAR_LITERAL; }

\"(\\.|[^"\\])*\"          { return TOK_STRING_LITERAL; }

{ID}                       { return TOK_IDENT; }



"~"       { return TOK_TILDE; }
","       { return TOK_COMMA; }
"("       { return TOK_LPAREN; }
")"       { return TOK_RPAREN; }

"="       { return TOK_ASSIGN; }
"+="      { return TOK_PLUS_ASSIGN; }
"-="      { return TOK_MINUS_ASSIGN; }
"*="      { return TOK_MULT_ASSIGN; }
"/="      { return TOK_DIV_ASSIGN; }


"+"       { return TOK_PLUS; }
"-"       { return TOK_MINUS; }
"*"       { return TOK_MULT; }
"/"       { return TOK_DIV; }


[ \t\r\n]+   ;


"//".* ;


"@".* ;


"/*"(.|\n)*?"/" ;


.         { 
            printf("Lexical error: Unrecognized character: %c\n", *yytext);
            /* You might want to skip it or exit */
          }

%%


int yywrap(void) {
    /* Return 1 to signal that we are done scanning. */
    return 1;
}