/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     VALUE = 258,
     IDENTIFIER = 259,
     TOKEN_SHL = 260,
     TOKEN_SHR = 261,
     TOKEN_LTE = 262,
     TOKEN_GTE = 263,
     TOKEN_EQ = 264,
     TOKEN_EQ_EXACT = 265,
     TOKEN_NE = 266,
     TOKEN_NE_EXACT = 267,
     TOKEN_LOGICAL_AND = 268,
     TOKEN_LOGICAL_OR = 269,
     TOKEN_ADD_OP = 270,
     TOKEN_SUB_OP = 271,
     TOKEN_MOD_OP = 272,
     TOKEN_OR_OP = 273,
     TOKEN_AND_OP = 274,
     TOKEN_XOR_OP = 275,
     TOKEN_DIV_OP = 276,
     TOKEN_MUL_OP = 277,
     TOKEN_POW_OP = 278,
     STRING_LITERAL = 279,
     STRING_IDENTIFIER = 280
   };
#endif
/* Tokens.  */
#define VALUE 258
#define IDENTIFIER 259
#define TOKEN_SHL 260
#define TOKEN_SHR 261
#define TOKEN_LTE 262
#define TOKEN_GTE 263
#define TOKEN_EQ 264
#define TOKEN_EQ_EXACT 265
#define TOKEN_NE 266
#define TOKEN_NE_EXACT 267
#define TOKEN_LOGICAL_AND 268
#define TOKEN_LOGICAL_OR 269
#define TOKEN_ADD_OP 270
#define TOKEN_SUB_OP 271
#define TOKEN_MOD_OP 272
#define TOKEN_OR_OP 273
#define TOKEN_AND_OP 274
#define TOKEN_XOR_OP 275
#define TOKEN_DIV_OP 276
#define TOKEN_MUL_OP 277
#define TOKEN_POW_OP 278
#define STRING_LITERAL 279
#define STRING_IDENTIFIER 280




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


