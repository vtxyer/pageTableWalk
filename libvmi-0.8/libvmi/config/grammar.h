/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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
     NUM = 258,
     LINUX_TASKS = 259,
     LINUX_MM = 260,
     LINUX_PID = 261,
     LINUX_NAME = 262,
     LINUX_PGD = 263,
     LINUX_ADDR = 264,
     WIN_NTOSKRNL = 265,
     WIN_TASKS = 266,
     WIN_PDBASE = 267,
     WIN_PID = 268,
     WIN_PEB = 269,
     WIN_IBA = 270,
     WIN_PH = 271,
     WIN_PNAME = 272,
     WIN_KDVB = 273,
     WIN_SYSPROC = 274,
     SYSMAPTOK = 275,
     OSTYPETOK = 276,
     WORD = 277,
     FILENAME = 278,
     QUOTE = 279,
     OBRACE = 280,
     EBRACE = 281,
     SEMICOLON = 282,
     EQUALS = 283
   };
#endif
/* Tokens.  */
#define NUM 258
#define LINUX_TASKS 259
#define LINUX_MM 260
#define LINUX_PID 261
#define LINUX_NAME 262
#define LINUX_PGD 263
#define LINUX_ADDR 264
#define WIN_NTOSKRNL 265
#define WIN_TASKS 266
#define WIN_PDBASE 267
#define WIN_PID 268
#define WIN_PEB 269
#define WIN_IBA 270
#define WIN_PH 271
#define WIN_PNAME 272
#define WIN_KDVB 273
#define WIN_SYSPROC 274
#define SYSMAPTOK 275
#define OSTYPETOK 276
#define WORD 277
#define FILENAME 278
#define QUOTE 279
#define OBRACE 280
#define EBRACE 281
#define SEMICOLON 282
#define EQUALS 283




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 2068 of yacc.c  */
#line 263 "grammar.y"

    char *str;



/* Line 2068 of yacc.c  */
#line 112 "grammar.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


