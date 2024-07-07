#ifndef jsi_h
#define jsi_h

#include "mujs.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <math.h>
#include <float.h>
#include <limits.h>

/* NOTE: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=103052 */
#ifdef __GNUC__
#if (__GNUC__ >= 6)
#pragma GCC optimize ("no-ipa-pure-const")
#endif
#endif

/* Microsoft Visual C */
#ifdef _MSC_VER
#pragma warning(disable:4996) /* _CRT_SECURE_NO_WARNINGS */
#pragma warning(disable:4244) /* implicit conversion from double to int */
#pragma warning(disable:4267) /* implicit conversion of int to smaller int */
#pragma warning(disable:4090) /* broken const warnings */
#define inline __inline
#if _MSC_VER < 1900 /* MSVC 2015 */
#define snprintf jsW_snprintf
#define vsnprintf jsW_vsnprintf
static int jsW_vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
	int n;
	n = _vsnprintf(str, size, fmt, ap);
	str[size-1] = 0;
	return n;
}
static int jsW_snprintf(char *str, size_t size, const char *fmt, ...)
{
	int n;
	va_list ap;
	va_start(ap, fmt);
	n = jsW_vsnprintf(str, size, fmt, ap);
	va_end(ap);
	return n;
}
#endif
#if _MSC_VER <= 1700 /* <= MSVC 2012 */
#define isnan(x) _isnan(x)
#define isinf(x) (!_finite(x))
#define isfinite(x) _finite(x)
static __inline int signbit(double x) { __int64 i; memcpy(&i, &x, 8); return i>>63; }
#define INFINITY (DBL_MAX+DBL_MAX)
#define NAN (INFINITY-INFINITY)
#endif
#endif

#define soffsetof(x,y) ((int)offsetof(x,y))
#define nelem(a) (int)(sizeof (a) / sizeof (a)[0])

void *js_malloc(js_State *J, int size);
void *js_realloc(js_State *J, void *ptr, int size);
void js_free(js_State *J, void *ptr);

typedef union js_Value js_Value;
typedef struct js_Regexp js_Regexp;
typedef struct js_Object js_Object;
typedef struct js_String js_String;
typedef struct js_Ast js_Ast;
typedef struct js_Function js_Function;
typedef struct js_Environment js_Environment;
typedef struct js_StringNode js_StringNode;
typedef struct js_Jumpbuf js_Jumpbuf;
typedef struct js_StackTrace js_StackTrace;

/* Limits */

#ifndef JS_STACKSIZE
#define JS_STACKSIZE 4096	/* value stack size */
#endif
#ifndef JS_ENVLIMIT
#define JS_ENVLIMIT 1024	/* environment stack size */
#endif
#ifndef JS_TRYLIMIT
#define JS_TRYLIMIT 64		/* exception stack size */
#endif

#ifndef JS_ARRAYLIMIT
#define JS_ARRAYLIMIT (1<<26)	/* limit arrays to 64M entries (1G of flat array data) */
#endif

#ifndef JS_GCFACTOR
/*
 * GC will try to trigger when memory usage is this value times the minimum
 * needed memory. E.g. if there are 100 remaining objects after GC and this
 * value is 5.0, then the next GC will trigger when the overall number is 500.
 * I.e. a value of 5.0 aims at 80% garbage, 20% remain-used on each GC.
 * The bigger the value the less impact GC has on overall performance, but more
 * memory is used and individual GC pauses are longer (but fewer).
 */
#define JS_GCFACTOR 5.0		/* memory overhead factor >= 1.0 */
#endif

#ifndef JS_ASTLIMIT
#define JS_ASTLIMIT 400		/* max nested expressions */
#endif

#ifndef JS_STRLIMIT
#define JS_STRLIMIT (1<<28)	/* max string length */
#endif

/* instruction size -- change to int if you get integer overflow syntax errors */

#ifdef JS_INSTRUCTION
typedef JS_INSTRUCTION js_Instruction;
#else
typedef unsigned short js_Instruction;
#endif

/* String interning */

char *js_strdup(js_State *J, const char *s);
const char *js_intern(js_State *J, const char *s);
void jsS_dumpstrings(js_State *J);
void jsS_freestrings(js_State *J);

/* Portable strtod and printf float formatting */

void js_fmtexp(char *p, int e);
int js_grisu2(double v, char *buffer, int *K);
double js_strtod(const char *as, char **aas);

double js_strtol(const char *s, char **ep, int radix);

/* Private stack functions */

void js_newarguments(js_State *J);
void js_newfunction(js_State *J, js_Function *function, js_Environment *scope);
void js_newscript(js_State *J, js_Function *function, js_Environment *scope);
void js_loadeval(js_State *J, const char *filename, const char *source);

js_Regexp *js_toregexp(js_State *J, int idx);
int js_isarrayindex(js_State *J, const char *str, int *idx);
int js_runeat(js_State *J, const char *s, int i);
int js_utflen(const char *s);
int js_utfptrtoidx(const char *s, const char *p);

void js_dup(js_State *J);
void js_dup2(js_State *J);
void js_rot2(js_State *J);
void js_rot3(js_State *J);
void js_rot4(js_State *J);
void js_rot2pop1(js_State *J);
void js_rot3pop2(js_State *J);
void js_dup1rot3(js_State *J);
void js_dup1rot4(js_State *J);

void js_RegExp_prototype_exec(js_State *J, js_Regexp *re, const char *text);

void js_trap(js_State *J, int pc); /* dump stack and environment to stdout */

struct js_StackTrace
{
	const char *name;
	const char *file;
	int line;
};

/* Exception handling */

struct js_Jumpbuf
{
	jmp_buf buf;
	js_Environment *E;
	int envtop;
	int tracetop;
	int top, bot;
	int strict;
	js_Instruction *pc;
};

void *js_savetrypc(js_State *J, js_Instruction *pc);

#define js_trypc(J, PC) \
	setjmp(js_savetrypc(J, PC))

/* String buffer */

typedef struct js_Buffer { int n, m; char s[64]; } js_Buffer;

void js_putc(js_State *J, js_Buffer **sbp, int c);
void js_puts(js_State *J, js_Buffer **sb, const char *s);
void js_putm(js_State *J, js_Buffer **sb, const char *s, const char *e);

/* State struct */

struct js_State
{
	void *actx;
	void *uctx;
	js_Alloc alloc;
	js_Report report;
	js_Panic panic;

	js_StringNode *strings;

	int default_strict;
	int strict;

	/* parser input source */
	const char *filename;
	const char *source;
	int line;

	/* lexer state */
	struct { char *text; int len, cap; } lexbuf;
	int lexline;
	int lexchar;
	int lasttoken;
	int newline;

	/* parser state */
	int astdepth;
	int lookahead;
	const char *text;
	double number;
	js_Ast *gcast; /* list of allocated nodes to free after parsing */

	/* runtime environment */
	js_Object *Object_prototype;
	js_Object *Array_prototype;
	js_Object *Function_prototype;
	js_Object *Boolean_prototype;
	js_Object *Number_prototype;
	js_Object *String_prototype;
	js_Object *RegExp_prototype;
	js_Object *Date_prototype;

	js_Object *Error_prototype;
	js_Object *EvalError_prototype;
	js_Object *RangeError_prototype;
	js_Object *ReferenceError_prototype;
	js_Object *SyntaxError_prototype;
	js_Object *TypeError_prototype;
	js_Object *URIError_prototype;

	unsigned int seed; /* Math.random seed */

	char scratch[12]; /* scratch buffer for iterating over array indices */

	int nextref; /* for js_ref use */
	js_Object *R; /* registry of hidden values */
	js_Object *G; /* the global object */
	js_Environment *E; /* current environment scope */
	js_Environment *GE; /* global environment scope (at the root) */

	/* execution stack */
	int top, bot;
	js_Value *stack;

	/* garbage collector list */
	int gcpause;
	int gcmark;
	unsigned int gccounter, gcthresh;
	js_Environment *gcenv;
	js_Function *gcfun;
	js_Object *gcobj;
	js_String *gcstr;

	js_Object *gcroot; /* gc scan list */

	/* environments on the call stack but currently not in scope */
	int envtop;
	js_Environment *envstack[JS_ENVLIMIT];

	/* debug info stack trace */
	int tracetop;
	js_StackTrace trace[JS_ENVLIMIT];

	/* exception stack */
	int trytop;
	js_Jumpbuf trybuf[JS_TRYLIMIT];
};

/* Values */

typedef struct js_Property js_Property;
typedef struct js_Iterator js_Iterator;

/* Hint to ToPrimitive() */
enum {
	JS_HNONE,
	JS_HNUMBER,
	JS_HSTRING
};

enum js_Type {
	JS_TSHRSTR, /* type tag doubles as string zero-terminator */
	JS_TUNDEFINED,
	JS_TNULL,
	JS_TBOOLEAN,
	JS_TNUMBER,
	JS_TLITSTR,
	JS_TMEMSTR,
	JS_TOBJECT,
};

enum js_Class {
	JS_COBJECT,
	JS_CARRAY,
	JS_CFUNCTION,
	JS_CSCRIPT, /* function created from global/eval code */
	JS_CCFUNCTION, /* built-in function */
	JS_CERROR,
	JS_CBOOLEAN,
	JS_CNUMBER,
	JS_CSTRING,
	JS_CREGEXP,
	JS_CDATE,
	JS_CMATH,
	JS_CJSON,
	JS_CARGUMENTS,
	JS_CITERATOR,
	JS_CUSERDATA,
};

/*
	Short strings abuse the js_Value struct. By putting the type tag in the
	last byte, and using 0 as the tag for short strings, we can use the
	entire js_Value as string storage by letting the type tag serve double
	purpose as the string zero terminator.
*/

union js_Value
{
	struct {
		char pad[15];
		char type; /* type tag overlaps with final byte of shrstr */
	} t;
	union {
		char shrstr[16];
		int boolean;
		double number;
		const char *litstr;
		js_String *memstr;
		js_Object *object;
	} u;
};

struct js_String
{
	js_String *gcnext;
	char gcmark;
	char p[1];
};

struct js_Regexp
{
	void *prog;
	char *source;
	unsigned short flags;
	unsigned short last;
};

struct js_Object
{
	enum js_Class type;
	int extensible;
	js_Property *properties;
	int count; /* number of properties, for array sparseness check */
	js_Object *prototype;
	union {
		int boolean;
		double number;
		struct {
			int length;
			char *string;
			char shrstr[16];
		} s;
		struct {
			int length; /* actual length */
			int simple; /* true if array has only non-sparse array properties */
			int flat_length; /* used length of simple array part */
			int flat_capacity; /* allocated length of simple array part */
			js_Value *array;
		} a;
		struct {
			js_Function *function;
			js_Environment *scope;
		} f;
		struct {
			const char *name;
			js_CFunction function;
			js_CFunction constructor;
			int length;
			void *data;
			js_Finalize finalize;
		} c;
		js_Regexp r;
		struct {
			js_Object *target;
			int i, n; /* for array part */
			js_Iterator *head, *current; /* for object part */
		} iter;
		struct {
			const char *tag;
			void *data;
			js_HasProperty has;
			js_Put put;
			js_Delete delete;
			js_Finalize finalize;
		} user;
	} u;
	js_Object *gcnext; /* allocation list */
	js_Object *gcroot; /* scan list */
	int gcmark;
};

struct js_Property
{
	js_Property *left, *right;
	int level;
	int atts;
	js_Value value;
	js_Object *getter;
	js_Object *setter;
	char name[1];
};

struct js_Iterator
{
	js_Iterator *next;
	char name[1];
};

struct js_Environment
{
	js_Environment *outer;
	js_Object *variables;

	js_Environment *gcnext;
	int gcmark;
};

/* jsrun.c */
js_Environment *jsR_newenvironment(js_State *J, js_Object *variables, js_Environment *outer);
js_String *jsV_newmemstring(js_State *J, const char *s, int n);
js_Value *js_tovalue(js_State *J, int idx);
void js_toprimitive(js_State *J, int idx, int hint);
js_Object *js_toobject(js_State *J, int idx);
void js_pushvalue(js_State *J, js_Value v);
void js_pushobject(js_State *J, js_Object *v);
void jsR_unflattenarray(js_State *J, js_Object *obj);

/* jsvalue.c */
int jsV_toboolean(js_State *J, js_Value *v);
double jsV_tonumber(js_State *J, js_Value *v);
double jsV_tointeger(js_State *J, js_Value *v);
const char *jsV_tostring(js_State *J, js_Value *v);
js_Object *jsV_toobject(js_State *J, js_Value *v);
void jsV_toprimitive(js_State *J, js_Value *v, int preferred);

const char *js_itoa(char *buf, int a);
double js_stringtofloat(const char *s, char **ep);
int jsV_numbertointeger(double n);
int jsV_numbertoint32(double n);
unsigned int jsV_numbertouint32(double n);
short jsV_numbertoint16(double n);
unsigned short jsV_numbertouint16(double n);
const char *jsV_numbertostring(js_State *J, char buf[32], double number);
double jsV_stringtonumber(js_State *J, const char *string);

/* jsproperty.c */
js_Object *jsV_newobject(js_State *J, enum js_Class type, js_Object *prototype);
js_Property *jsV_getownproperty(js_State *J, js_Object *obj, const char *name);
js_Property *jsV_getpropertyx(js_State *J, js_Object *obj, const char *name, int *own);
js_Property *jsV_getproperty(js_State *J, js_Object *obj, const char *name);
js_Property *jsV_setproperty(js_State *J, js_Object *obj, const char *name);
js_Property *jsV_nextproperty(js_State *J, js_Object *obj, const char *name);
void jsV_delproperty(js_State *J, js_Object *obj, const char *name);

js_Object *jsV_newiterator(js_State *J, js_Object *obj, int own);
const char *jsV_nextiterator(js_State *J, js_Object *iter);

void jsV_resizearray(js_State *J, js_Object *obj, int newlen);

void jsV_unflattenarray(js_State *J, js_Object *obj);
void jsV_growarray(js_State *J, js_Object *obj);

/* Lexer */

enum
{
	TK_IDENTIFIER = 256,
	TK_NUMBER,
	TK_STRING,
	TK_REGEXP,

	/* multi-character punctuators */
	TK_LE,
	TK_GE,
	TK_EQ,
	TK_NE,
	TK_STRICTEQ,
	TK_STRICTNE,
	TK_SHL,
	TK_SHR,
	TK_USHR,
	TK_AND,
	TK_OR,
	TK_ADD_ASS,
	TK_SUB_ASS,
	TK_MUL_ASS,
	TK_DIV_ASS,
	TK_MOD_ASS,
	TK_SHL_ASS,
	TK_SHR_ASS,
	TK_USHR_ASS,
	TK_AND_ASS,
	TK_OR_ASS,
	TK_XOR_ASS,
	TK_INC,
	TK_DEC,

	/* keywords */
	TK_BREAK,
	TK_CASE,
	TK_CATCH,
	TK_CONTINUE,
	TK_DEBUGGER,
	TK_DEFAULT,
	TK_DELETE,
	TK_DO,
	TK_ELSE,
	TK_FALSE,
	TK_FINALLY,
	TK_FOR,
	TK_FUNCTION,
	TK_IF,
	TK_IN,
	TK_INSTANCEOF,
	TK_NEW,
	TK_NULL,
	TK_RETURN,
	TK_SWITCH,
	TK_THIS,
	TK_THROW,
	TK_TRUE,
	TK_TRY,
	TK_TYPEOF,
	TK_VAR,
	TK_VOID,
	TK_WHILE,
	TK_WITH,
};

int jsY_iswhite(int c);
int jsY_isnewline(int c);
int jsY_ishex(int c);
int jsY_tohex(int c);

const char *jsY_tokenstring(int token);
int jsY_findword(const char *s, const char **list, int num);

void jsY_initlex(js_State *J, const char *filename, const char *source);
int jsY_lex(js_State *J);
int jsY_lexjson(js_State *J);

/* Parser */

enum js_AstType
{
	AST_LIST,
	AST_FUNDEC,
	AST_IDENTIFIER,

	EXP_IDENTIFIER,
	EXP_NUMBER,
	EXP_STRING,
	EXP_REGEXP,

	/* literals */
	EXP_ELISION, /* for array elisions */
	EXP_NULL,
	EXP_TRUE,
	EXP_FALSE,
	EXP_THIS,

	EXP_ARRAY,
	EXP_OBJECT,
	EXP_PROP_VAL,
	EXP_PROP_GET,
	EXP_PROP_SET,

	EXP_FUN,

	/* expressions */
	EXP_INDEX,
	EXP_MEMBER,
	EXP_CALL,
	EXP_NEW,

	EXP_POSTINC,
	EXP_POSTDEC,

	EXP_DELETE,
	EXP_VOID,
	EXP_TYPEOF,
	EXP_PREINC,
	EXP_PREDEC,
	EXP_POS,
	EXP_NEG,
	EXP_BITNOT,
	EXP_LOGNOT,

	EXP_MOD,
	EXP_DIV,
	EXP_MUL,
	EXP_SUB,
	EXP_ADD,
	EXP_USHR,
	EXP_SHR,
	EXP_SHL,
	EXP_IN,
	EXP_INSTANCEOF,
	EXP_GE,
	EXP_LE,
	EXP_GT,
	EXP_LT,
	EXP_STRICTNE,
	EXP_STRICTEQ,
	EXP_NE,
	EXP_EQ,
	EXP_BITAND,
	EXP_BITXOR,
	EXP_BITOR,
	EXP_LOGAND,
	EXP_LOGOR,

	EXP_COND,

	EXP_ASS,
	EXP_ASS_MUL,
	EXP_ASS_DIV,
	EXP_ASS_MOD,
	EXP_ASS_ADD,
	EXP_ASS_SUB,
	EXP_ASS_SHL,
	EXP_ASS_SHR,
	EXP_ASS_USHR,
	EXP_ASS_BITAND,
	EXP_ASS_BITXOR,
	EXP_ASS_BITOR,

	EXP_COMMA,

	EXP_VAR, /* var initializer */

	/* statements */
	STM_BLOCK,
	STM_EMPTY,
	STM_VAR,
	STM_IF,
	STM_DO,
	STM_WHILE,
	STM_FOR,
	STM_FOR_VAR,
	STM_FOR_IN,
	STM_FOR_IN_VAR,
	STM_CONTINUE,
	STM_BREAK,
	STM_RETURN,
	STM_WITH,
	STM_SWITCH,
	STM_THROW,
	STM_TRY,
	STM_DEBUGGER,

	STM_LABEL,
	STM_CASE,
	STM_DEFAULT,
};

typedef struct js_JumpList js_JumpList;

struct js_JumpList
{
	enum js_AstType type;
	int inst;
	js_JumpList *next;
};

struct js_Ast
{
	enum js_AstType type;
	int line;
	js_Ast *parent, *a, *b, *c, *d;
	double number;
	const char *string;
	js_JumpList *jumps; /* list of break/continue jumps to patch */
	int casejump; /* for switch case clauses */
	js_Ast *gcnext; /* next in alloc list */
};

js_Ast *jsP_parsefunction(js_State *J, const char *filename, const char *params, const char *body);
js_Ast *jsP_parse(js_State *J, const char *filename, const char *source);
void jsP_freeparse(js_State *J);

/* Compiler */

enum js_OpCode
{
	OP_POP,		/* A -- */
	OP_DUP,		/* A -- A A */
	OP_DUP2,	/* A B -- A B A B */
	OP_ROT2,	/* A B -- B A */
	OP_ROT3,	/* A B C -- C A B */
	OP_ROT4,	/* A B C D -- D A B C */

	OP_INTEGER,	/* -K- (number-32768) */
	OP_NUMBER,	/* -N- <number> */
	OP_STRING,	/* -S- <string> */
	OP_CLOSURE,	/* -F- <closure> */

	OP_NEWARRAY,
	OP_NEWOBJECT,
	OP_NEWREGEXP,	/* -S,opts- <regexp> */

	OP_UNDEF,
	OP_NULL,
	OP_TRUE,
	OP_FALSE,

	OP_THIS,
	OP_CURRENT,	/* currently executing function object */

	OP_GETLOCAL,	/* -K- <value> */
	OP_SETLOCAL,	/* <value> -K- <value> */
	OP_DELLOCAL,	/* -K- false */

	OP_HASVAR,	/* -S- ( <value> | undefined ) */
	OP_GETVAR,	/* -S- <value> */
	OP_SETVAR,	/* <value> -S- <value> */
	OP_DELVAR,	/* -S- <success> */

	OP_IN,		/* <name> <obj> -- <exists?> */

	OP_SKIPARRAY,	/* <obj> -- <obj> */
	OP_INITARRAY,	/* <obj> <val> -- <obj> */
	OP_INITPROP,	/* <obj> <key> <val> -- <obj> */
	OP_INITGETTER,	/* <obj> <key> <closure> -- <obj> */
	OP_INITSETTER,	/* <obj> <key> <closure> -- <obj> */

	OP_GETPROP,	/* <obj> <name> -- <value> */
	OP_GETPROP_S,	/* <obj> -S- <value> */
	OP_SETPROP,	/* <obj> <name> <value> -- <value> */
	OP_SETPROP_S,	/* <obj> <value> -S- <value> */
	OP_DELPROP,	/* <obj> <name> -- <success> */
	OP_DELPROP_S,	/* <obj> -S- <success> */

	OP_ITERATOR,	/* <obj> -- <iobj> */
	OP_NEXTITER,	/* <iobj> -- ( <iobj> <name> true | false ) */

	OP_EVAL,	/* <args...> -(numargs)- <returnvalue> */
	OP_CALL,	/* <closure> <this> <args...> -(numargs)- <returnvalue> */
	OP_NEW,		/* <closure> <args...> -(numargs)- <returnvalue> */

	OP_TYPEOF,
	OP_POS,
	OP_NEG,
	OP_BITNOT,
	OP_LOGNOT,
	OP_INC,		/* <x> -- ToNumber(x)+1 */
	OP_DEC,		/* <x> -- ToNumber(x)-1 */
	OP_POSTINC,	/* <x> -- ToNumber(x)+1 ToNumber(x) */
	OP_POSTDEC,	/* <x> -- ToNumber(x)-1 ToNumber(x) */

	OP_MUL,
	OP_DIV,
	OP_MOD,
	OP_ADD,
	OP_SUB,
	OP_SHL,
	OP_SHR,
	OP_USHR,
	OP_LT,
	OP_GT,
	OP_LE,
	OP_GE,
	OP_EQ,
	OP_NE,
	OP_STRICTEQ,
	OP_STRICTNE,
	OP_JCASE,
	OP_BITAND,
	OP_BITXOR,
	OP_BITOR,

	OP_INSTANCEOF,

	OP_THROW,

	OP_TRY,		/* -ADDR- /jump/ or -ADDR- <exception> */
	OP_ENDTRY,

	OP_CATCH,	/* push scope chain with exception variable */
	OP_ENDCATCH,

	OP_WITH,
	OP_ENDWITH,

	OP_DEBUGGER,
	OP_JUMP,
	OP_JTRUE,
	OP_JFALSE,
	OP_RETURN,
};

struct js_Function
{
	const char *name;
	int script;
	int lightweight;
	int strict;
	int arguments;
	int numparams;

	js_Instruction *code;
	int codecap, codelen;

	js_Function **funtab;
	int funcap, funlen;

	const char **vartab;
	int varcap, varlen;

	const char *filename;
	int line, lastline;

	js_Function *gcnext;
	int gcmark;
};

js_Function *jsC_compilefunction(js_State *J, js_Ast *prog);
js_Function *jsC_compilescript(js_State *J, js_Ast *prog, int default_strict);

/* Builtins */

void jsB_init(js_State *J);
void jsB_initobject(js_State *J);
void jsB_initarray(js_State *J);
void jsB_initfunction(js_State *J);
void jsB_initboolean(js_State *J);
void jsB_initnumber(js_State *J);
void jsB_initstring(js_State *J);
void jsB_initregexp(js_State *J);
void jsB_initerror(js_State *J);
void jsB_initmath(js_State *J);
void jsB_initjson(js_State *J);
void jsB_initdate(js_State *J);

void jsB_propf(js_State *J, const char *name, js_CFunction cfun, int n);
void jsB_propn(js_State *J, const char *name, double number);
void jsB_props(js_State *J, const char *name, const char *string);

#endif
