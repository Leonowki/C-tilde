%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"
#include "ast.h"
#include "parser.tab.h"    

static char *unquote_string(const char *s) {
    size_t len = strlen(s);
    if (len < 2) return strdup("");
    char *out = malloc(len - 1);
    size_t oi = 0;
    for (size_t i = 1; i + 1 < len; ++i) {
        if (s[i] == '\\' && i + 1 < len - 1) {
            ++i;
            switch (s[i]) {
                case 'n':  out[oi++] = '\n'; break;
                case 't':  out[oi++] = '\t'; break;
                case '\\': out[oi++] = '\\'; break;
                case '\'': out[oi++] = '\''; break;
                case '\"': out[oi++] = '\"'; break;
                case '0':  out[oi++] = '\0'; break;
                default:   out[oi++] = s[i]; break;
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

    /* Comments - must come before catch-all */
"//".*                ;
"@".*                 ;
"/*"                  { BEGIN(COMMENT); }
<COMMENT>"*/"         { BEGIN(INITIAL); }
<COMMENT>\n           { }
<COMMENT>.            { }

    /* Keywords */
"nmbr"    { return TOK_NMBR; } 
"chr"     { return TOK_CHR; } 
"flex"    { return TOK_FLEX; } 
"shw"     { return TOK_SHW; }  

    /* Newline - handle both styles */
\r?\n     { return TOK_NEWLINE; }


    /* Number literals */
{DIGIT}+  {
            yylval.num = atoi(yytext);
            return TOK_NUMBER_LITERAL;
        }

    /* Compound assignment operators (must come before single char operators) */
"+:"      { return TOK_PLUS_ASSIGN; }
"-:"      { return TOK_MINUS_ASSIGN; }
"*:"      { return TOK_MULT_ASSIGN; }
"/:"      { return TOK_DIV_ASSIGN; }

    /* Single char operators and delimiters */
":"       { return TOK_ASSIGN; }
"."       { return TOK_CONCAT; }
"+"       { return TOK_PLUS; }
"-"       { return TOK_MINUS; }
"*"       { return TOK_MULT; }
"/"       { return TOK_DIV; }
","       { return TOK_COMMA; }
"("       { return TOK_LPAREN; }
")"       { return TOK_RPAREN; }

\\[ \t]*\r?\n   { /* ignore backslash + optional spaces + newline */ }

    /* Character literals - escape sequences */
'\\n'     { yylval.ch = '\n'; return TOK_CHAR_LITERAL; }
'\\t'     { yylval.ch = '\t'; return TOK_CHAR_LITERAL; }
'\\\\'    { yylval.ch = '\\'; return TOK_CHAR_LITERAL; }
'\\''     { yylval.ch = '\''; return TOK_CHAR_LITERAL; }
'\\0'     { yylval.ch = '\0'; return TOK_CHAR_LITERAL; }

    /* Character literals - regular */
\'[^\\]\'   {
            yylval.ch = yytext[1];
            return TOK_CHAR_LITERAL;
          }

    /* String literals */
\"([^\"\n\\]|\\.)*\" {
            yylval.str = unquote_string(yytext);
            return TOK_STRING_LITERAL;
          }

  

    /* Identifiers */
{ID}      {
            yylval.str = strdup(yytext);
            return TOK_IDENTIFIER;
          }


    /* Whitespace */
[ \t]+    ;

    /* Unknown characters */
.          { 
            fprintf(stderr, "Error at line %d: Unexpected character '%s'\n", 
                    yylineno, yytext);
            exit(1);
        }

%%

int yywrap(void) {
    return 1;
}