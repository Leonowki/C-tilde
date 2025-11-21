%{
#include "ast.h"
#include "parser.tab.h"
#include <string.h>
#include <stdlib.h>

/* yylval is provided by Bison via parser.tab.h */
extern YYSTYPE yylval;

/* helper to unquote a string literal (very small unescape) */
static char *unquote_string(const char *s) {
    size_t len = strlen(s);
    if (len < 2) return strdup("");
    /* skip starting and ending quote */
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

/* unquote char literal like 'a' or '\n' */
static char unquote_char(const char *s) {
    /* s like 'x' or '\n' or '\'' */
    if (s[0] != '\'' || s[strlen(s)-1] != '\'') return 0;
    if (s[1] == '\\') {
        char esc = s[2];
        switch (esc) {
            case 'n': return '\n';
            case 't': return '\t';
            case '\\': return '\\';
            case '\'': return '\'';
            case '0': return '\0';
            default: return esc;
        }
    } else {
        return s[1];
    }
}

%}
%option yylineno

%x COMMENT

DIGIT     [0-9]
ID        [a-zA-Z_][a-zA-Z0-9_]*

%%

"nmbr"    { return TOK_NMBR; }//int
"chr"     { return TOK_CHR; }//char
"flex"    { return TOK_FLEX; }//dynamic data type
"shw"     { return TOK_SHW; }//print function

{DIGIT}+  {
            yylval.num = atoi(yytext);// convert string to integer
            return TOK_NUMBER_LITERAL;
          }

'\\n'     { yylval.ch = '\n'; return TOK_CHAR_LITERAL; }
'\\t'     { yylval.ch = '\t'; return TOK_CHAR_LITERAL; }
'\\\\'    { yylval.ch = '\\'; return TOK_CHAR_LITERAL; }
'\\''     { yylval.ch = '\''; return TOK_CHAR_LITERAL; }
'\\0'     { yylval.ch = '\0'; return TOK_CHAR_LITERAL; }
'.'       { yylval.ch = yytext[1]; return TOK_CHAR_LITERAL; }

\"([^\"\n\\]|\\.)*\" {
            yylval.str = unquote_string(yytext);
            return TOK_STRING_LITERAL;
          }

{ID}      {
            yylval.str = strdup(yytext);
            return TOK_IDENT;
          }

"~"       { return TOK_TILDE; }
","       { return TOK_COMMA; }
"("       { return TOK_LPAREN; }
")"       { return TOK_RPAREN; }

"+="      { return TOK_PLUS_ASSIGN; }
"-="      { return TOK_MINUS_ASSIGN; }
"*="      { return TOK_MULT_ASSIGN; }
"/="      { return TOK_DIV_ASSIGN; }
"="       { return TOK_ASSIGN; }

"+"       { return TOK_PLUS; }
"-"       { return TOK_MINUS; }
"*"       { return TOK_MULT; }
"/"       { return TOK_DIV; }

[ \t\r\n]+   ;

"//".*    ;

"@".*     ;

"/*"              { BEGIN(COMMENT); }
<COMMENT>"*/"     { BEGIN(INITIAL); }
<COMMENT>\n       { }
<COMMENT>.        { }

.         {
            fprintf(stderr, "Lexical error: Unrecognized character: %c\n", *yytext);
          }

%%

int yywrap(void) {
    return 1;
}