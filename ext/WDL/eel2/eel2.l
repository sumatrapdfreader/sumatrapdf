%option reentrant
%option prefix="nseel"
%option bison-bridge
%option bison-locations
%option noyywrap
%option never-interactive
%option batch
%option nounput

%{
#include <stdlib.h>
#include <stdio.h>

#define YY_USER_ACTION yylloc->first_line = yylineno;

#define YY_FATAL_ERROR(msg) { ((struct yyguts_t*)yyscanner)->yyextra_r->errVar=1; }
#define YY_INPUT(buf,result,max_size) { (result) = nseel_gets(yyextra,(buf),max_size); }

#define YY_EXTRA_TYPE compileContext *

#undef YY_BUF_SIZE
#define YY_BUF_SIZE (NSEEL_MAX_VARIABLE_NAMELEN*2)

#undef YY_READ_BUF_SIZE
#define YY_READ_BUF_SIZE (NSEEL_MAX_VARIABLE_NAMELEN)

#include "y.tab.h"

#ifdef _WIN32
#define YY_NO_UNISTD_H
#endif

#include "ns-eel-int.h"

int nseel_gets(compileContext *ctx, char *buf, size_t sz);

#define PARSENUM *yylval = nseel_translate(yyextra,yytext, 0); return VALUE;
#define EEL_ACTION(x) return x;

#ifdef stdin
#undef stdin
#endif
#define stdin (0)

#ifdef stdout
#undef stdout
#endif
#define stdout (0)

static int g_fake_errno;
#ifdef errno
#undef errno
#endif

#define errno g_fake_errno

static void comment(yyscan_t yyscanner);

%}

%%

[0-9]+\.?[0-9]*               PARSENUM;
\.[0-9]+                      PARSENUM;
0[xX][0-9a-fA-F]*             PARSENUM; 
\$[xX][0-9a-fA-F]*            PARSENUM;
\$\~[0-9]*                    PARSENUM;
\$[Ee]                        PARSENUM;
\$[Pp][Ii]                    PARSENUM;
\$[Pp][Hh][Ii]                PARSENUM;
\$\'.\'                       PARSENUM;
\#[a-zA-Z0-9\._]*             *yylval = nseel_translate(yyextra,yytext, 0); return STRING_IDENTIFIER;
\<\<                          return TOKEN_SHL;
\>\>                          return TOKEN_SHR;
\<=                          return TOKEN_LTE;
\>=                          return TOKEN_GTE;
==                              return TOKEN_EQ;
===                              return TOKEN_EQ_EXACT;
\!=                              return TOKEN_NE;
\!==                              return TOKEN_NE_EXACT;
\&\&                              return TOKEN_LOGICAL_AND;
\|\|                              return TOKEN_LOGICAL_OR;
\+=                             return TOKEN_ADD_OP;
-=                              return TOKEN_SUB_OP;
%=                              return TOKEN_MOD_OP;
\|=                             return TOKEN_OR_OP;
\&=                             return TOKEN_AND_OP;
\~=                             return TOKEN_XOR_OP;
\/=                             return TOKEN_DIV_OP;
\*=                             return TOKEN_MUL_OP;
\^=                             return TOKEN_POW_OP;

[a-zA-Z_][a-zA-Z0-9\._]*        &yylval = nseel_createCompiledValuePtr((compileContext *)yyextra, NULL, yytext); return IDENTIFIER; 

[ \t\r\n]+      /* whitespace */
\/\/.*$         /* comment */
"/*"            { comment(yyscanner); }

.       return (int)yytext[0];

%%

static void comment(yyscan_t yyscanner)
{
    int c,lc=0;

    while (0 != (c = input(yyscanner)))
    {
        if (c == '/' && lc == '*') return;
        lc = c;
    }
    // end of file, ignore for now
}
