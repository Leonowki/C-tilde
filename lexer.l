%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"    

static char *unquote_string(const char *s) {
    size_t len = strlen(s);
    if (len < 2) return strdup("");
    char *out = malloc(len-1);
    size_t oi = 0;
    for (size_t i = 1; i + 1 < len; ++i) {
        if (s[i] == '\\' && i + 1 < len - 1) {
            ++i;
            switch (s[i]) {
                case 'n': out[oi++] = '\n'; break;
                case 't': out[oi++] = '\t'; break;
                case '\\': out[oi++] = '\\'; break;
                case '\'': out[oi++] = '\''; break;
                case '\"': out[oi++] = '\"'; break;
                case '0': out[oi++] = '\0'; break;
                default: out[oi++] = s[i]; break;
            }
        } else {
            out[oi++] = s[i];
        }
    }
    out[oi] = '\0';
    return out;
}
%}

%option yylineno

%x COMMENT

DIGIT     [0-9]
ID        [a-zA-Z_][a-zA-Z0-9_]*

%%

"nmbr"    { printf("TOK_NMBR ") ; return TOK_NMBR; } 
"chr"     { printf("TOK_CHR ") ; return TOK_CHR; } 
"flex"    { printf("TOK_FLEX ") ; return TOK_FLEX; } 
"shw"     { printf("TOK_SHW ") ; return TOK_SHW; }  

\n                      { printf("TOK_NEWLINE ") ; return TOK_NEWLINE; }

{DIGIT}+  {
            printf("TOK_NUMBER_LITERAL ") ;
            return TOK_NUMBER_LITERAL;
          }

","       { printf("TOK_COMMA ") ; return TOK_COMMA; }
"("       { printf("TOK_LPAREN ") ;  return TOK_LPAREN; }
")"       { printf("TOK_RPAREN ") ;  return TOK_RPAREN; }
"+:"      { printf("TOK_PLUS_ASSIGN ") ;  return TOK_PLUS_ASSIGN; }
"-:"      { printf("TOK_MINUS_ASSIGN ") ;  return TOK_MINUS_ASSIGN; }
"*:"      { printf("TOK_MULT_ASSIGN ") ;  return TOK_MULT_ASSIGN; }
"/:"      { printf("TOK_DIV_ASSIGN ") ;  return TOK_DIV_ASSIGN; }
":"       { printf("TOK_ASSIGN ") ; return TOK_ASSIGN; }
"."       { printf("TOK_CONCAT ") ;  return TOK_CONCAT; }

"+"       { printf("TOK_PLUS ") ;  return TOK_PLUS; }
"-"       { printf("TOK_MINUS ") ;  return TOK_MINUS; }
"*"       { printf("TOK_MULT ") ;  return TOK_MULT; }
"/"       { printf("TOK_DIV ") ;  return TOK_DIV; }

          
'\\n'     { printf("TOK_CHAR_LITERAL ") ;  return TOK_CHAR_LITERAL; }
'\\t'     { printf("TOK_CHAR_LITERAL ") ;  return TOK_CHAR_LITERAL; }
'\\\\'    { printf("TOK_CHAR_LITERAL ") ;   return TOK_CHAR_LITERAL; }
'\\''     { printf("TOK_CHAR_LITERAL ") ;   return TOK_CHAR_LITERAL; }
'\\0'     { printf("TOK_CHAR_LITERAL ") ;   return TOK_CHAR_LITERAL; }

\'[^\\]\'   {
    printf("TOK_CHAR_LITERAL ") ;

    return TOK_CHAR_LITERAL;
}

\"([^\"\n\\]|\\.)*\" {
            printf("TOK_STRING_LITERAL ") ;

            return TOK_STRING_LITERAL;
          }

{ID}      {
            printf("TOK_IDENTIFIER ") ;

            return TOK_IDENTIFIER;
          }

[ \t\r]+                ;


.         { printf("TOK_UNKNOWN ") ;  return TOK_UNKNOWN; }



"//".* ;

"@".* ;

"/*"              { BEGIN(COMMENT); }
<COMMENT>"*/"     { BEGIN(INITIAL); }
<COMMENT>\n       { }
<COMMENT>.        { }

%%



int yywrap(void) {
    return 1;
}