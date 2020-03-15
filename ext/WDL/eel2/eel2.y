%pure-parser
%name-prefix="nseel"
%parse-param { compileContext* context }
%lex-param { void* scanner  }


/* this will prevent y.tab.c from ever calling yydestruct(), since we do not use it and it is a waste */
%destructor {
 #define yydestruct(a,b,c,d,e) 
} VALUE


%{
#ifdef _WIN32
#include <windows.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "y.tab.h"
#include "ns-eel-int.h"
  
#define scanner context->scanner
#define YY_(x) ("")

%}

%token VALUE IDENTIFIER TOKEN_SHL TOKEN_SHR 
%token TOKEN_LTE TOKEN_GTE TOKEN_EQ TOKEN_EQ_EXACT TOKEN_NE TOKEN_NE_EXACT TOKEN_LOGICAL_AND TOKEN_LOGICAL_OR
%token TOKEN_ADD_OP TOKEN_SUB_OP TOKEN_MOD_OP TOKEN_OR_OP TOKEN_AND_OP TOKEN_XOR_OP TOKEN_DIV_OP TOKEN_MUL_OP TOKEN_POW_OP
%token STRING_LITERAL STRING_IDENTIFIER
%expect 75
%start program

%%

more_params:
	expression
	| expression ',' more_params
	{
	  $$ = nseel_createMoreParametersOpcode(context,$1,$3);
	}
	;

string:
        STRING_LITERAL
        | STRING_LITERAL string
        {
          ((struct eelStringSegmentRec *)$1)->_next = (struct eelStringSegmentRec *)$2;
          $$ = $1;
        }
        ;

assignable_value:
	IDENTIFIER
        {
          if (!($$ = nseel_resolve_named_symbol(context, $1, -1, NULL))) /* convert from purely named to namespace-relative, etc */
          {
            yyerror(&yyloc, context, ""); 
            YYERROR;
          }
        }
        /* we used to have VALUE in here rather than rvalue, to allow 1=1 1+=2 etc, but silly to, 
           though this breaks Vmorph, which does 1=1 for a nop, and Jonas DrumReaplacer, which does x = 0 = y = 0 */
	| '(' expression ')'
	{
	  $$ = $2;
	}
	| IDENTIFIER '(' expression ')' '(' expression ')'
	{
          int err;
  	  if (!($$ = nseel_setCompiledFunctionCallParameters(context,$1, $3, 0, 0, $6, &err))) 
          { 
            if (err == -1) yyerror(&yylsp[-2], context, "");
            else if (err == 0) yyerror(&yylsp[-6], context, "");
            else yyerror(&yylsp[-3], context, ""); // parameter count wrong

            YYERROR; 
          }
	}
	| IDENTIFIER '(' expression ')'
	{
          int err;
  	  if (!($$ = nseel_setCompiledFunctionCallParameters(context,$1, $3, 0, 0, 0, &err))) 
          { 
            if (err == 0) yyerror(&yylsp[-3], context, "");
            else yyerror(&yylsp[0], context, ""); // parameter count wrong
            YYERROR; 
          }
	}
	| IDENTIFIER '(' ')'
	{
          int err;
  	  if (!($$ = nseel_setCompiledFunctionCallParameters(context,$1, nseel_createCompiledValue(context,0.0), 0, 0, 0,&err))) 
          { 
            if (err == 0) yyerror(&yylsp[-2], context, ""); // function not found
            else yyerror(&yylsp[0], context, ""); // parameter count wrong
            YYERROR; 
          }
	}
	| IDENTIFIER '(' expression ',' expression ')'
	{
          int err;
  	  if (!($$ = nseel_setCompiledFunctionCallParameters(context,$1, $3, $5, 0, 0,&err))) 
          { 
            if (err == 0) yyerror(&yylsp[-5], context, "");
            else if (err == 2) yyerror(&yylsp[0], context, ""); // needs more than 2 parameters
            else yyerror(&yylsp[-2], context, ""); // less than 2
            YYERROR; 
          }
	}
	| IDENTIFIER '(' expression ',' expression ',' more_params ')' 
	{
          int err;
  	  if (!($$ = nseel_setCompiledFunctionCallParameters(context,$1, $3, $5, $7, 0, &err))) 
          { 
            if (err == 0) yyerror(&yylsp[-7], context, "");
            else if (err==2) yyerror(&yylsp[0], context, ""); // needs more parameters
            else if (err==4) yyerror(&yylsp[-4], context, ""); // needs single parameter
            else yyerror(&yylsp[-2], context, ""); // less parm
            YYERROR; 
          }
	}
        | rvalue '[' ']'
        {
	  $$ = nseel_createMemoryAccess(context,$1,0);
        }
        | rvalue '[' expression ']'
        {
	  $$ = nseel_createMemoryAccess(context,$1,$3);
        }
        ;

rvalue:
	VALUE
        | STRING_IDENTIFIER
        | string
        {
          $$ = nseel_eelMakeOpcodeFromStringSegments(context,(struct eelStringSegmentRec *)$1);
        }
        | assignable_value
        ;


assignment:
        rvalue
        | assignable_value '=' if_else_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_ASSIGN,2,$1,$3);
        }
        | assignable_value TOKEN_ADD_OP if_else_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_ADD_OP,2,$1,$3);
        }
        | assignable_value TOKEN_SUB_OP if_else_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_SUB_OP,2,$1,$3);
        }
        | assignable_value TOKEN_MOD_OP if_else_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_MOD_OP,2,$1,$3);
        }
        | assignable_value TOKEN_OR_OP if_else_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_OR_OP,2,$1,$3);
        }
        | assignable_value TOKEN_AND_OP if_else_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_AND_OP,2,$1,$3);
        }
        | assignable_value TOKEN_XOR_OP if_else_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_XOR_OP,2,$1,$3);
        }
        | assignable_value TOKEN_DIV_OP if_else_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_DIV_OP,2,$1,$3);
        }
        | assignable_value TOKEN_MUL_OP if_else_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_MUL_OP,2,$1,$3);
        }
        | assignable_value TOKEN_POW_OP if_else_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_POW_OP,2,$1,$3);
        }
        | STRING_IDENTIFIER '=' if_else_expr
        {
          $$ = nseel_createFunctionByName(context,"strcpy",2,$1,$3,NULL); 
        }
        | STRING_IDENTIFIER TOKEN_ADD_OP if_else_expr
        {
          $$ = nseel_createFunctionByName(context,"strcat",2,$1,$3,NULL); 
        }
        ;

unary_expr:
        assignment
	| '+' unary_expr
	{
	  $$ = $2;
	}
	| '-' unary_expr
	{
	  $$ = nseel_createSimpleCompiledFunction(context,FN_UMINUS,1,$2,0);
	}
	| '!' unary_expr
	{
	  $$ = nseel_createSimpleCompiledFunction(context,FN_NOT,1,$2,0);
	}
	;

pow_expr:
        unary_expr
        | pow_expr '^' unary_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_POW,2,$1,$3);
        }
        ;

mod_expr:
        pow_expr
        | mod_expr '%' pow_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_MOD,2,$1,$3);
        }
        | mod_expr TOKEN_SHL pow_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_SHL,2,$1,$3);
        }
        | mod_expr TOKEN_SHR pow_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_SHR,2,$1,$3);
        }
        ;

div_expr:
	mod_expr
	| div_expr '/' mod_expr
	{
	  $$ = nseel_createSimpleCompiledFunction(context,FN_DIVIDE,2,$1,$3);
	}
	;


mul_expr:
	div_expr
	| mul_expr '*' div_expr
	{
	  $$ = nseel_createSimpleCompiledFunction(context,FN_MULTIPLY,2,$1,$3);
	}
	;


sub_expr:
	mul_expr
	| sub_expr '-' mul_expr
	{
	  $$ = nseel_createSimpleCompiledFunction(context,FN_SUB,2,$1,$3);
	}
	;

add_expr:
	sub_expr
	| add_expr '+' sub_expr
	{
	  $$ = nseel_createSimpleCompiledFunction(context,FN_ADD,2,$1,$3);
	}
	;

andor_expr:
	add_expr
	| andor_expr '&' add_expr
	{
	  $$ = nseel_createSimpleCompiledFunction(context,FN_AND,2,$1,$3);
	}
	| andor_expr '|' add_expr
	{
	  $$ = nseel_createSimpleCompiledFunction(context,FN_OR,2,$1,$3);
	}
	| andor_expr '~' add_expr
	{
	  $$ = nseel_createSimpleCompiledFunction(context,FN_XOR,2,$1,$3);
	}
	;

cmp_expr:
        andor_expr
        | cmp_expr '<' andor_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_LT,2,$1,$3);
        }
        | cmp_expr '>' andor_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_GT,2,$1,$3);
        }
        | cmp_expr TOKEN_LTE andor_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_LTE,2,$1,$3);
        }
        | cmp_expr TOKEN_GTE andor_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_GTE,2,$1,$3);
        }
        | cmp_expr TOKEN_EQ andor_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_EQ,2,$1,$3);
        }
        | cmp_expr TOKEN_EQ_EXACT andor_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_EQ_EXACT,2,$1,$3);
        }
        | cmp_expr TOKEN_NE andor_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_NE,2,$1,$3);
        }
        | cmp_expr TOKEN_NE_EXACT andor_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_NE_EXACT,2,$1,$3);
        }
        ;

logical_and_or_expr:
        cmp_expr
        | logical_and_or_expr TOKEN_LOGICAL_AND cmp_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_LOGICAL_AND,2,$1,$3);
        }
        | logical_and_or_expr TOKEN_LOGICAL_OR cmp_expr
        {
	  $$ = nseel_createSimpleCompiledFunction(context,FN_LOGICAL_OR,2,$1,$3);
        }
        ;

if_else_expr:
        logical_and_or_expr
        | logical_and_or_expr '?' if_else_expr ':' if_else_expr
        {
	  $$ = nseel_createIfElse(context, $1, $3, $5);
        }
        | logical_and_or_expr '?' ':' if_else_expr
        {
	  $$ = nseel_createIfElse(context, $1, 0, $4);
        }
        | logical_and_or_expr '?' if_else_expr
        {
	  $$ = nseel_createIfElse(context, $1, $3, 0);
        }
        ;


expression: 
	if_else_expr
	| expression ';' if_else_expr
	{
	  $$ = nseel_createSimpleCompiledFunction(context,FN_JOIN_STATEMENTS,2,$1,$3);
	}
	| expression ';'
	{
	  $$ = $1;
	}
	;


program:
	expression
	{ 
                if (@1.first_line) { }
                context->result = $1;
	}
	;


%%
