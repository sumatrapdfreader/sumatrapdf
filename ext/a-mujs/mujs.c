#include "mujs.h"

#ifndef jsi_h
#define jsi_h

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <math.h>
#include <float.h>
#include <limits.h>

#ifdef __GNUC__
#if (__GNUC__ >= 6)
#pragma GCC optimize ("no-ipa-pure-const")
#endif
#endif

#ifdef _MSC_VER
#pragma warning(disable:4996)
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4090)
#define inline __inline
#if _MSC_VER < 1900
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
#if _MSC_VER <= 1700
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

#ifndef JS_STACKSIZE
#define JS_STACKSIZE 4096
#endif
#ifndef JS_ENVLIMIT
#define JS_ENVLIMIT 1024
#endif
#ifndef JS_TRYLIMIT
#define JS_TRYLIMIT 64
#endif

#ifndef JS_ARRAYLIMIT
#define JS_ARRAYLIMIT (1<<26)
#endif

#ifndef JS_GCFACTOR

#define JS_GCFACTOR 5.0
#endif

#ifndef JS_ASTLIMIT
#define JS_ASTLIMIT 400
#endif

#ifndef JS_STRLIMIT
#define JS_STRLIMIT (1<<28)
#endif

#ifdef JS_INSTRUCTION
typedef JS_INSTRUCTION js_Instruction;
#else
typedef unsigned short js_Instruction;
#endif

char *js_strdup(js_State *J, const char *s);
const char *js_intern(js_State *J, const char *s);
void jsS_dumpstrings(js_State *J);
void jsS_freestrings(js_State *J);

void js_fmtexp(char *p, int e);
int js_grisu2(double v, char *buffer, int *K);
double js_strtod(const char *as, char **aas);

double js_strtol(const char *s, char **ep, int radix);

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

void js_trap(js_State *J, int pc);

struct js_StackTrace
{
	const char *name;
	const char *file;
	int line;
	int stack;
};

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

typedef struct js_Buffer { int n, m; char s[64]; } js_Buffer;

void js_putc(js_State *J, js_Buffer **sbp, int c);
void js_puts(js_State *J, js_Buffer **sb, const char *s);
void js_putm(js_State *J, js_Buffer **sb, const char *s, const char *e);

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

	const char *filename;
	const char *source;
	int line;

	struct { char *text; int len, cap; } lexbuf;
	int lexline;
	int lexchar;
	int lasttoken;
	int newline;

	int astdepth;
	int lookahead;
	const char *text;
	double number;
	js_Ast *gcast;

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

	unsigned int seed;

	char scratch[12];

	int nextref;
	js_Object *R;
	js_Object *G;
	js_Environment *E;
	js_Environment *GE;

	int top, bot;
	js_Value *stack;

	int gcmark;
	unsigned int gccounter, gcthresh;
	js_Environment *gcenv;
	js_Function *gcfun;
	js_Object *gcobj;
	js_String *gcstr;

	js_Object *gcroot;

	int runlimit;
	int memlimit;

	int envtop;
	js_Environment *envstack[JS_ENVLIMIT];

	int tracetop;
	js_StackTrace trace[JS_ENVLIMIT];

	int trytop;
	js_Jumpbuf trybuf[JS_TRYLIMIT];
};

typedef struct js_Property js_Property;
typedef struct js_Iterator js_Iterator;

enum {
	JS_HNONE,
	JS_HNUMBER,
	JS_HSTRING
};

enum js_Type {
	JS_TSHRSTR,
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
	JS_CSCRIPT,
	JS_CCFUNCTION,
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

union js_Value
{
	struct {
		char pad[15];
		char type;
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
	int count;
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
			int length;
			int simple;
			int flat_length;
			int flat_capacity;
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
			int i, n;
			js_Iterator *head, *current;
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
	js_Object *gcnext;
	js_Object *gcroot;
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

js_Environment *jsR_newenvironment(js_State *J, js_Object *variables, js_Environment *outer);
js_String *jsV_newmemstring(js_State *J, const char *s, int n);
js_Value *js_tovalue(js_State *J, int idx);
void js_toprimitive(js_State *J, int idx, int hint);
js_Object *js_toobject(js_State *J, int idx);
void js_pushvalue(js_State *J, js_Value v);
void js_pushobject(js_State *J, js_Object *v);
void jsR_unflattenarray(js_State *J, js_Object *obj);

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

enum
{
	TK_IDENTIFIER = 256,
	TK_NUMBER,
	TK_STRING,
	TK_REGEXP,

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

enum js_AstType
{
	AST_LIST,
	AST_FUNDEC,
	AST_IDENTIFIER,

	EXP_IDENTIFIER,
	EXP_NUMBER,
	EXP_STRING,
	EXP_REGEXP,

	EXP_ELISION,
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

	EXP_VAR,

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
	js_JumpList *jumps;
	int casejump;
	js_Ast *gcnext;
};

js_Ast *jsP_parsefunction(js_State *J, const char *filename, const char *params, const char *body);
js_Ast *jsP_parse(js_State *J, const char *filename, const char *source);
void jsP_freeparse(js_State *J);

enum js_OpCode
{
	OP_POP,
	OP_DUP,
	OP_DUP2,
	OP_ROT2,
	OP_ROT3,
	OP_ROT4,

	OP_INTEGER,
	OP_NUMBER,
	OP_STRING,
	OP_CLOSURE,

	OP_NEWARRAY,
	OP_NEWOBJECT,
	OP_NEWREGEXP,

	OP_UNDEF,
	OP_NULL,
	OP_TRUE,
	OP_FALSE,

	OP_THIS,
	OP_CURRENT,

	OP_GETLOCAL,
	OP_SETLOCAL,
	OP_DELLOCAL,

	OP_HASVAR,
	OP_GETVAR,
	OP_SETVAR,
	OP_DELVAR,

	OP_IN,

	OP_SKIPARRAY,
	OP_INITARRAY,
	OP_INITPROP,
	OP_INITGETTER,
	OP_INITSETTER,

	OP_GETPROP,
	OP_GETPROP_S,
	OP_SETPROP,
	OP_SETPROP_S,
	OP_DELPROP,
	OP_DELPROP_S,

	OP_ITERATOR,
	OP_NEXTITER,

	OP_EVAL,
	OP_CALL,
	OP_NEW,

	OP_TYPEOF,
	OP_POS,
	OP_NEG,
	OP_BITNOT,
	OP_LOGNOT,
	OP_INC,
	OP_DEC,
	OP_POSTINC,
	OP_POSTDEC,

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

	OP_TRY,
	OP_ENDTRY,

	OP_CATCH,
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

#ifndef JS_HEAPSORT
#define JS_HEAPSORT 0
#endif

int js_getlength(js_State *J, int idx)
{
	int len;
	js_getproperty(J, idx, "length");
	len = js_tointeger(J, -1);
	js_pop(J, 1);
	return len;
}

void js_setlength(js_State *J, int idx, int len)
{
	js_pushnumber(J, len);
	js_setproperty(J, idx < 0 ? idx - 1 : idx, "length");
}

static void jsB_new_Array(js_State *J)
{
	int i, top = js_gettop(J);

	js_newarray(J);

	if (top == 2) {
		if (js_isnumber(J, 1)) {
			js_copy(J, 1);
			js_setproperty(J, -2, "length");
		} else {
			js_copy(J, 1);
			js_setindex(J, -2, 0);
		}
	} else {
		for (i = 1; i < top; ++i) {
			js_copy(J, i);
			js_setindex(J, -2, i - 1);
		}
	}
}

static void Ap_concat(js_State *J)
{
	int i, top = js_gettop(J);
	int n, k, len;

	js_newarray(J);
	n = 0;

	for (i = 0; i < top; ++i) {
		js_copy(J, i);
		if (js_isarray(J, -1)) {
			len = js_getlength(J, -1);
			for (k = 0; k < len; ++k)
				if (js_hasindex(J, -1, k))
					js_setindex(J, -3, n++);
			js_pop(J, 1);
		} else {
			js_setindex(J, -2, n++);
		}
	}
}

static void Ap_join(js_State *J);
static void Ap_toString(js_State *J);
static int Ap_join_cycle(js_State *J)
{
	js_Object *needle = js_toobject(J, 0);
	int top = J->tracetop - 1;
	while (top > 0) {
		int stk = J->trace[top].stack;
		js_Value *fun = &J->stack[stk-1];
		if (fun->t.type != JS_TOBJECT) return 0;
		if (fun->u.object->type != JS_CCFUNCTION) return 0;
		if (fun->u.object->u.c.function == Ap_join)
		{
			js_Value *obj = &J->stack[stk];
			if (obj->t.type != JS_TOBJECT) return 0;
			if (obj->u.object == needle)
				return 1;
		}
		else if (fun->u.object->u.c.function == Ap_toString)
		{

		}
		else
		{
			return 0;
		}
		--top;
	}
	return 0;
}

static void Ap_join(js_State *J)
{
	char * volatile out = NULL;
	const char * volatile r = NULL;
	const char *sep;
	int seplen;
	int k, n, len, rlen;

	if (Ap_join_cycle(J)) {
		js_pushliteral(J, "");
		return;
	}

	len = js_getlength(J, 0);

	if (js_isdefined(J, 1)) {
		sep = js_tostring(J, 1);
		seplen = strlen(sep);
	} else {
		sep = ",";
		seplen = 1;
	}

	if (len <= 0) {
		js_pushliteral(J, "");
		return;
	}

	if (js_try(J)) {
		js_free(J, out);
		js_throw(J);
	}

	n = 0;
	for (k = 0; k < len; ++k) {
		js_getindex(J, 0, k);
		if (js_iscoercible(J, -1)) {
			r = js_tostring(J, -1);
			rlen = strlen(r);
		} else {
			rlen = 0;
		}

		if (k == 0) {
			out = js_malloc(J, rlen + 1);
			if (rlen > 0) {
				memcpy(out, r, rlen);
				n += rlen;
			}
		} else {
			if (n + seplen + rlen > JS_STRLIMIT)
				js_rangeerror(J, "invalid string length");
			out = js_realloc(J, out, n + seplen + rlen + 1);
			if (seplen > 0) {
				memcpy(out + n, sep, seplen);
				n += seplen;
			}
			if (rlen > 0) {
				memcpy(out + n, r, rlen);
				n += rlen;
			}
		}

		js_pop(J, 1);
	}

	js_pushlstring(J, out, n);
	js_endtry(J);
	js_free(J, out);
}

static void Ap_pop(js_State *J)
{
	int n;

	n = js_getlength(J, 0);

	if (n > 0) {
		js_getindex(J, 0, n - 1);
		js_delindex(J, 0, n - 1);
		js_setlength(J, 0, n - 1);
	} else {
		js_setlength(J, 0, 0);
		js_pushundefined(J);
	}
}

static void Ap_push(js_State *J)
{
	int i, top = js_gettop(J);
	int n;

	n = js_getlength(J, 0);

	for (i = 1; i < top; ++i, ++n) {
		js_copy(J, i);
		js_setindex(J, 0, n);
	}

	js_setlength(J, 0, n);

	js_pushnumber(J, n);
}

static void Ap_reverse(js_State *J)
{
	int len, middle, lower;

	len = js_getlength(J, 0);
	middle = len / 2;
	lower = 0;

	while (lower != middle) {
		int upper = len - lower - 1;
		int haslower = js_hasindex(J, 0, lower);
		int hasupper = js_hasindex(J, 0, upper);
		if (haslower && hasupper) {
			js_setindex(J, 0, lower);
			js_setindex(J, 0, upper);
		} else if (hasupper) {
			js_setindex(J, 0, lower);
			js_delindex(J, 0, upper);
		} else if (haslower) {
			js_setindex(J, 0, upper);
			js_delindex(J, 0, lower);
		}
		++lower;
	}

	js_copy(J, 0);
}

static void Ap_shift(js_State *J)
{
	int k, len;

	len = js_getlength(J, 0);

	if (len == 0) {
		js_setlength(J, 0, 0);
		js_pushundefined(J);
		return;
	}

	js_getindex(J, 0, 0);

	for (k = 1; k < len; ++k) {
		if (js_hasindex(J, 0, k))
			js_setindex(J, 0, k - 1);
		else
			js_delindex(J, 0, k - 1);
	}

	js_delindex(J, 0, len - 1);
	js_setlength(J, 0, len - 1);
}

static void Ap_slice(js_State *J)
{
	int len, s, e, n;
	double sv, ev;

	js_newarray(J);

	len = js_getlength(J, 0);
	sv = js_tointeger(J, 1);
	ev = js_isdefined(J, 2) ? js_tointeger(J, 2) : len;

	if (sv < 0) sv = sv + len;
	if (ev < 0) ev = ev + len;

	s = sv < 0 ? 0 : sv > len ? len : sv;
	e = ev < 0 ? 0 : ev > len ? len : ev;

	for (n = 0; s < e; ++s, ++n)
		if (js_hasindex(J, 0, s))
			js_setindex(J, -2, n);
}

static int Ap_sort_cmp(js_State *J, int idx_a, int idx_b)
{
	js_Object *obj = js_tovalue(J, 0)->u.object;
	if (obj->u.a.simple && idx_b < obj->u.a.flat_length) {
		js_Value *val_a = &obj->u.a.array[idx_a];
		js_Value *val_b = &obj->u.a.array[idx_b];
		int und_a = val_a->t.type == JS_TUNDEFINED;
		int und_b = val_b->t.type == JS_TUNDEFINED;
		if (und_a) return und_b;
		if (und_b) return -1;
		if (js_iscallable(J, 1)) {
			double v;
			js_copy(J, 1);
			js_pushundefined(J);
			js_pushvalue(J, *val_a);
			js_pushvalue(J, *val_b);
			js_call(J, 2);
			v = js_tonumber(J, -1);
			js_pop(J, 1);
			if (isnan(v))
				return 0;
			if (v == 0)
				return 0;
			return v < 0 ? -1 : 1;
		} else {
			const char *str_a, *str_b;
			int c;
			js_pushvalue(J, *val_a);
			js_pushvalue(J, *val_b);
			str_a = js_tostring(J, -2);
			str_b = js_tostring(J, -1);
			c = strcmp(str_a, str_b);
			js_pop(J, 2);
			return c;
		}
	} else {
		int und_a, und_b;
		int has_a = js_hasindex(J, 0, idx_a);
		int has_b = js_hasindex(J, 0, idx_b);
		if (!has_a && !has_b) {
			return 0;
		}
		if (has_a && !has_b) {
			js_pop(J, 1);
			return -1;
		}
		if (!has_a && has_b) {
			js_pop(J, 1);
			return 1;
		}

		und_a = js_isundefined(J, -2);
		und_b = js_isundefined(J, -1);
		if (und_a) {
			js_pop(J, 2);
			return und_b;
		}
		if (und_b) {
			js_pop(J, 2);
			return -1;
		}

		if (js_iscallable(J, 1)) {
			double v;
			js_copy(J, 1);
			js_pushundefined(J);
			js_copy(J, -4);
			js_copy(J, -4);
			js_call(J, 2);
			v = js_tonumber(J, -1);
			js_pop(J, 3);
			if (isnan(v))
				return 0;
			if (v == 0)
				return 0;
			return v < 0 ? -1 : 1;
		} else {
			const char *str_a = js_tostring(J, -2);
			const char *str_b = js_tostring(J, -1);
			int c = strcmp(str_a, str_b);
			js_pop(J, 2);
			return c;
		}
	}
}

static void Ap_sort_swap(js_State *J, int idx_a, int idx_b)
{
	js_Object *obj = js_tovalue(J, 0)->u.object;
	if (obj->u.a.simple && idx_b < obj->u.a.flat_length) {
		js_Value tmp = obj->u.a.array[idx_a];
		obj->u.a.array[idx_a] = obj->u.a.array[idx_b];
		obj->u.a.array[idx_b] = tmp;
	} else {
		int has_a = js_hasindex(J, 0, idx_a);
		int has_b = js_hasindex(J, 0, idx_b);
		if (has_a && has_b) {
			js_setindex(J, 0, idx_a);
			js_setindex(J, 0, idx_b);
		} else if (has_a && !has_b) {
			js_delindex(J, 0, idx_a);
			js_setindex(J, 0, idx_b);
		} else if (!has_a && has_b) {
			js_delindex(J, 0, idx_b);
			js_setindex(J, 0, idx_a);
		}
	}
}

static int Ap_sort_leaf(js_State *J, int i, int end)
{
	int j = i;
	int lc = (j << 1) + 1;
	int rc = (j << 1) + 2;
	while (rc < end) {
		if (Ap_sort_cmp(J, lc, rc) <= 0)
			j = rc;
		else
			j = lc;
		lc = (j << 1) + 1;
		rc = (j << 1) + 2;
	}
	if (lc < end)
		j = lc;
	return j;
}

static void Ap_sort_sift(js_State *J, int i, int end)
{
	int j = Ap_sort_leaf(J, i, end);
	while (j > i && Ap_sort_cmp(J, i, j) > 0) {
		j = (j - 1) >> 1;
	}
	while (j > i) {
		Ap_sort_swap(J, i, j);
		j = (j - 1) >> 1;
	}
}

static void Ap_sort_heapsort(js_State *J, int n)
{
	int i;
	for (i = n / 2 - 1; i >= 0; --i)
		Ap_sort_sift(J, i, n);
	for (i = n - 1; i > 0; --i) {
		Ap_sort_swap(J, 0, i);
		Ap_sort_sift(J, 0, i);
	}
}

static void Ap_sort(js_State *J)
{
	int len;

	len = js_getlength(J, 0);
	if (len <= 1) {
		js_copy(J, 0);
		return;
	}

	if (!js_iscallable(J, 1) && !js_isundefined(J, 1))
		js_typeerror(J, "comparison function must be a function or undefined");

	if (len >= INT_MAX)
		js_rangeerror(J, "array is too large to sort");

	Ap_sort_heapsort(J, len);

	js_copy(J, 0);
}

static void Ap_splice(js_State *J)
{
	int top = js_gettop(J);
	int len, start, del, add, k;

	len = js_getlength(J, 0);
	start = js_tointeger(J, 1);
	if (start < 0)
		start = (len + start) > 0 ? len + start : 0;
	else if (start > len)
		start = len;

	if (js_isdefined(J, 2))
		del = js_tointeger(J, 2);
	else
		del = len - start;
	if (del > len - start)
		del = len - start;
	if (del < 0)
		del = 0;

	js_newarray(J);

	for (k = 0; k < del; ++k)
		if (js_hasindex(J, 0, start + k))
			js_setindex(J, -2, k);
	js_setlength(J, -1, del);

	add = top - 3;
	if (add < del) {
		for (k = start; k < len - del; ++k) {
			if (js_hasindex(J, 0, k + del))
				js_setindex(J, 0, k + add);
			else
				js_delindex(J, 0, k + add);
		}
		for (k = len; k > len - del + add; --k)
			js_delindex(J, 0, k - 1);
	} else if (add > del) {
		for (k = len - del; k > start; --k) {
			if (js_hasindex(J, 0, k + del - 1))
				js_setindex(J, 0, k + add - 1);
			else
				js_delindex(J, 0, k + add - 1);
		}
	}

	for (k = 0; k < add; ++k) {
		js_copy(J, 3 + k);
		js_setindex(J, 0, start + k);
	}

	js_setlength(J, 0, len - del + add);
}

static void Ap_unshift(js_State *J)
{
	int i, top = js_gettop(J);
	int k, len;

	len = js_getlength(J, 0);

	for (k = len; k > 0; --k) {
		int from = k - 1;
		int to = k + top - 2;
		if (js_hasindex(J, 0, from))
			js_setindex(J, 0, to);
		else
			js_delindex(J, 0, to);
	}

	for (i = 1; i < top; ++i) {
		js_copy(J, i);
		js_setindex(J, 0, i - 1);
	}

	js_setlength(J, 0, len + top - 1);

	js_pushnumber(J, len + top - 1);
}

static void Ap_toString(js_State *J)
{
	if (!js_iscoercible(J, 0))
		js_typeerror(J, "'this' is not an object");
	js_getproperty(J, 0, "join");
	if (!js_iscallable(J, -1)) {
		js_pop(J, 1);

		js_getglobal(J, "Object");
		js_getproperty(J, -1, "prototype");
		js_rot2pop1(J);
		js_getproperty(J, -1, "toString");
		js_rot2pop1(J);
	}
	js_copy(J, 0);
	js_call(J, 0);
}

static void Ap_indexOf(js_State *J)
{
	int k, len, from;

	len = js_getlength(J, 0);
	from = js_isdefined(J, 2) ? js_tointeger(J, 2) : 0;
	if (from < 0) from = len + from;
	if (from < 0) from = 0;

	js_copy(J, 1);
	for (k = from; k < len; ++k) {
		if (js_hasindex(J, 0, k)) {
			if (js_strictequal(J)) {
				js_pushnumber(J, k);
				return;
			}
			js_pop(J, 1);
		}
	}

	js_pushnumber(J, -1);
}

static void Ap_lastIndexOf(js_State *J)
{
	int k, len, from;

	len = js_getlength(J, 0);
	from = js_isdefined(J, 2) ? js_tointeger(J, 2) : len - 1;
	if (from > len - 1) from = len - 1;
	if (from < 0) from = len + from;

	js_copy(J, 1);
	for (k = from; k >= 0; --k) {
		if (js_hasindex(J, 0, k)) {
			if (js_strictequal(J)) {
				js_pushnumber(J, k);
				return;
			}
			js_pop(J, 1);
		}
	}

	js_pushnumber(J, -1);
}

static void Ap_every(js_State *J)
{
	int hasthis = js_gettop(J) >= 3;
	int k, len;

	if (!js_iscallable(J, 1))
		js_typeerror(J, "callback is not a function");

	len = js_getlength(J, 0);
	for (k = 0; k < len; ++k) {
		if (js_hasindex(J, 0, k)) {
			js_copy(J, 1);
			if (hasthis)
				js_copy(J, 2);
			else
				js_pushundefined(J);
			js_copy(J, -3);
			js_pushnumber(J, k);
			js_copy(J, 0);
			js_call(J, 3);
			if (!js_toboolean(J, -1))
				return;
			js_pop(J, 2);
		}
	}

	js_pushboolean(J, 1);
}

static void Ap_some(js_State *J)
{
	int hasthis = js_gettop(J) >= 3;
	int k, len;

	if (!js_iscallable(J, 1))
		js_typeerror(J, "callback is not a function");

	len = js_getlength(J, 0);
	for (k = 0; k < len; ++k) {
		if (js_hasindex(J, 0, k)) {
			js_copy(J, 1);
			if (hasthis)
				js_copy(J, 2);
			else
				js_pushundefined(J);
			js_copy(J, -3);
			js_pushnumber(J, k);
			js_copy(J, 0);
			js_call(J, 3);
			if (js_toboolean(J, -1))
				return;
			js_pop(J, 2);
		}
	}

	js_pushboolean(J, 0);
}

static void Ap_forEach(js_State *J)
{
	int hasthis = js_gettop(J) >= 3;
	int k, len;

	if (!js_iscallable(J, 1))
		js_typeerror(J, "callback is not a function");

	len = js_getlength(J, 0);
	for (k = 0; k < len; ++k) {
		if (js_hasindex(J, 0, k)) {
			js_copy(J, 1);
			if (hasthis)
				js_copy(J, 2);
			else
				js_pushundefined(J);
			js_copy(J, -3);
			js_pushnumber(J, k);
			js_copy(J, 0);
			js_call(J, 3);
			js_pop(J, 2);
		}
	}

	js_pushundefined(J);
}

static void Ap_map(js_State *J)
{
	int hasthis = js_gettop(J) >= 3;
	int k, len;

	if (!js_iscallable(J, 1))
		js_typeerror(J, "callback is not a function");

	js_newarray(J);

	len = js_getlength(J, 0);
	for (k = 0; k < len; ++k) {
		if (js_hasindex(J, 0, k)) {
			js_copy(J, 1);
			if (hasthis)
				js_copy(J, 2);
			else
				js_pushundefined(J);
			js_copy(J, -3);
			js_pushnumber(J, k);
			js_copy(J, 0);
			js_call(J, 3);
			js_setindex(J, -3, k);
			js_pop(J, 1);
		}
	}
	js_setlength(J, -1, len);
}

static void Ap_filter(js_State *J)
{
	int hasthis = js_gettop(J) >= 3;
	int k, to, len;

	if (!js_iscallable(J, 1))
		js_typeerror(J, "callback is not a function");

	js_newarray(J);
	to = 0;

	len = js_getlength(J, 0);
	for (k = 0; k < len; ++k) {
		if (js_hasindex(J, 0, k)) {
			js_copy(J, 1);
			if (hasthis)
				js_copy(J, 2);
			else
				js_pushundefined(J);
			js_copy(J, -3);
			js_pushnumber(J, k);
			js_copy(J, 0);
			js_call(J, 3);
			if (js_toboolean(J, -1)) {
				js_pop(J, 1);
				js_setindex(J, -2, to++);
			} else {
				js_pop(J, 2);
			}
		}
	}
}

static void Ap_reduce(js_State *J)
{
	int hasinitial = js_gettop(J) >= 3;
	int k, len;

	if (!js_iscallable(J, 1))
		js_typeerror(J, "callback is not a function");

	len = js_getlength(J, 0);
	k = 0;

	if (len == 0 && !hasinitial)
		js_typeerror(J, "no initial value");

	if (hasinitial)
		js_copy(J, 2);
	else {
		while (k < len)
			if (js_hasindex(J, 0, k++))
				break;
		if (k == len)
			js_typeerror(J, "no initial value");
	}

	while (k < len) {
		if (js_hasindex(J, 0, k)) {
			js_copy(J, 1);
			js_pushundefined(J);
			js_rot(J, 4);
			js_rot(J, 4);
			js_pushnumber(J, k);
			js_copy(J, 0);
			js_call(J, 4);
		}
		++k;
	}

}

static void Ap_reduceRight(js_State *J)
{
	int hasinitial = js_gettop(J) >= 3;
	int k, len;

	if (!js_iscallable(J, 1))
		js_typeerror(J, "callback is not a function");

	len = js_getlength(J, 0);
	k = len - 1;

	if (len == 0 && !hasinitial)
		js_typeerror(J, "no initial value");

	if (hasinitial)
		js_copy(J, 2);
	else {
		while (k >= 0)
			if (js_hasindex(J, 0, k--))
				break;
		if (k < 0)
			js_typeerror(J, "no initial value");
	}

	while (k >= 0) {
		if (js_hasindex(J, 0, k)) {
			js_copy(J, 1);
			js_pushundefined(J);
			js_rot(J, 4);
			js_rot(J, 4);
			js_pushnumber(J, k);
			js_copy(J, 0);
			js_call(J, 4);
		}
		--k;
	}

}

static void A_isArray(js_State *J)
{
	if (js_isobject(J, 1)) {
		js_Object *T = js_toobject(J, 1);
		js_pushboolean(J, T->type == JS_CARRAY);
	} else {
		js_pushboolean(J, 0);
	}
}

void jsB_initarray(js_State *J)
{
	js_pushobject(J, J->Array_prototype);
	{
		jsB_propf(J, "Array.prototype.toString", Ap_toString, 0);
		jsB_propf(J, "Array.prototype.concat", Ap_concat, 0);
		jsB_propf(J, "Array.prototype.join", Ap_join, 1);
		jsB_propf(J, "Array.prototype.pop", Ap_pop, 0);
		jsB_propf(J, "Array.prototype.push", Ap_push, 0);
		jsB_propf(J, "Array.prototype.reverse", Ap_reverse, 0);
		jsB_propf(J, "Array.prototype.shift", Ap_shift, 0);
		jsB_propf(J, "Array.prototype.slice", Ap_slice, 2);
		jsB_propf(J, "Array.prototype.sort", Ap_sort, 1);
		jsB_propf(J, "Array.prototype.splice", Ap_splice, 2);
		jsB_propf(J, "Array.prototype.unshift", Ap_unshift, 0);

		jsB_propf(J, "Array.prototype.indexOf", Ap_indexOf, 1);
		jsB_propf(J, "Array.prototype.lastIndexOf", Ap_lastIndexOf, 1);
		jsB_propf(J, "Array.prototype.every", Ap_every, 1);
		jsB_propf(J, "Array.prototype.some", Ap_some, 1);
		jsB_propf(J, "Array.prototype.forEach", Ap_forEach, 1);
		jsB_propf(J, "Array.prototype.map", Ap_map, 1);
		jsB_propf(J, "Array.prototype.filter", Ap_filter, 1);
		jsB_propf(J, "Array.prototype.reduce", Ap_reduce, 1);
		jsB_propf(J, "Array.prototype.reduceRight", Ap_reduceRight, 1);
	}
	js_newcconstructor(J, jsB_new_Array, jsB_new_Array, "Array", 0);
	{

		jsB_propf(J, "Array.isArray", A_isArray, 1);
	}
	js_defglobal(J, "Array", JS_DONTENUM);
}

static void jsB_new_Boolean(js_State *J)
{
	js_newboolean(J, js_toboolean(J, 1));
}

static void jsB_Boolean(js_State *J)
{
	js_pushboolean(J, js_toboolean(J, 1));
}

static void Bp_toString(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	if (self->type != JS_CBOOLEAN) js_typeerror(J, "not a boolean");
	js_pushliteral(J, self->u.boolean ? "true" : "false");
}

static void Bp_valueOf(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	if (self->type != JS_CBOOLEAN) js_typeerror(J, "not a boolean");
	js_pushboolean(J, self->u.boolean);
}

void jsB_initboolean(js_State *J)
{
	J->Boolean_prototype->u.boolean = 0;

	js_pushobject(J, J->Boolean_prototype);
	{
		jsB_propf(J, "Boolean.prototype.toString", Bp_toString, 0);
		jsB_propf(J, "Boolean.prototype.valueOf", Bp_valueOf, 0);
	}
	js_newcconstructor(J, jsB_Boolean, jsB_new_Boolean, "Boolean", 1);
	js_defglobal(J, "Boolean", JS_DONTENUM);
}

#ifndef regexp_h
#define regexp_h

#define regcompx js_regcompx
#define regfreex js_regfreex
#define regcomp js_regcomp
#define regexec js_regexec
#define regfree js_regfree

typedef struct Reprog Reprog;
typedef struct Resub Resub;

Reprog *regcompx(void *(*alloc)(void *ctx, void *p, int n), void *ctx,
	const char *pattern, int cflags, const char **errorp);
void regfreex(void *(*alloc)(void *ctx, void *p, int n), void *ctx,
	Reprog *prog);

Reprog *regcomp(const char *pattern, int cflags, const char **errorp);
int regexec(Reprog *prog, const char *string, Resub *sub, int eflags);
void regfree(Reprog *prog);

enum {

	REG_ICASE = 1,
	REG_NEWLINE = 2,

	REG_NOTBOL = 4,
};

#ifndef REG_MAXSUB
#define REG_MAXSUB 16
#endif

struct Resub {
	int nsub;
	struct {
		const char *sp;
		const char *ep;
	} sub[REG_MAXSUB];
};

#endif

static void jsB_globalf(js_State *J, const char *name, js_CFunction cfun, int n)
{
	js_newcfunction(J, cfun, name, n);
	js_defglobal(J, name, JS_DONTENUM);
}

void jsB_propf(js_State *J, const char *name, js_CFunction cfun, int n)
{
	const char *pname = strrchr(name, '.');
	pname = pname ? pname + 1 : name;
	js_newcfunction(J, cfun, name, n);
	js_defproperty(J, -2, pname, JS_DONTENUM);
}

void jsB_propn(js_State *J, const char *name, double number)
{
	js_pushnumber(J, number);
	js_defproperty(J, -2, name, JS_READONLY | JS_DONTENUM | JS_DONTCONF);
}

void jsB_props(js_State *J, const char *name, const char *string)
{
	js_pushliteral(J, string);
	js_defproperty(J, -2, name, JS_DONTENUM);
}

static void jsB_parseInt(js_State *J)
{
	const char *s = js_tostring(J, 1);
	int radix = js_isdefined(J, 2) ? js_tointeger(J, 2) : 0;
	double sign = 1;
	double n;
	char *e;

	while (jsY_iswhite(*s) || jsY_isnewline(*s))
		++s;
	if (*s == '-') {
		++s;
		sign = -1;
	} else if (*s == '+') {
		++s;
	}
	if (radix == 0) {
		radix = 10;
		if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
			s += 2;
			radix = 16;
		}
	} else if (radix < 2 || radix > 36) {
		js_pushnumber(J, NAN);
		return;
	}
	n = js_strtol(s, &e, radix);
	if (s == e)
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, n * sign);
}

static void jsB_parseFloat(js_State *J)
{
	const char *s = js_tostring(J, 1);
	char *e;
	double n;

	while (jsY_iswhite(*s) || jsY_isnewline(*s)) ++s;
	if (!strncmp(s, "Infinity", 8))
		js_pushnumber(J, INFINITY);
	else if (!strncmp(s, "+Infinity", 9))
		js_pushnumber(J, INFINITY);
	else if (!strncmp(s, "-Infinity", 9))
		js_pushnumber(J, -INFINITY);
	else {
		n = js_stringtofloat(s, &e);
		if (e == s)
			js_pushnumber(J, NAN);
		else
			js_pushnumber(J, n);
	}
}

static void jsB_isNaN(js_State *J)
{
	double n = js_tonumber(J, 1);
	js_pushboolean(J, isnan(n));
}

static void jsB_isFinite(js_State *J)
{
	double n = js_tonumber(J, 1);
	js_pushboolean(J, isfinite(n));
}

static void Encode(js_State *J, const char *str_, const char *unescaped)
{

	const char * volatile str = str_;
	js_Buffer *sb = NULL;

	static const char *HEX = "0123456789ABCDEF";

	if (js_try(J)) {
		js_free(J, sb);
		js_throw(J);
	}

	while (*str) {
		int c = (unsigned char) *str++;
		if (strchr(unescaped, c))
			js_putc(J, &sb, c);
		else {
			js_putc(J, &sb, '%');
			js_putc(J, &sb, HEX[(c >> 4) & 0xf]);
			js_putc(J, &sb, HEX[c & 0xf]);
		}
	}
	js_putc(J, &sb, 0);

	js_pushstring(J, sb ? sb->s : "");
	js_endtry(J);
	js_free(J, sb);
}

static void Decode(js_State *J, const char *str_, const char *reserved)
{

	const char * volatile str = str_;
	js_Buffer *sb = NULL;
	int a, b;

	if (js_try(J)) {
		js_free(J, sb);
		js_throw(J);
	}

	while (*str) {
		int c = (unsigned char) *str++;
		if (c != '%')
			js_putc(J, &sb, c);
		else {
			if (!str[0] || !str[1])
				js_urierror(J, "truncated escape sequence");
			a = *str++;
			b = *str++;
			if (!jsY_ishex(a) || !jsY_ishex(b))
				js_urierror(J, "invalid escape sequence");
			c = jsY_tohex(a) << 4 | jsY_tohex(b);
			if (!strchr(reserved, c))
				js_putc(J, &sb, c);
			else {
				js_putc(J, &sb, '%');
				js_putc(J, &sb, a);
				js_putc(J, &sb, b);
			}
		}
	}
	js_putc(J, &sb, 0);

	js_pushstring(J, sb ? sb->s : "");
	js_endtry(J);
	js_free(J, sb);
}

#define URIRESERVED ";/?:@&=+$,"
#define URIALPHA "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define URIDIGIT "0123456789"
#define URIMARK "-_.!~*'()"
#define URIUNESCAPED URIALPHA URIDIGIT URIMARK

static void jsB_decodeURI(js_State *J)
{
	Decode(J, js_tostring(J, 1), URIRESERVED "#");
}

static void jsB_decodeURIComponent(js_State *J)
{
	Decode(J, js_tostring(J, 1), "");
}

static void jsB_encodeURI(js_State *J)
{
	Encode(J, js_tostring(J, 1), URIUNESCAPED URIRESERVED "#");
}

static void jsB_encodeURIComponent(js_State *J)
{
	Encode(J, js_tostring(J, 1), URIUNESCAPED);
}

void jsB_init(js_State *J)
{

	J->Object_prototype = jsV_newobject(J, JS_COBJECT, NULL);
	J->Array_prototype = jsV_newobject(J, JS_CARRAY, J->Object_prototype);
	J->Function_prototype = jsV_newobject(J, JS_CCFUNCTION, J->Object_prototype);
	J->Boolean_prototype = jsV_newobject(J, JS_CBOOLEAN, J->Object_prototype);
	J->Number_prototype = jsV_newobject(J, JS_CNUMBER, J->Object_prototype);
	J->String_prototype = jsV_newobject(J, JS_CSTRING, J->Object_prototype);
	J->Date_prototype = jsV_newobject(J, JS_CDATE, J->Object_prototype);

	J->RegExp_prototype = jsV_newobject(J, JS_CREGEXP, J->Object_prototype);
	J->RegExp_prototype->u.r.prog = js_regcompx(J->alloc, J->actx, "(?:)", 0, NULL);
	J->RegExp_prototype->u.r.source = js_strdup(J, "(?:)");

	J->Error_prototype = jsV_newobject(J, JS_CERROR, J->Object_prototype);
	J->EvalError_prototype = jsV_newobject(J, JS_CERROR, J->Error_prototype);
	J->RangeError_prototype = jsV_newobject(J, JS_CERROR, J->Error_prototype);
	J->ReferenceError_prototype = jsV_newobject(J, JS_CERROR, J->Error_prototype);
	J->SyntaxError_prototype = jsV_newobject(J, JS_CERROR, J->Error_prototype);
	J->TypeError_prototype = jsV_newobject(J, JS_CERROR, J->Error_prototype);
	J->URIError_prototype = jsV_newobject(J, JS_CERROR, J->Error_prototype);

	jsB_initobject(J);
	jsB_initarray(J);
	jsB_initfunction(J);
	jsB_initboolean(J);
	jsB_initnumber(J);
	jsB_initstring(J);
	jsB_initregexp(J);
	jsB_initdate(J);
	jsB_initerror(J);
	jsB_initmath(J);
	jsB_initjson(J);

	js_pushnumber(J, NAN);
	js_defglobal(J, "NaN", JS_READONLY | JS_DONTENUM | JS_DONTCONF);

	js_pushnumber(J, INFINITY);
	js_defglobal(J, "Infinity", JS_READONLY | JS_DONTENUM | JS_DONTCONF);

	js_pushundefined(J);
	js_defglobal(J, "undefined", JS_READONLY | JS_DONTENUM | JS_DONTCONF);

	jsB_globalf(J, "parseInt", jsB_parseInt, 1);
	jsB_globalf(J, "parseFloat", jsB_parseFloat, 1);
	jsB_globalf(J, "isNaN", jsB_isNaN, 1);
	jsB_globalf(J, "isFinite", jsB_isFinite, 1);

	jsB_globalf(J, "decodeURI", jsB_decodeURI, 1);
	jsB_globalf(J, "decodeURIComponent", jsB_decodeURIComponent, 1);
	jsB_globalf(J, "encodeURI", jsB_encodeURI, 1);
	jsB_globalf(J, "encodeURIComponent", jsB_encodeURIComponent, 1);
}

#define cexp jsC_cexp

#define JF js_State *J, js_Function *F

JS_NORETURN void jsC_error(js_State *J, js_Ast *node, const char *fmt, ...) JS_PRINTFLIKE(3,4);

static void cfunbody(JF, js_Ast *name, js_Ast *params, js_Ast *body, int is_fun_exp);
static void cexp(JF, js_Ast *exp);
static void cstmlist(JF, js_Ast *list);
static void cstm(JF, js_Ast *stm);

void jsC_error(js_State *J, js_Ast *node, const char *fmt, ...)
{
	va_list ap;
	char buf[512];
	char msgbuf[256];

	va_start(ap, fmt);
	vsnprintf(msgbuf, 256, fmt, ap);
	va_end(ap);

	snprintf(buf, 256, "%s:%d: ", J->filename, node->line);
	strcat(buf, msgbuf);

	js_newsyntaxerror(J, buf);
	js_throw(J);
}

static const char *futurewords[] = {
	"class", "const", "enum", "export", "extends", "import", "super",
};

static const char *strictfuturewords[] = {
	"implements", "interface", "let", "package", "private", "protected",
	"public", "static", "yield",
};

static void checkfutureword(JF, js_Ast *exp)
{
	if (jsY_findword(exp->string, futurewords, nelem(futurewords)) >= 0)
		jsC_error(J, exp, "'%s' is a future reserved word", exp->string);
	if (F->strict) {
		if (jsY_findword(exp->string, strictfuturewords, nelem(strictfuturewords)) >= 0)
			jsC_error(J, exp, "'%s' is a strict mode future reserved word", exp->string);
	}
}

static js_Function *newfun(js_State *J, int line, js_Ast *name, js_Ast *params, js_Ast *body, int script, int default_strict, int is_fun_exp)
{
	js_Function *F = js_malloc(J, sizeof *F);
	memset(F, 0, sizeof *F);
	F->gcmark = 0;
	F->gcnext = J->gcfun;
	J->gcfun = F;
	++J->gccounter;

	F->filename = js_intern(J, J->filename);
	F->line = line;
	F->script = script;
	F->strict = default_strict;
	F->name = name ? name->string : "";

	cfunbody(J, F, name, params, body, is_fun_exp);

	return F;
}

static void emitraw(JF, int value)
{
	if (value != (js_Instruction)value)
		js_syntaxerror(J, "integer overflow in instruction coding");
	if (F->codelen >= F->codecap) {
		F->codecap = F->codecap ? F->codecap * 2 : 64;
		F->code = js_realloc(J, F->code, F->codecap * sizeof *F->code);
	}
	F->code[F->codelen++] = value;
}

static void emit(JF, int value)
{
	emitraw(J, F, F->lastline);
	emitraw(J, F, value);
}

static void emitarg(JF, int value)
{
	emitraw(J, F, value);
}

static void emitline(JF, js_Ast *node)
{
	F->lastline = node->line;
}

static int addfunction(JF, js_Function *value)
{
	if (F->funlen >= F->funcap) {
		F->funcap = F->funcap ? F->funcap * 2 : 16;
		F->funtab = js_realloc(J, F->funtab, F->funcap * sizeof *F->funtab);
	}
	F->funtab[F->funlen] = value;
	return F->funlen++;
}

static int addlocal(JF, js_Ast *ident, int reuse)
{
	const char *name = ident->string;
	if (F->strict) {
		if (!strcmp(name, "arguments"))
			jsC_error(J, ident, "redefining 'arguments' is not allowed in strict mode");
		if (!strcmp(name, "eval"))
			jsC_error(J, ident, "redefining 'eval' is not allowed in strict mode");
	} else {
		if (!strcmp(name, "eval"))
			js_evalerror(J, "%s:%d: invalid use of 'eval'", J->filename, ident->line);
	}
	if (reuse || F->strict) {
		int i;
		for (i = 0; i < F->varlen; ++i) {
			if (!strcmp(F->vartab[i], name)) {
				if (reuse)
					return i+1;
				if (F->strict)
					jsC_error(J, ident, "duplicate formal parameter '%s'", name);
			}
		}
	}
	if (F->varlen >= F->varcap) {
		F->varcap = F->varcap ? F->varcap * 2 : 16;
		F->vartab = js_realloc(J, F->vartab, F->varcap * sizeof *F->vartab);
	}
	F->vartab[F->varlen] = name;
	return ++F->varlen;
}

static int findlocal(JF, const char *name)
{
	int i;
	for (i = F->varlen; i > 0; --i)
		if (!strcmp(F->vartab[i-1], name))
			return i;
	return -1;
}

static void emitfunction(JF, js_Function *fun)
{
	F->lightweight = 0;
	emit(J, F, OP_CLOSURE);
	emitarg(J, F, addfunction(J, F, fun));
}

static void emitnumber(JF, double num)
{
	if (num == 0) {
		emit(J, F, OP_INTEGER);
		emitarg(J, F, 32768);
		if (signbit(num))
			emit(J, F, OP_NEG);
	} else if (num >= SHRT_MIN && num <= SHRT_MAX && num == (int)num) {
		emit(J, F, OP_INTEGER);
		emitarg(J, F, num + 32768);
	} else {
#define N (sizeof(num) / sizeof(js_Instruction))
		js_Instruction x[N];
		size_t i;
		emit(J, F, OP_NUMBER);
		memcpy(x, &num, sizeof(num));
		for (i = 0; i < N; ++i)
			emitarg(J, F, x[i]);
#undef N
	}
}

static void emitstring(JF, int opcode, const char *str)
{
#define N (sizeof(str) / sizeof(js_Instruction))
	js_Instruction x[N];
	size_t i;
	emit(J, F, opcode);
	memcpy(x, &str, sizeof(str));
	for (i = 0; i < N; ++i)
		emitarg(J, F, x[i]);
#undef N
}

static void emitlocal(JF, int oploc, int opvar, js_Ast *ident)
{
	int is_arguments = !strcmp(ident->string, "arguments");
	int is_eval = !strcmp(ident->string, "eval");
	int i;

	if (is_arguments) {
		F->lightweight = 0;
		F->arguments = 1;
	}

	checkfutureword(J, F, ident);
	if (F->strict && oploc == OP_SETLOCAL) {
		if (is_arguments)
			jsC_error(J, ident, "'arguments' is read-only in strict mode");
		if (is_eval)
			jsC_error(J, ident, "'eval' is read-only in strict mode");
	}
	if (is_eval)
		js_evalerror(J, "%s:%d: invalid use of 'eval'", J->filename, ident->line);

	i = findlocal(J, F, ident->string);
	if (i < 0) {
		emitstring(J, F, opvar, ident->string);
	} else {
		emit(J, F, oploc);
		emitarg(J, F, i);
	}
}

static int here(JF)
{
	return F->codelen;
}

static int emitjump(JF, int opcode)
{
	int inst;
	emit(J, F, opcode);
	inst = F->codelen;
	emitarg(J, F, 0);
	return inst;
}

static void emitjumpto(JF, int opcode, int dest)
{
	emit(J, F, opcode);
	if (dest != (js_Instruction)dest)
		js_syntaxerror(J, "jump address integer overflow");
	emitarg(J, F, dest);
}

static void labelto(JF, int inst, int addr)
{
	if (addr != (js_Instruction)addr)
		js_syntaxerror(J, "jump address integer overflow");
	F->code[inst] = addr;
}

static void label(JF, int inst)
{
	labelto(J, F, inst, F->codelen);
}

static void ctypeof(JF, js_Ast *exp)
{
	if (exp->a->type == EXP_IDENTIFIER) {
		emitline(J, F, exp->a);
		emitlocal(J, F, OP_GETLOCAL, OP_HASVAR, exp->a);
	} else {
		cexp(J, F, exp->a);
	}
	emitline(J, F, exp);
	emit(J, F, OP_TYPEOF);
}

static void cunary(JF, js_Ast *exp, int opcode)
{
	cexp(J, F, exp->a);
	emitline(J, F, exp);
	emit(J, F, opcode);
}

static void cbinary(JF, js_Ast *exp, int opcode)
{
	cexp(J, F, exp->a);
	cexp(J, F, exp->b);
	emitline(J, F, exp);
	emit(J, F, opcode);
}

static void carray(JF, js_Ast *list)
{
	while (list) {
		emitline(J, F, list->a);
		if (list->a->type == EXP_ELISION) {
			emit(J, F, OP_SKIPARRAY);
		} else {
			cexp(J, F, list->a);
			emit(J, F, OP_INITARRAY);
		}
		list = list->b;
	}
}

static void checkdup(JF, js_Ast *list, js_Ast *end)
{
	char nbuf[32], sbuf[32];
	const char *needle, *straw;

	if (end->a->type == EXP_NUMBER)
		needle = jsV_numbertostring(J, nbuf, end->a->number);
	else
		needle = end->a->string;

	while (list->a != end) {
		if (list->a->type == end->type) {
			js_Ast *prop = list->a->a;
			if (prop->type == EXP_NUMBER)
				straw = jsV_numbertostring(J, sbuf, prop->number);
			else
				straw =  prop->string;
			if (!strcmp(needle, straw))
				jsC_error(J, list, "duplicate property '%s' in object literal", needle);
		}
		list = list->b;
	}
}

static void cobject(JF, js_Ast *list)
{
	js_Ast *head = list;

	while (list) {
		js_Ast *kv = list->a;
		js_Ast *prop = kv->a;

		if (prop->type == AST_IDENTIFIER || prop->type == EXP_STRING) {
			emitline(J, F, prop);
			emitstring(J, F, OP_STRING, prop->string);
		} else if (prop->type == EXP_NUMBER) {
			emitline(J, F, prop);
			emitnumber(J, F, prop->number);
		} else {
			jsC_error(J, prop, "invalid property name in object initializer");
		}

		if (F->strict)
			checkdup(J, F, head, kv);

		switch (kv->type) {
		default:  break;
		case EXP_PROP_VAL:
			cexp(J, F, kv->b);
			emitline(J, F, kv);
			emit(J, F, OP_INITPROP);
			break;
		case EXP_PROP_GET:
			emitfunction(J, F, newfun(J, prop->line, NULL, NULL, kv->c, 0, F->strict, 1));
			emitline(J, F, kv);
			emit(J, F, OP_INITGETTER);
			break;
		case EXP_PROP_SET:
			emitfunction(J, F, newfun(J, prop->line, NULL, kv->b, kv->c, 0, F->strict, 1));
			emitline(J, F, kv);
			emit(J, F, OP_INITSETTER);
			break;
		}

		list = list->b;
	}
}

static int cargs(JF, js_Ast *list)
{
	int n = 0;
	while (list) {
		cexp(J, F, list->a);
		list = list->b;
		++n;
	}
	return n;
}

static void cassign(JF, js_Ast *exp)
{
	js_Ast *lhs = exp->a;
	js_Ast *rhs = exp->b;
	switch (lhs->type) {
	case EXP_IDENTIFIER:
		cexp(J, F, rhs);
		emitline(J, F, exp);
		emitlocal(J, F, OP_SETLOCAL, OP_SETVAR, lhs);
		break;
	case EXP_INDEX:
		cexp(J, F, lhs->a);
		cexp(J, F, lhs->b);
		cexp(J, F, rhs);
		emitline(J, F, exp);
		emit(J, F, OP_SETPROP);
		break;
	case EXP_MEMBER:
		cexp(J, F, lhs->a);
		cexp(J, F, rhs);
		emitline(J, F, exp);
		emitstring(J, F, OP_SETPROP_S, lhs->b->string);
		break;
	default:
		jsC_error(J, lhs, "invalid l-value in assignment");
	}
}

static void cassignforin(JF, js_Ast *stm)
{
	js_Ast *lhs = stm->a;

	if (stm->type == STM_FOR_IN_VAR) {
		if (lhs->b)
			jsC_error(J, lhs->b, "more than one loop variable in for-in statement");
		emitline(J, F, lhs->a);
		emitlocal(J, F, OP_SETLOCAL, OP_SETVAR, lhs->a->a);
		emit(J, F, OP_POP);
		return;
	}

	switch (lhs->type) {
	case EXP_IDENTIFIER:
		emitline(J, F, lhs);
		emitlocal(J, F, OP_SETLOCAL, OP_SETVAR, lhs);
		emit(J, F, OP_POP);
		break;
	case EXP_INDEX:
		cexp(J, F, lhs->a);
		cexp(J, F, lhs->b);
		emitline(J, F, lhs);
		emit(J, F, OP_ROT3);
		emit(J, F, OP_SETPROP);
		emit(J, F, OP_POP);
		break;
	case EXP_MEMBER:
		cexp(J, F, lhs->a);
		emitline(J, F, lhs);
		emit(J, F, OP_ROT2);
		emitstring(J, F, OP_SETPROP_S, lhs->b->string);
		emit(J, F, OP_POP);
		break;
	default:
		jsC_error(J, lhs, "invalid l-value in for-in loop assignment");
	}
}

static void cassignop1(JF, js_Ast *lhs)
{
	switch (lhs->type) {
	case EXP_IDENTIFIER:
		emitline(J, F, lhs);
		emitlocal(J, F, OP_GETLOCAL, OP_GETVAR, lhs);
		break;
	case EXP_INDEX:
		cexp(J, F, lhs->a);
		cexp(J, F, lhs->b);
		emitline(J, F, lhs);
		emit(J, F, OP_DUP2);
		emit(J, F, OP_GETPROP);
		break;
	case EXP_MEMBER:
		cexp(J, F, lhs->a);
		emitline(J, F, lhs);
		emit(J, F, OP_DUP);
		emitstring(J, F, OP_GETPROP_S, lhs->b->string);
		break;
	default:
		jsC_error(J, lhs, "invalid l-value in assignment");
	}
}

static void cassignop2(JF, js_Ast *lhs, int postfix)
{
	switch (lhs->type) {
	case EXP_IDENTIFIER:
		emitline(J, F, lhs);
		if (postfix) emit(J, F, OP_ROT2);
		emitlocal(J, F, OP_SETLOCAL, OP_SETVAR, lhs);
		break;
	case EXP_INDEX:
		emitline(J, F, lhs);
		if (postfix) emit(J, F, OP_ROT4);
		emit(J, F, OP_SETPROP);
		break;
	case EXP_MEMBER:
		emitline(J, F, lhs);
		if (postfix) emit(J, F, OP_ROT3);
		emitstring(J, F, OP_SETPROP_S, lhs->b->string);
		break;
	default:
		jsC_error(J, lhs, "invalid l-value in assignment");
	}
}

static void cassignop(JF, js_Ast *exp, int opcode)
{
	js_Ast *lhs = exp->a;
	js_Ast *rhs = exp->b;
	cassignop1(J, F, lhs);
	cexp(J, F, rhs);
	emitline(J, F, exp);
	emit(J, F, opcode);
	cassignop2(J, F, lhs, 0);
}

static void cdelete(JF, js_Ast *exp)
{
	js_Ast *arg = exp->a;
	switch (arg->type) {
	case EXP_IDENTIFIER:
		if (F->strict)
			jsC_error(J, exp, "delete on an unqualified name is not allowed in strict mode");
		emitline(J, F, exp);
		emitlocal(J, F, OP_DELLOCAL, OP_DELVAR, arg);
		break;
	case EXP_INDEX:
		cexp(J, F, arg->a);
		cexp(J, F, arg->b);
		emitline(J, F, exp);
		emit(J, F, OP_DELPROP);
		break;
	case EXP_MEMBER:
		cexp(J, F, arg->a);
		emitline(J, F, exp);
		emitstring(J, F, OP_DELPROP_S, arg->b->string);
		break;
	default:
		jsC_error(J, exp, "invalid l-value in delete expression");
	}
}

static void ceval(JF, js_Ast *fun, js_Ast *args)
{
	int n = cargs(J, F, args);
	F->lightweight = 0;
	F->arguments = 1;
	if (n == 0)
		emit(J, F, OP_UNDEF);
	else while (n-- > 1)
		emit(J, F, OP_POP);
	emit(J, F, OP_EVAL);
}

static void ccall(JF, js_Ast *fun, js_Ast *args)
{
	int n;
	switch (fun->type) {
	case EXP_INDEX:
		cexp(J, F, fun->a);
		emit(J, F, OP_DUP);
		cexp(J, F, fun->b);
		emit(J, F, OP_GETPROP);
		emit(J, F, OP_ROT2);
		break;
	case EXP_MEMBER:
		cexp(J, F, fun->a);
		emit(J, F, OP_DUP);
		emitstring(J, F, OP_GETPROP_S, fun->b->string);
		emit(J, F, OP_ROT2);
		break;
	case EXP_IDENTIFIER:
		if (!strcmp(fun->string, "eval")) {
			ceval(J, F, fun, args);
			return;
		}

	default:
		cexp(J, F, fun);
		emit(J, F, OP_UNDEF);
		break;
	}
	n = cargs(J, F, args);
	emit(J, F, OP_CALL);
	emitarg(J, F, n);
}

static void cexp(JF, js_Ast *exp)
{
	int then, end;
	int n;

	switch (exp->type) {
	case EXP_STRING:
		emitline(J, F, exp);
		emitstring(J, F, OP_STRING, exp->string);
		break;
	case EXP_NUMBER:
		emitline(J, F, exp);
		emitnumber(J, F, exp->number);
		break;
	case EXP_ELISION:
		break;
	case EXP_NULL:
		emitline(J, F, exp);
		emit(J, F, OP_NULL);
		break;
	case EXP_TRUE:
		emitline(J, F, exp);
		emit(J, F, OP_TRUE);
		break;
	case EXP_FALSE:
		emitline(J, F, exp);
		emit(J, F, OP_FALSE);
		break;
	case EXP_THIS:
		emitline(J, F, exp);
		emit(J, F, OP_THIS);
		break;

	case EXP_REGEXP:
		emitline(J, F, exp);
		emitstring(J, F, OP_NEWREGEXP, exp->string);
		emitarg(J, F, exp->number);
		break;

	case EXP_OBJECT:
		emitline(J, F, exp);
		emit(J, F, OP_NEWOBJECT);
		cobject(J, F, exp->a);
		break;

	case EXP_ARRAY:
		emitline(J, F, exp);
		emit(J, F, OP_NEWARRAY);
		carray(J, F, exp->a);
		break;

	case EXP_FUN:
		emitline(J, F, exp);
		emitfunction(J, F, newfun(J, exp->line, exp->a, exp->b, exp->c, 0, F->strict, 1));
		break;

	case EXP_IDENTIFIER:
		emitline(J, F, exp);
		emitlocal(J, F, OP_GETLOCAL, OP_GETVAR, exp);
		break;

	case EXP_INDEX:
		cexp(J, F, exp->a);
		cexp(J, F, exp->b);
		emitline(J, F, exp);
		emit(J, F, OP_GETPROP);
		break;

	case EXP_MEMBER:
		cexp(J, F, exp->a);
		emitline(J, F, exp);
		emitstring(J, F, OP_GETPROP_S, exp->b->string);
		break;

	case EXP_CALL:
		ccall(J, F, exp->a, exp->b);
		break;

	case EXP_NEW:
		cexp(J, F, exp->a);
		n = cargs(J, F, exp->b);
		emitline(J, F, exp);
		emit(J, F, OP_NEW);
		emitarg(J, F, n);
		break;

	case EXP_DELETE:
		cdelete(J, F, exp);
		break;

	case EXP_PREINC:
		cassignop1(J, F, exp->a);
		emitline(J, F, exp);
		emit(J, F, OP_INC);
		cassignop2(J, F, exp->a, 0);
		break;

	case EXP_PREDEC:
		cassignop1(J, F, exp->a);
		emitline(J, F, exp);
		emit(J, F, OP_DEC);
		cassignop2(J, F, exp->a, 0);
		break;

	case EXP_POSTINC:
		cassignop1(J, F, exp->a);
		emitline(J, F, exp);
		emit(J, F, OP_POSTINC);
		cassignop2(J, F, exp->a, 1);
		emit(J, F, OP_POP);
		break;

	case EXP_POSTDEC:
		cassignop1(J, F, exp->a);
		emitline(J, F, exp);
		emit(J, F, OP_POSTDEC);
		cassignop2(J, F, exp->a, 1);
		emit(J, F, OP_POP);
		break;

	case EXP_VOID:
		cexp(J, F, exp->a);
		emitline(J, F, exp);
		emit(J, F, OP_POP);
		emit(J, F, OP_UNDEF);
		break;

	case EXP_TYPEOF: ctypeof(J, F, exp); break;
	case EXP_POS: cunary(J, F, exp, OP_POS); break;
	case EXP_NEG: cunary(J, F, exp, OP_NEG); break;
	case EXP_BITNOT: cunary(J, F, exp, OP_BITNOT); break;
	case EXP_LOGNOT: cunary(J, F, exp, OP_LOGNOT); break;

	case EXP_BITOR: cbinary(J, F, exp, OP_BITOR); break;
	case EXP_BITXOR: cbinary(J, F, exp, OP_BITXOR); break;
	case EXP_BITAND: cbinary(J, F, exp, OP_BITAND); break;
	case EXP_EQ: cbinary(J, F, exp, OP_EQ); break;
	case EXP_NE: cbinary(J, F, exp, OP_NE); break;
	case EXP_STRICTEQ: cbinary(J, F, exp, OP_STRICTEQ); break;
	case EXP_STRICTNE: cbinary(J, F, exp, OP_STRICTNE); break;
	case EXP_LT: cbinary(J, F, exp, OP_LT); break;
	case EXP_GT: cbinary(J, F, exp, OP_GT); break;
	case EXP_LE: cbinary(J, F, exp, OP_LE); break;
	case EXP_GE: cbinary(J, F, exp, OP_GE); break;
	case EXP_INSTANCEOF: cbinary(J, F, exp, OP_INSTANCEOF); break;
	case EXP_IN: cbinary(J, F, exp, OP_IN); break;
	case EXP_SHL: cbinary(J, F, exp, OP_SHL); break;
	case EXP_SHR: cbinary(J, F, exp, OP_SHR); break;
	case EXP_USHR: cbinary(J, F, exp, OP_USHR); break;
	case EXP_ADD: cbinary(J, F, exp, OP_ADD); break;
	case EXP_SUB: cbinary(J, F, exp, OP_SUB); break;
	case EXP_MUL: cbinary(J, F, exp, OP_MUL); break;
	case EXP_DIV: cbinary(J, F, exp, OP_DIV); break;
	case EXP_MOD: cbinary(J, F, exp, OP_MOD); break;

	case EXP_ASS: cassign(J, F, exp); break;
	case EXP_ASS_MUL: cassignop(J, F, exp, OP_MUL); break;
	case EXP_ASS_DIV: cassignop(J, F, exp, OP_DIV); break;
	case EXP_ASS_MOD: cassignop(J, F, exp, OP_MOD); break;
	case EXP_ASS_ADD: cassignop(J, F, exp, OP_ADD); break;
	case EXP_ASS_SUB: cassignop(J, F, exp, OP_SUB); break;
	case EXP_ASS_SHL: cassignop(J, F, exp, OP_SHL); break;
	case EXP_ASS_SHR: cassignop(J, F, exp, OP_SHR); break;
	case EXP_ASS_USHR: cassignop(J, F, exp, OP_USHR); break;
	case EXP_ASS_BITAND: cassignop(J, F, exp, OP_BITAND); break;
	case EXP_ASS_BITXOR: cassignop(J, F, exp, OP_BITXOR); break;
	case EXP_ASS_BITOR: cassignop(J, F, exp, OP_BITOR); break;

	case EXP_COMMA:
		cexp(J, F, exp->a);
		emitline(J, F, exp);
		emit(J, F, OP_POP);
		cexp(J, F, exp->b);
		break;

	case EXP_LOGOR:
		cexp(J, F, exp->a);
		emitline(J, F, exp);
		emit(J, F, OP_DUP);
		end = emitjump(J, F, OP_JTRUE);
		emit(J, F, OP_POP);
		cexp(J, F, exp->b);
		label(J, F, end);
		break;

	case EXP_LOGAND:
		cexp(J, F, exp->a);
		emitline(J, F, exp);
		emit(J, F, OP_DUP);
		end = emitjump(J, F, OP_JFALSE);
		emit(J, F, OP_POP);
		cexp(J, F, exp->b);
		label(J, F, end);
		break;

	case EXP_COND:
		cexp(J, F, exp->a);
		emitline(J, F, exp);
		then = emitjump(J, F, OP_JTRUE);
		cexp(J, F, exp->c);
		end = emitjump(J, F, OP_JUMP);
		label(J, F, then);
		cexp(J, F, exp->b);
		label(J, F, end);
		break;

	default:
		jsC_error(J, exp, "unknown expression type");
	}
}

static void addjump(JF, enum js_AstType type, js_Ast *target, int inst)
{
	js_JumpList *jump = js_malloc(J, sizeof *jump);
	jump->type = type;
	jump->inst = inst;
	jump->next = target->jumps;
	target->jumps = jump;
}

static void labeljumps(JF, js_Ast *stm, int baddr, int caddr)
{
	js_JumpList *jump = stm->jumps;
	while (jump) {
		js_JumpList *next = jump->next;
		if (jump->type == STM_BREAK)
			labelto(J, F, jump->inst, baddr);
		if (jump->type == STM_CONTINUE)
			labelto(J, F, jump->inst, caddr);
		js_free(J, jump);
		jump = next;
	}
	stm->jumps = NULL;
}

static int isloop(enum js_AstType T)
{
	return T == STM_DO || T == STM_WHILE ||
		T == STM_FOR || T == STM_FOR_VAR ||
		T == STM_FOR_IN || T == STM_FOR_IN_VAR;
}

static int isfun(enum js_AstType T)
{
	return T == AST_FUNDEC || T == EXP_FUN || T == EXP_PROP_GET || T == EXP_PROP_SET;
}

static int matchlabel(js_Ast *node, const char *label)
{
	while (node && node->type == STM_LABEL) {
		if (!strcmp(node->a->string, label))
			return 1;
		node = node->parent;
	}
	return 0;
}

static js_Ast *breaktarget(JF, js_Ast *node, const char *label)
{
	while (node) {
		if (isfun(node->type))
			break;
		if (!label) {
			if (isloop(node->type) || node->type == STM_SWITCH)
				return node;
		} else {
			if (matchlabel(node->parent, label))
				return node;
		}
		node = node->parent;
	}
	return NULL;
}

static js_Ast *continuetarget(JF, js_Ast *node, const char *label)
{
	while (node) {
		if (isfun(node->type))
			break;
		if (isloop(node->type)) {
			if (!label)
				return node;
			else if (matchlabel(node->parent, label))
				return node;
		}
		node = node->parent;
	}
	return NULL;
}

static js_Ast *returntarget(JF, js_Ast *node)
{
	while (node) {
		if (isfun(node->type))
			return node;
		node = node->parent;
	}
	return NULL;
}

static void cexit(JF, enum js_AstType T, js_Ast *node, js_Ast *target)
{
	js_Ast *prev;
	do {
		prev = node, node = node->parent;
		switch (node->type) {
		default:

			break;
		case STM_WITH:
			emitline(J, F, node);
			emit(J, F, OP_ENDWITH);
			break;
		case STM_FOR_IN:
		case STM_FOR_IN_VAR:
			emitline(J, F, node);

			if (F->script) {
				if (T == STM_RETURN || T == STM_BREAK || (T == STM_CONTINUE && target != node)) {

					emit(J, F, OP_ROT2);
					emit(J, F, OP_POP);
				}
				if (T == STM_CONTINUE)
					emit(J, F, OP_ROT2);
			} else {
				if (T == STM_RETURN) {

					emit(J, F, OP_ROT2);
					emit(J, F, OP_POP);
				}
				if (T == STM_BREAK || (T == STM_CONTINUE && target != node))
					emit(J, F, OP_POP);
			}
			break;
		case STM_TRY:
			emitline(J, F, node);

			if (prev == node->a) {
				emit(J, F, OP_ENDTRY);
				if (node->d) cstm(J, F, node->d);
			}

			if (prev == node->c) {

				if (node->d) {
					emit(J, F, OP_ENDCATCH);
					emit(J, F, OP_ENDTRY);
					cstm(J, F, node->d);
				} else {
					emit(J, F, OP_ENDCATCH);
				}
			}
			break;
		}
	} while (node != target);
}

static void ctryfinally(JF, js_Ast *trystm, js_Ast *finallystm)
{
	int L1;
	L1 = emitjump(J, F, OP_TRY);
	{

		cstm(J, F, finallystm);
		emit(J, F, OP_THROW);
	}
	label(J, F, L1);
	cstm(J, F, trystm);
	emit(J, F, OP_ENDTRY);
	cstm(J, F, finallystm);
}

static void ctrycatch(JF, js_Ast *trystm, js_Ast *catchvar, js_Ast *catchstm)
{
	int L1, L2;
	L1 = emitjump(J, F, OP_TRY);
	{

		checkfutureword(J, F, catchvar);
		if (F->strict) {
			if (!strcmp(catchvar->string, "arguments"))
				jsC_error(J, catchvar, "redefining 'arguments' is not allowed in strict mode");
			if (!strcmp(catchvar->string, "eval"))
				jsC_error(J, catchvar, "redefining 'eval' is not allowed in strict mode");
		}
		emitline(J, F, catchvar);
		emitstring(J, F, OP_CATCH, catchvar->string);
		cstm(J, F, catchstm);
		emit(J, F, OP_ENDCATCH);
		L2 = emitjump(J, F, OP_JUMP);
	}
	label(J, F, L1);
	cstm(J, F, trystm);
	emit(J, F, OP_ENDTRY);
	label(J, F, L2);
}

static void ctrycatchfinally(JF, js_Ast *trystm, js_Ast *catchvar, js_Ast *catchstm, js_Ast *finallystm)
{
	int L1, L2, L3;
	L1 = emitjump(J, F, OP_TRY);
	{

		L2 = emitjump(J, F, OP_TRY);
		{

			cstm(J, F, finallystm);
			emit(J, F, OP_THROW);
		}
		label(J, F, L2);
		if (F->strict) {
			checkfutureword(J, F, catchvar);
			if (!strcmp(catchvar->string, "arguments"))
				jsC_error(J, catchvar, "redefining 'arguments' is not allowed in strict mode");
			if (!strcmp(catchvar->string, "eval"))
				jsC_error(J, catchvar, "redefining 'eval' is not allowed in strict mode");
		}
		emitline(J, F, catchvar);
		emitstring(J, F, OP_CATCH, catchvar->string);
		cstm(J, F, catchstm);
		emit(J, F, OP_ENDCATCH);
		emit(J, F, OP_ENDTRY);
		L3 = emitjump(J, F, OP_JUMP);
	}
	label(J, F, L1);
	cstm(J, F, trystm);
	emit(J, F, OP_ENDTRY);
	label(J, F, L3);
	cstm(J, F, finallystm);
}

static void cswitch(JF, js_Ast *ref, js_Ast *head)
{
	js_Ast *node, *clause, *def = NULL;
	int end;

	cexp(J, F, ref);

	for (node = head; node; node = node->b) {
		clause = node->a;
		if (clause->type == STM_DEFAULT) {
			if (def)
				jsC_error(J, clause, "more than one default label in switch");
			def = clause;
		} else {
			cexp(J, F, clause->a);
			emitline(J, F, clause);
			clause->casejump = emitjump(J, F, OP_JCASE);
		}
	}
	emit(J, F, OP_POP);
	if (def) {
		emitline(J, F, def);
		def->casejump = emitjump(J, F, OP_JUMP);
		end = 0;
	} else {
		end = emitjump(J, F, OP_JUMP);
	}

	for (node = head; node; node = node->b) {
		clause = node->a;
		label(J, F, clause->casejump);
		if (clause->type == STM_DEFAULT)
			cstmlist(J, F, clause->a);
		else
			cstmlist(J, F, clause->b);
	}

	if (end)
		label(J, F, end);
}

static void cvarinit(JF, js_Ast *list)
{
	while (list) {
		js_Ast *var = list->a;
		if (var->b) {
			cexp(J, F, var->b);
			emitline(J, F, var);
			emitlocal(J, F, OP_SETLOCAL, OP_SETVAR, var->a);
			emit(J, F, OP_POP);
		}
		list = list->b;
	}
}

static void cstm(JF, js_Ast *stm)
{
	js_Ast *target;
	int loop, cont, then, end;

	emitline(J, F, stm);

	switch (stm->type) {
	case AST_FUNDEC:
		break;

	case STM_BLOCK:
		cstmlist(J, F, stm->a);
		break;

	case STM_EMPTY:
		if (F->script) {
			emitline(J, F, stm);
			emit(J, F, OP_POP);
			emit(J, F, OP_UNDEF);
		}
		break;

	case STM_VAR:
		cvarinit(J, F, stm->a);
		break;

	case STM_IF:
		if (stm->c) {
			cexp(J, F, stm->a);
			emitline(J, F, stm);
			then = emitjump(J, F, OP_JTRUE);
			cstm(J, F, stm->c);
			emitline(J, F, stm);
			end = emitjump(J, F, OP_JUMP);
			label(J, F, then);
			cstm(J, F, stm->b);
			label(J, F, end);
		} else {
			cexp(J, F, stm->a);
			emitline(J, F, stm);
			end = emitjump(J, F, OP_JFALSE);
			cstm(J, F, stm->b);
			label(J, F, end);
		}
		break;

	case STM_DO:
		loop = here(J, F);
		cstm(J, F, stm->a);
		cont = here(J, F);
		cexp(J, F, stm->b);
		emitline(J, F, stm);
		emitjumpto(J, F, OP_JTRUE, loop);
		labeljumps(J, F, stm, here(J,F), cont);
		break;

	case STM_WHILE:
		loop = here(J, F);
		cexp(J, F, stm->a);
		emitline(J, F, stm);
		end = emitjump(J, F, OP_JFALSE);
		cstm(J, F, stm->b);
		emitline(J, F, stm);
		emitjumpto(J, F, OP_JUMP, loop);
		label(J, F, end);
		labeljumps(J, F, stm, here(J,F), loop);
		break;

	case STM_FOR:
	case STM_FOR_VAR:
		if (stm->type == STM_FOR_VAR) {
			cvarinit(J, F, stm->a);
		} else {
			if (stm->a) {
				cexp(J, F, stm->a);
				emit(J, F, OP_POP);
			}
		}
		loop = here(J, F);
		if (stm->b) {
			cexp(J, F, stm->b);
			emitline(J, F, stm);
			end = emitjump(J, F, OP_JFALSE);
		} else {
			end = 0;
		}
		cstm(J, F, stm->d);
		cont = here(J, F);
		if (stm->c) {
			cexp(J, F, stm->c);
			emit(J, F, OP_POP);
		}
		emitline(J, F, stm);
		emitjumpto(J, F, OP_JUMP, loop);
		if (end)
			label(J, F, end);
		labeljumps(J, F, stm, here(J,F), cont);
		break;

	case STM_FOR_IN:
	case STM_FOR_IN_VAR:
		cexp(J, F, stm->b);
		emitline(J, F, stm);
		emit(J, F, OP_ITERATOR);
		loop = here(J, F);
		{
			emitline(J, F, stm);
			emit(J, F, OP_NEXTITER);
			end = emitjump(J, F, OP_JFALSE);
			cassignforin(J, F, stm);
			if (F->script) {
				emit(J, F, OP_ROT2);
				cstm(J, F, stm->c);
				emit(J, F, OP_ROT2);
			} else {
				cstm(J, F, stm->c);
			}
			emitline(J, F, stm);
			emitjumpto(J, F, OP_JUMP, loop);
		}
		label(J, F, end);
		labeljumps(J, F, stm, here(J,F), loop);
		break;

	case STM_SWITCH:
		cswitch(J, F, stm->a, stm->b);
		labeljumps(J, F, stm, here(J,F), 0);
		break;

	case STM_LABEL:
		cstm(J, F, stm->b);

		while (stm->type == STM_LABEL)
			stm = stm->b;

		if (!isloop(stm->type) && stm->type != STM_SWITCH)
			labeljumps(J, F, stm, here(J,F), 0);
		break;

	case STM_BREAK:
		if (stm->a) {
			checkfutureword(J, F, stm->a);
			target = breaktarget(J, F, stm->parent, stm->a->string);
			if (!target)
				jsC_error(J, stm, "break label '%s' not found", stm->a->string);
		} else {
			target = breaktarget(J, F, stm->parent, NULL);
			if (!target)
				jsC_error(J, stm, "unlabelled break must be inside loop or switch");
		}
		cexit(J, F, STM_BREAK, stm, target);
		emitline(J, F, stm);
		addjump(J, F, STM_BREAK, target, emitjump(J, F, OP_JUMP));
		break;

	case STM_CONTINUE:
		if (stm->a) {
			checkfutureword(J, F, stm->a);
			target = continuetarget(J, F, stm->parent, stm->a->string);
			if (!target)
				jsC_error(J, stm, "continue label '%s' not found", stm->a->string);
		} else {
			target = continuetarget(J, F, stm->parent, NULL);
			if (!target)
				jsC_error(J, stm, "continue must be inside loop");
		}
		cexit(J, F, STM_CONTINUE, stm, target);
		emitline(J, F, stm);
		addjump(J, F, STM_CONTINUE, target, emitjump(J, F, OP_JUMP));
		break;

	case STM_RETURN:
		if (stm->a)
			cexp(J, F, stm->a);
		else
			emit(J, F, OP_UNDEF);
		target = returntarget(J, F, stm->parent);
		if (!target)
			jsC_error(J, stm, "return not in function");
		cexit(J, F, STM_RETURN, stm, target);
		emitline(J, F, stm);
		emit(J, F, OP_RETURN);
		break;

	case STM_THROW:
		cexp(J, F, stm->a);
		emitline(J, F, stm);
		emit(J, F, OP_THROW);
		break;

	case STM_WITH:
		F->lightweight = 0;
		if (F->strict)
			jsC_error(J, stm->a, "'with' statements are not allowed in strict mode");
		cexp(J, F, stm->a);
		emitline(J, F, stm);
		emit(J, F, OP_WITH);
		cstm(J, F, stm->b);
		emitline(J, F, stm);
		emit(J, F, OP_ENDWITH);
		break;

	case STM_TRY:
		emitline(J, F, stm);
		if (stm->b && stm->c) {
			F->lightweight = 0;
			if (stm->d)
				ctrycatchfinally(J, F, stm->a, stm->b, stm->c, stm->d);
			else
				ctrycatch(J, F, stm->a, stm->b, stm->c);
		} else {
			ctryfinally(J, F, stm->a, stm->d);
		}
		break;

	case STM_DEBUGGER:
		emitline(J, F, stm);
		emit(J, F, OP_DEBUGGER);
		break;

	default:
		if (F->script) {
			emitline(J, F, stm);
			emit(J, F, OP_POP);
			cexp(J, F, stm);
		} else {
			cexp(J, F, stm);
			emitline(J, F, stm);
			emit(J, F, OP_POP);
		}
		break;
	}
}

static void cstmlist(JF, js_Ast *list)
{
	while (list) {
		cstm(J, F, list->a);
		list = list->b;
	}
}

static int listlength(js_Ast *list)
{
	int n = 0;
	while (list) ++n, list = list->b;
	return n;
}

static void cparams(JF, js_Ast *list, js_Ast *fname)
{
	F->numparams = listlength(list);
	while (list) {
		checkfutureword(J, F, list->a);
		addlocal(J, F, list->a, 0);
		list = list->b;
	}
}

static void cvardecs(JF, js_Ast *node)
{
	if (node->type == AST_LIST) {
		while (node) {
			cvardecs(J, F, node->a);
			node = node->b;
		}
		return;
	}

	if (isfun(node->type))
		return;

	if (node->type == EXP_VAR) {
		checkfutureword(J, F, node->a);
		addlocal(J, F, node->a, 1);
	}

	if (node->a) cvardecs(J, F, node->a);
	if (node->b) cvardecs(J, F, node->b);
	if (node->c) cvardecs(J, F, node->c);
	if (node->d) cvardecs(J, F, node->d);
}

static void cfundecs(JF, js_Ast *list)
{
	while (list) {
		js_Ast *stm = list->a;
		if (stm->type == AST_FUNDEC) {
			emitline(J, F, stm);
			emitfunction(J, F, newfun(J, stm->line, stm->a, stm->b, stm->c, 0, F->strict, 0));
			emitline(J, F, stm);
			emit(J, F, OP_SETLOCAL);
			emitarg(J, F, addlocal(J, F, stm->a, 1));
			emit(J, F, OP_POP);
		}
		list = list->b;
	}
}

static void cfunbody(JF, js_Ast *name, js_Ast *params, js_Ast *body, int is_fun_exp)
{
	F->lightweight = 1;
	F->arguments = 0;

	if (F->script)
		F->lightweight = 0;

	if (body && body->type == AST_LIST && body->a && body->a->type == EXP_STRING)
		if (!strcmp(body->a->string, "use strict"))
			F->strict = 1;

	F->lastline = F->line;

	cparams(J, F, params, name);

	if (body) {
		cvardecs(J, F, body);
		cfundecs(J, F, body);
	}

	if (name) {
		checkfutureword(J, F, name);
		if (is_fun_exp) {
			if (findlocal(J, F, name->string) < 0) {

				emit(J, F, OP_CURRENT);
				emit(J, F, OP_SETLOCAL);
				emitarg(J, F, addlocal(J, F, name, 1));
				emit(J, F, OP_POP);
			}
		}
	}

	if (F->script) {
		emit(J, F, OP_UNDEF);
		cstmlist(J, F, body);
		emit(J, F, OP_RETURN);
	} else {
		cstmlist(J, F, body);
		emit(J, F, OP_UNDEF);
		emit(J, F, OP_RETURN);
	}
}

js_Function *jsC_compilefunction(js_State *J, js_Ast *prog)
{
	return newfun(J, prog->line, prog->a, prog->b, prog->c, 0, J->default_strict, 1);
}

js_Function *jsC_compilescript(js_State *J, js_Ast *prog, int default_strict)
{
	return newfun(J, prog ? prog->line : 0, NULL, NULL, prog, 1, default_strict, 0);
}

#include <time.h>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/time.h>
#elif defined(_WIN32)
#include <sys/timeb.h>
#endif

#define js_optnumber(J,I,V) (js_isdefined(J,I) ? js_tonumber(J,I) : V)

static double Now(void)
{
#if defined(__unix__) || defined(__APPLE__)
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return floor(tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0);
#elif defined(_WIN32)
	struct _timeb tv;
	_ftime(&tv);
	return tv.time * 1000.0 + tv.millitm;
#else
	return time(NULL) * 1000.0;
#endif
}

static double LocalTZA(void)
{
	static int once = 1;
	static double tza = 0;
	if (once) {
		time_t now = time(NULL);
		time_t utc = mktime(gmtime(&now));
		time_t loc = mktime(localtime(&now));
		tza = (loc - utc) * 1000;
		once = 0;
	}
	return tza;
}

static double DaylightSavingTA(double t)
{
	return 0;
}

#define HoursPerDay		24.0
#define MinutesPerDay		(HoursPerDay * MinutesPerHour)
#define MinutesPerHour		60.0
#define SecondsPerDay		(MinutesPerDay * SecondsPerMinute)
#define SecondsPerHour		(MinutesPerHour * SecondsPerMinute)
#define SecondsPerMinute	60.0

#define msPerDay	(SecondsPerDay * msPerSecond)
#define msPerHour	(SecondsPerHour * msPerSecond)
#define msPerMinute	(SecondsPerMinute * msPerSecond)
#define msPerSecond	1000.0

static double pmod(double x, double y)
{
	x = fmod(x, y);
	if (x < 0)
		x += y;
	return x;
}

static int Day(double t)
{
	return floor(t / msPerDay);
}

static double TimeWithinDay(double t)
{
	return pmod(t, msPerDay);
}

static int DaysInYear(int y)
{
	return y % 4 == 0 && (y % 100 || (y % 400 == 0)) ? 366 : 365;
}

static int DayFromYear(int y)
{
	return 365 * (y - 1970) +
		floor((y - 1969) / 4.0) -
		floor((y - 1901) / 100.0) +
		floor((y - 1601) / 400.0);
}

static double TimeFromYear(int y)
{
	return DayFromYear(y) * msPerDay;
}

static int YearFromTime(double t)
{
	int y = floor(t / (msPerDay * 365.2425)) + 1970;
	double t2 = TimeFromYear(y);
	if (t2 > t)
		--y;
	else if (t2 + msPerDay * DaysInYear(y) <= t)
		++y;
	return y;
}

static int InLeapYear(double t)
{
	return DaysInYear(YearFromTime(t)) == 366;
}

static int DayWithinYear(double t)
{
	return Day(t) - DayFromYear(YearFromTime(t));
}

static int MonthFromTime(double t)
{
	int day = DayWithinYear(t);
	int leap = InLeapYear(t);
	if (day < 31) return 0;
	if (day < 59 + leap) return 1;
	if (day < 90 + leap) return 2;
	if (day < 120 + leap) return 3;
	if (day < 151 + leap) return 4;
	if (day < 181 + leap) return 5;
	if (day < 212 + leap) return 6;
	if (day < 243 + leap) return 7;
	if (day < 273 + leap) return 8;
	if (day < 304 + leap) return 9;
	if (day < 334 + leap) return 10;
	return 11;
}

static int DateFromTime(double t)
{
	int day = DayWithinYear(t);
	int leap = InLeapYear(t);
	switch (MonthFromTime(t)) {
	case 0: return day + 1;
	case 1: return day - 30;
	case 2: return day - 58 - leap;
	case 3: return day - 89 - leap;
	case 4: return day - 119 - leap;
	case 5: return day - 150 - leap;
	case 6: return day - 180 - leap;
	case 7: return day - 211 - leap;
	case 8: return day - 242 - leap;
	case 9: return day - 272 - leap;
	case 10: return day - 303 - leap;
	default : return day - 333 - leap;
	}
}

static int WeekDay(double t)
{
	return pmod(Day(t) + 4, 7);
}

static double LocalTime(double utc)
{
	return utc + LocalTZA() + DaylightSavingTA(utc);
}

static double UTC(double loc)
{
	return loc - LocalTZA() - DaylightSavingTA(loc - LocalTZA());
}

static int HourFromTime(double t)
{
	return pmod(floor(t / msPerHour), HoursPerDay);
}

static int MinFromTime(double t)
{
	return pmod(floor(t / msPerMinute), MinutesPerHour);
}

static int SecFromTime(double t)
{
	return pmod(floor(t / msPerSecond), SecondsPerMinute);
}

static int msFromTime(double t)
{
	return pmod(t, msPerSecond);
}

static double MakeTime(double hour, double min, double sec, double ms)
{
	return ((hour * MinutesPerHour + min) * SecondsPerMinute + sec) * msPerSecond + ms;
}

static double MakeDay(double y, double m, double date)
{

	static const double firstDayOfMonth[2][12] = {
		{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
		{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}
	};

	double yd, md;
	int im;

	y += floor(m / 12);
	m = pmod(m, 12);

	im = (int)m;
	if (im < 0 || im >= 12)
		return NAN;

	yd = floor(TimeFromYear(y) / msPerDay);
	md = firstDayOfMonth[DaysInYear(y) == 366][im];

	return yd + md + date - 1;
}

static double MakeDate(double day, double time)
{
	return day * msPerDay + time;
}

static double TimeClip(double t)
{
	if (!isfinite(t))
		return NAN;
	if (fabs(t) > 8.64e15)
		return NAN;
	return t < 0 ? -floor(-t) : floor(t);
}

static int toint(const char **sp, int w, int *v)
{
	const char *s = *sp;
	*v = 0;
	while (w--) {
		if (*s < '0' || *s > '9')
			return 0;
		*v = *v * 10 + (*s++ - '0');
	}
	*sp = s;
	return 1;
}

static double parseDateTime(const char *s)
{
	int y = 1970, m = 1, d = 1, H = 0, M = 0, S = 0, ms = 0;
	int tza = 0;
	double t;

	if (!toint(&s, 4, &y)) return NAN;
	if (*s == '-') {
		s += 1;
		if (!toint(&s, 2, &m)) return NAN;
		if (*s == '-') {
			s += 1;
			if (!toint(&s, 2, &d)) return NAN;
		}
	}

	if (*s == 'T') {
		s += 1;
		if (!toint(&s, 2, &H)) return NAN;
		if (*s != ':') return NAN;
		s += 1;
		if (!toint(&s, 2, &M)) return NAN;
		if (*s == ':') {
			s += 1;
			if (!toint(&s, 2, &S)) return NAN;
			if (*s == '.') {
				s += 1;
				if (!toint(&s, 3, &ms)) return NAN;
			}
		}
		if (*s == 'Z') {
			s += 1;
			tza = 0;
		} else if (*s == '+' || *s == '-') {
			int tzh = 0, tzm = 0;
			int tzs = *s == '+' ? 1 : -1;
			s += 1;
			if (!toint(&s, 2, &tzh)) return NAN;
			if (*s == ':') {
				s += 1;
				if (!toint(&s, 2, &tzm)) return NAN;
			}
			if (tzh > 23 || tzm > 59) return NAN;
			tza = tzs * (tzh * msPerHour + tzm * msPerMinute);
		} else {
			tza = LocalTZA();
		}
	}

	if (*s) return NAN;

	if (m < 1 || m > 12) return NAN;
	if (d < 1 || d > 31) return NAN;
	if (H < 0 || H > 24) return NAN;
	if (M < 0 || M > 59) return NAN;
	if (S < 0 || S > 59) return NAN;
	if (ms < 0 || ms > 999) return NAN;
	if (H == 24 && (M != 0 || S != 0 || ms != 0)) return NAN;

	t = MakeDate(MakeDay(y, m-1, d), MakeTime(H, M, S, ms));
	return t - tza;
}

static char *fmtdate(char *buf, double t)
{
	int y = YearFromTime(t);
	int m = MonthFromTime(t);
	int d = DateFromTime(t);
	if (!isfinite(t))
		return "Invalid Date";
	sprintf(buf, "%04d-%02d-%02d", y, m+1, d);
	return buf;
}

static char *fmttime(char *buf, double t, double tza)
{
	int H = HourFromTime(t);
	int M = MinFromTime(t);
	int S = SecFromTime(t);
	int ms = msFromTime(t);
	int tzh = HourFromTime(fabs(tza));
	int tzm = MinFromTime(fabs(tza));
	if (!isfinite(t))
		return "Invalid Date";
	if (tza == 0)
		sprintf(buf, "%02d:%02d:%02d.%03dZ", H, M, S, ms);
	else if (tza < 0)
		sprintf(buf, "%02d:%02d:%02d.%03d-%02d:%02d", H, M, S, ms, tzh, tzm);
	else
		sprintf(buf, "%02d:%02d:%02d.%03d+%02d:%02d", H, M, S, ms, tzh, tzm);
	return buf;
}

static char *fmtdatetime(char *buf, double t, double tza)
{
	char dbuf[20], tbuf[20];
	if (!isfinite(t))
		return "Invalid Date";
	fmtdate(dbuf, t);
	fmttime(tbuf, t, tza);
	sprintf(buf, "%sT%s", dbuf, tbuf);
	return buf;
}

static double js_todate(js_State *J, int idx)
{
	js_Object *self = js_toobject(J, idx);
	if (self->type != JS_CDATE)
		js_typeerror(J, "not a date");
	return self->u.number;
}

static void js_setdate(js_State *J, int idx, double t)
{
	js_Object *self = js_toobject(J, idx);
	if (self->type != JS_CDATE)
		js_typeerror(J, "not a date");
	self->u.number = TimeClip(t);
	js_pushnumber(J, self->u.number);
}

static void D_parse(js_State *J)
{
	double t = parseDateTime(js_tostring(J, 1));
	js_pushnumber(J, t);
}

static void D_UTC(js_State *J)
{
	double y, m, d, H, M, S, ms, t;
	y = js_tonumber(J, 1);
	if (y < 100) y += 1900;
	m = js_tonumber(J, 2);
	d = js_optnumber(J, 3, 1);
	H = js_optnumber(J, 4, 0);
	M = js_optnumber(J, 5, 0);
	S = js_optnumber(J, 6, 0);
	ms = js_optnumber(J, 7, 0);
	t = MakeDate(MakeDay(y, m, d), MakeTime(H, M, S, ms));
	t = TimeClip(t);
	js_pushnumber(J, t);
}

static void D_now(js_State *J)
{
	js_pushnumber(J, Now());
}

static void jsB_Date(js_State *J)
{
	char buf[64];
	js_pushstring(J, fmtdatetime(buf, LocalTime(Now()), LocalTZA()));
}

static void jsB_new_Date(js_State *J)
{
	int top = js_gettop(J);
	js_Object *obj;
	double t;

	if (top == 1)
		t = Now();
	else if (top == 2) {
		js_toprimitive(J, 1, JS_HNONE);
		if (js_isstring(J, 1))
			t = parseDateTime(js_tostring(J, 1));
		else
			t = TimeClip(js_tonumber(J, 1));
	} else {
		double y, m, d, H, M, S, ms;
		y = js_tonumber(J, 1);
		if (y < 100) y += 1900;
		m = js_tonumber(J, 2);
		d = js_optnumber(J, 3, 1);
		H = js_optnumber(J, 4, 0);
		M = js_optnumber(J, 5, 0);
		S = js_optnumber(J, 6, 0);
		ms = js_optnumber(J, 7, 0);
		t = MakeDate(MakeDay(y, m, d), MakeTime(H, M, S, ms));
		t = TimeClip(UTC(t));
	}

	obj = jsV_newobject(J, JS_CDATE, J->Date_prototype);
	obj->u.number = t;

	js_pushobject(J, obj);
}

static void Dp_valueOf(js_State *J)
{
	double t = js_todate(J, 0);
	js_pushnumber(J, t);
}

static void Dp_toString(js_State *J)
{
	char buf[64];
	double t = js_todate(J, 0);
	js_pushstring(J, fmtdatetime(buf, LocalTime(t), LocalTZA()));
}

static void Dp_toDateString(js_State *J)
{
	char buf[64];
	double t = js_todate(J, 0);
	js_pushstring(J, fmtdate(buf, LocalTime(t)));
}

static void Dp_toTimeString(js_State *J)
{
	char buf[64];
	double t = js_todate(J, 0);
	js_pushstring(J, fmttime(buf, LocalTime(t), LocalTZA()));
}

static void Dp_toUTCString(js_State *J)
{
	char buf[64];
	double t = js_todate(J, 0);
	js_pushstring(J, fmtdatetime(buf, t, 0));
}

static void Dp_toISOString(js_State *J)
{
	char buf[64];
	double t = js_todate(J, 0);
	if (!isfinite(t))
		js_rangeerror(J, "invalid date");
	js_pushstring(J, fmtdatetime(buf, t, 0));
}

static void Dp_getFullYear(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, YearFromTime(LocalTime(t)));
}

static void Dp_getMonth(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, MonthFromTime(LocalTime(t)));
}

static void Dp_getDate(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, DateFromTime(LocalTime(t)));
}

static void Dp_getDay(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, WeekDay(LocalTime(t)));
}

static void Dp_getHours(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, HourFromTime(LocalTime(t)));
}

static void Dp_getMinutes(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, MinFromTime(LocalTime(t)));
}

static void Dp_getSeconds(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, SecFromTime(LocalTime(t)));
}

static void Dp_getMilliseconds(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, msFromTime(LocalTime(t)));
}

static void Dp_getUTCFullYear(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, YearFromTime(t));
}

static void Dp_getUTCMonth(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, MonthFromTime(t));
}

static void Dp_getUTCDate(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, DateFromTime(t));
}

static void Dp_getUTCDay(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, WeekDay(t));
}

static void Dp_getUTCHours(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, HourFromTime(t));
}

static void Dp_getUTCMinutes(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, MinFromTime(t));
}

static void Dp_getUTCSeconds(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, SecFromTime(t));
}

static void Dp_getUTCMilliseconds(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, msFromTime(t));
}

static void Dp_getTimezoneOffset(js_State *J)
{
	double t = js_todate(J, 0);
	if (isnan(t))
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, (t - LocalTime(t)) / msPerMinute);
}

static void Dp_setTime(js_State *J)
{
	js_setdate(J, 0, js_tonumber(J, 1));
}

static void Dp_setMilliseconds(js_State *J)
{
	double t = LocalTime(js_todate(J, 0));
	double h = HourFromTime(t);
	double m = MinFromTime(t);
	double s = SecFromTime(t);
	double ms = js_tonumber(J, 1);
	js_setdate(J, 0, UTC(MakeDate(Day(t), MakeTime(h, m, s, ms))));
}

static void Dp_setSeconds(js_State *J)
{
	double t = LocalTime(js_todate(J, 0));
	double h = HourFromTime(t);
	double m = MinFromTime(t);
	double s = js_tonumber(J, 1);
	double ms = js_optnumber(J, 2, msFromTime(t));
	js_setdate(J, 0, UTC(MakeDate(Day(t), MakeTime(h, m, s, ms))));
}

static void Dp_setMinutes(js_State *J)
{
	double t = LocalTime(js_todate(J, 0));
	double h = HourFromTime(t);
	double m = js_tonumber(J, 1);
	double s = js_optnumber(J, 2, SecFromTime(t));
	double ms = js_optnumber(J, 3, msFromTime(t));
	js_setdate(J, 0, UTC(MakeDate(Day(t), MakeTime(h, m, s, ms))));
}

static void Dp_setHours(js_State *J)
{
	double t = LocalTime(js_todate(J, 0));
	double h = js_tonumber(J, 1);
	double m = js_optnumber(J, 2, MinFromTime(t));
	double s = js_optnumber(J, 3, SecFromTime(t));
	double ms = js_optnumber(J, 4, msFromTime(t));
	js_setdate(J, 0, UTC(MakeDate(Day(t), MakeTime(h, m, s, ms))));
}

static void Dp_setDate(js_State *J)
{
	double t = LocalTime(js_todate(J, 0));
	double y = YearFromTime(t);
	double m = MonthFromTime(t);
	double d = js_tonumber(J, 1);
	js_setdate(J, 0, UTC(MakeDate(MakeDay(y, m, d), TimeWithinDay(t))));
}

static void Dp_setMonth(js_State *J)
{
	double t = LocalTime(js_todate(J, 0));
	double y = YearFromTime(t);
	double m = js_tonumber(J, 1);
	double d = js_optnumber(J, 2, DateFromTime(t));
	js_setdate(J, 0, UTC(MakeDate(MakeDay(y, m, d), TimeWithinDay(t))));
}

static void Dp_setFullYear(js_State *J)
{
	double t = LocalTime(js_todate(J, 0));
	double y = js_tonumber(J, 1);
	double m = js_optnumber(J, 2, MonthFromTime(t));
	double d = js_optnumber(J, 3, DateFromTime(t));
	js_setdate(J, 0, UTC(MakeDate(MakeDay(y, m, d), TimeWithinDay(t))));
}

static void Dp_setUTCMilliseconds(js_State *J)
{
	double t = js_todate(J, 0);
	double h = HourFromTime(t);
	double m = MinFromTime(t);
	double s = SecFromTime(t);
	double ms = js_tonumber(J, 1);
	js_setdate(J, 0, MakeDate(Day(t), MakeTime(h, m, s, ms)));
}

static void Dp_setUTCSeconds(js_State *J)
{
	double t = js_todate(J, 0);
	double h = HourFromTime(t);
	double m = MinFromTime(t);
	double s = js_tonumber(J, 1);
	double ms = js_optnumber(J, 2, msFromTime(t));
	js_setdate(J, 0, MakeDate(Day(t), MakeTime(h, m, s, ms)));
}

static void Dp_setUTCMinutes(js_State *J)
{
	double t = js_todate(J, 0);
	double h = HourFromTime(t);
	double m = js_tonumber(J, 1);
	double s = js_optnumber(J, 2, SecFromTime(t));
	double ms = js_optnumber(J, 3, msFromTime(t));
	js_setdate(J, 0, MakeDate(Day(t), MakeTime(h, m, s, ms)));
}

static void Dp_setUTCHours(js_State *J)
{
	double t = js_todate(J, 0);
	double h = js_tonumber(J, 1);
	double m = js_optnumber(J, 2, HourFromTime(t));
	double s = js_optnumber(J, 3, SecFromTime(t));
	double ms = js_optnumber(J, 4, msFromTime(t));
	js_setdate(J, 0, MakeDate(Day(t), MakeTime(h, m, s, ms)));
}

static void Dp_setUTCDate(js_State *J)
{
	double t = js_todate(J, 0);
	double y = YearFromTime(t);
	double m = MonthFromTime(t);
	double d = js_tonumber(J, 1);
	js_setdate(J, 0, MakeDate(MakeDay(y, m, d), TimeWithinDay(t)));
}

static void Dp_setUTCMonth(js_State *J)
{
	double t = js_todate(J, 0);
	double y = YearFromTime(t);
	double m = js_tonumber(J, 1);
	double d = js_optnumber(J, 2, DateFromTime(t));
	js_setdate(J, 0, MakeDate(MakeDay(y, m, d), TimeWithinDay(t)));
}

static void Dp_setUTCFullYear(js_State *J)
{
	double t = js_todate(J, 0);
	double y = js_tonumber(J, 1);
	double m = js_optnumber(J, 2, MonthFromTime(t));
	double d = js_optnumber(J, 3, DateFromTime(t));
	js_setdate(J, 0, MakeDate(MakeDay(y, m, d), TimeWithinDay(t)));
}

static void Dp_toJSON(js_State *J)
{
	js_copy(J, 0);
	js_toprimitive(J, -1, JS_HNUMBER);
	if (js_isnumber(J, -1) && !isfinite(js_tonumber(J, -1))) {
		js_pushnull(J);
		return;
	}
	js_pop(J, 1);

	js_getproperty(J, 0, "toISOString");
	if (!js_iscallable(J, -1))
		js_typeerror(J, "this.toISOString is not a function");
	js_copy(J, 0);
	js_call(J, 0);
}

void jsB_initdate(js_State *J)
{
	J->Date_prototype->u.number = 0;

	js_pushobject(J, J->Date_prototype);
	{
		jsB_propf(J, "Date.prototype.valueOf", Dp_valueOf, 0);
		jsB_propf(J, "Date.prototype.toString", Dp_toString, 0);
		jsB_propf(J, "Date.prototype.toDateString", Dp_toDateString, 0);
		jsB_propf(J, "Date.prototype.toTimeString", Dp_toTimeString, 0);
		jsB_propf(J, "Date.prototype.toLocaleString", Dp_toString, 0);
		jsB_propf(J, "Date.prototype.toLocaleDateString", Dp_toDateString, 0);
		jsB_propf(J, "Date.prototype.toLocaleTimeString", Dp_toTimeString, 0);
		jsB_propf(J, "Date.prototype.toUTCString", Dp_toUTCString, 0);

		jsB_propf(J, "Date.prototype.getTime", Dp_valueOf, 0);
		jsB_propf(J, "Date.prototype.getFullYear", Dp_getFullYear, 0);
		jsB_propf(J, "Date.prototype.getUTCFullYear", Dp_getUTCFullYear, 0);
		jsB_propf(J, "Date.prototype.getMonth", Dp_getMonth, 0);
		jsB_propf(J, "Date.prototype.getUTCMonth", Dp_getUTCMonth, 0);
		jsB_propf(J, "Date.prototype.getDate", Dp_getDate, 0);
		jsB_propf(J, "Date.prototype.getUTCDate", Dp_getUTCDate, 0);
		jsB_propf(J, "Date.prototype.getDay", Dp_getDay, 0);
		jsB_propf(J, "Date.prototype.getUTCDay", Dp_getUTCDay, 0);
		jsB_propf(J, "Date.prototype.getHours", Dp_getHours, 0);
		jsB_propf(J, "Date.prototype.getUTCHours", Dp_getUTCHours, 0);
		jsB_propf(J, "Date.prototype.getMinutes", Dp_getMinutes, 0);
		jsB_propf(J, "Date.prototype.getUTCMinutes", Dp_getUTCMinutes, 0);
		jsB_propf(J, "Date.prototype.getSeconds", Dp_getSeconds, 0);
		jsB_propf(J, "Date.prototype.getUTCSeconds", Dp_getUTCSeconds, 0);
		jsB_propf(J, "Date.prototype.getMilliseconds", Dp_getMilliseconds, 0);
		jsB_propf(J, "Date.prototype.getUTCMilliseconds", Dp_getUTCMilliseconds, 0);
		jsB_propf(J, "Date.prototype.getTimezoneOffset", Dp_getTimezoneOffset, 0);

		jsB_propf(J, "Date.prototype.setTime", Dp_setTime, 1);
		jsB_propf(J, "Date.prototype.setMilliseconds", Dp_setMilliseconds, 1);
		jsB_propf(J, "Date.prototype.setUTCMilliseconds", Dp_setUTCMilliseconds, 1);
		jsB_propf(J, "Date.prototype.setSeconds", Dp_setSeconds, 2);
		jsB_propf(J, "Date.prototype.setUTCSeconds", Dp_setUTCSeconds, 2);
		jsB_propf(J, "Date.prototype.setMinutes", Dp_setMinutes, 3);
		jsB_propf(J, "Date.prototype.setUTCMinutes", Dp_setUTCMinutes, 3);
		jsB_propf(J, "Date.prototype.setHours", Dp_setHours, 4);
		jsB_propf(J, "Date.prototype.setUTCHours", Dp_setUTCHours, 4);
		jsB_propf(J, "Date.prototype.setDate", Dp_setDate, 1);
		jsB_propf(J, "Date.prototype.setUTCDate", Dp_setUTCDate, 1);
		jsB_propf(J, "Date.prototype.setMonth", Dp_setMonth, 2);
		jsB_propf(J, "Date.prototype.setUTCMonth", Dp_setUTCMonth, 2);
		jsB_propf(J, "Date.prototype.setFullYear", Dp_setFullYear, 3);
		jsB_propf(J, "Date.prototype.setUTCFullYear", Dp_setUTCFullYear, 3);

		jsB_propf(J, "Date.prototype.toISOString", Dp_toISOString, 0);
		jsB_propf(J, "Date.prototype.toJSON", Dp_toJSON, 1);
	}
	js_newcconstructor(J, jsB_Date, jsB_new_Date, "Date", 0);
	{
		jsB_propf(J, "Date.parse", D_parse, 1);
		jsB_propf(J, "Date.UTC", D_UTC, 7);

		jsB_propf(J, "Date.now", D_now, 0);
	}
	js_defglobal(J, "Date", JS_DONTENUM);
}

#if defined(_MSC_VER) && (_MSC_VER < 1700)
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

#include <errno.h>
#include <assert.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

void
js_fmtexp(char *p, int e)
{
	char se[9];
	int i;

	*p++ = 'e';
	if(e < 0) {
		*p++ = '-';
		e = -e;
	} else
		*p++ = '+';
	i = 0;
	while(e) {
		se[i++] = e % 10 + '0';
		e /= 10;
	}
	while(i < 1)
		se[i++] = '0';
	while(i > 0)
		*p++ = se[--i];
	*p++ = '\0';
}

typedef struct diy_fp_t {
	uint64_t f;
	int e;
} diy_fp_t;

#define DIY_SIGNIFICAND_SIZE 64
#define D_1_LOG2_10 0.30102999566398114

static const uint64_t powers_ten[] = {
	0xbf29dcaba82fdeae, 0xeef453d6923bd65a, 0x9558b4661b6565f8,
	0xbaaee17fa23ebf76, 0xe95a99df8ace6f54, 0x91d8a02bb6c10594,
	0xb64ec836a47146fa, 0xe3e27a444d8d98b8, 0x8e6d8c6ab0787f73,
	0xb208ef855c969f50, 0xde8b2b66b3bc4724, 0x8b16fb203055ac76,
	0xaddcb9e83c6b1794, 0xd953e8624b85dd79, 0x87d4713d6f33aa6c,
	0xa9c98d8ccb009506, 0xd43bf0effdc0ba48, 0x84a57695fe98746d,
	0xa5ced43b7e3e9188, 0xcf42894a5dce35ea, 0x818995ce7aa0e1b2,
	0xa1ebfb4219491a1f, 0xca66fa129f9b60a7, 0xfd00b897478238d1,
	0x9e20735e8cb16382, 0xc5a890362fddbc63, 0xf712b443bbd52b7c,
	0x9a6bb0aa55653b2d, 0xc1069cd4eabe89f9, 0xf148440a256e2c77,
	0x96cd2a865764dbca, 0xbc807527ed3e12bd, 0xeba09271e88d976c,
	0x93445b8731587ea3, 0xb8157268fdae9e4c, 0xe61acf033d1a45df,
	0x8fd0c16206306bac, 0xb3c4f1ba87bc8697, 0xe0b62e2929aba83c,
	0x8c71dcd9ba0b4926, 0xaf8e5410288e1b6f, 0xdb71e91432b1a24b,
	0x892731ac9faf056f, 0xab70fe17c79ac6ca, 0xd64d3d9db981787d,
	0x85f0468293f0eb4e, 0xa76c582338ed2622, 0xd1476e2c07286faa,
	0x82cca4db847945ca, 0xa37fce126597973d, 0xcc5fc196fefd7d0c,
	0xff77b1fcbebcdc4f, 0x9faacf3df73609b1, 0xc795830d75038c1e,
	0xf97ae3d0d2446f25, 0x9becce62836ac577, 0xc2e801fb244576d5,
	0xf3a20279ed56d48a, 0x9845418c345644d7, 0xbe5691ef416bd60c,
	0xedec366b11c6cb8f, 0x94b3a202eb1c3f39, 0xb9e08a83a5e34f08,
	0xe858ad248f5c22ca, 0x91376c36d99995be, 0xb58547448ffffb2e,
	0xe2e69915b3fff9f9, 0x8dd01fad907ffc3c, 0xb1442798f49ffb4b,
	0xdd95317f31c7fa1d, 0x8a7d3eef7f1cfc52, 0xad1c8eab5ee43b67,
	0xd863b256369d4a41, 0x873e4f75e2224e68, 0xa90de3535aaae202,
	0xd3515c2831559a83, 0x8412d9991ed58092, 0xa5178fff668ae0b6,
	0xce5d73ff402d98e4, 0x80fa687f881c7f8e, 0xa139029f6a239f72,
	0xc987434744ac874f, 0xfbe9141915d7a922, 0x9d71ac8fada6c9b5,
	0xc4ce17b399107c23, 0xf6019da07f549b2b, 0x99c102844f94e0fb,
	0xc0314325637a193a, 0xf03d93eebc589f88, 0x96267c7535b763b5,
	0xbbb01b9283253ca3, 0xea9c227723ee8bcb, 0x92a1958a7675175f,
	0xb749faed14125d37, 0xe51c79a85916f485, 0x8f31cc0937ae58d3,
	0xb2fe3f0b8599ef08, 0xdfbdcece67006ac9, 0x8bd6a141006042be,
	0xaecc49914078536d, 0xda7f5bf590966849, 0x888f99797a5e012d,
	0xaab37fd7d8f58179, 0xd5605fcdcf32e1d7, 0x855c3be0a17fcd26,
	0xa6b34ad8c9dfc070, 0xd0601d8efc57b08c, 0x823c12795db6ce57,
	0xa2cb1717b52481ed, 0xcb7ddcdda26da269, 0xfe5d54150b090b03,
	0x9efa548d26e5a6e2, 0xc6b8e9b0709f109a, 0xf867241c8cc6d4c1,
	0x9b407691d7fc44f8, 0xc21094364dfb5637, 0xf294b943e17a2bc4,
	0x979cf3ca6cec5b5b, 0xbd8430bd08277231, 0xece53cec4a314ebe,
	0x940f4613ae5ed137, 0xb913179899f68584, 0xe757dd7ec07426e5,
	0x9096ea6f3848984f, 0xb4bca50b065abe63, 0xe1ebce4dc7f16dfc,
	0x8d3360f09cf6e4bd, 0xb080392cc4349ded, 0xdca04777f541c568,
	0x89e42caaf9491b61, 0xac5d37d5b79b6239, 0xd77485cb25823ac7,
	0x86a8d39ef77164bd, 0xa8530886b54dbdec, 0xd267caa862a12d67,
	0x8380dea93da4bc60, 0xa46116538d0deb78, 0xcd795be870516656,
	0x806bd9714632dff6, 0xa086cfcd97bf97f4, 0xc8a883c0fdaf7df0,
	0xfad2a4b13d1b5d6c, 0x9cc3a6eec6311a64, 0xc3f490aa77bd60fd,
	0xf4f1b4d515acb93c, 0x991711052d8bf3c5, 0xbf5cd54678eef0b7,
	0xef340a98172aace5, 0x9580869f0e7aac0f, 0xbae0a846d2195713,
	0xe998d258869facd7, 0x91ff83775423cc06, 0xb67f6455292cbf08,
	0xe41f3d6a7377eeca, 0x8e938662882af53e, 0xb23867fb2a35b28e,
	0xdec681f9f4c31f31, 0x8b3c113c38f9f37f, 0xae0b158b4738705f,
	0xd98ddaee19068c76, 0x87f8a8d4cfa417ca, 0xa9f6d30a038d1dbc,
	0xd47487cc8470652b, 0x84c8d4dfd2c63f3b, 0xa5fb0a17c777cf0a,
	0xcf79cc9db955c2cc, 0x81ac1fe293d599c0, 0xa21727db38cb0030,
	0xca9cf1d206fdc03c, 0xfd442e4688bd304b, 0x9e4a9cec15763e2f,
	0xc5dd44271ad3cdba, 0xf7549530e188c129, 0x9a94dd3e8cf578ba,
	0xc13a148e3032d6e8, 0xf18899b1bc3f8ca2, 0x96f5600f15a7b7e5,
	0xbcb2b812db11a5de, 0xebdf661791d60f56, 0x936b9fcebb25c996,
	0xb84687c269ef3bfb, 0xe65829b3046b0afa, 0x8ff71a0fe2c2e6dc,
	0xb3f4e093db73a093, 0xe0f218b8d25088b8, 0x8c974f7383725573,
	0xafbd2350644eead0, 0xdbac6c247d62a584, 0x894bc396ce5da772,
	0xab9eb47c81f5114f, 0xd686619ba27255a3, 0x8613fd0145877586,
	0xa798fc4196e952e7, 0xd17f3b51fca3a7a1, 0x82ef85133de648c5,
	0xa3ab66580d5fdaf6, 0xcc963fee10b7d1b3, 0xffbbcfe994e5c620,
	0x9fd561f1fd0f9bd4, 0xc7caba6e7c5382c9, 0xf9bd690a1b68637b,
	0x9c1661a651213e2d, 0xc31bfa0fe5698db8, 0xf3e2f893dec3f126,
	0x986ddb5c6b3a76b8, 0xbe89523386091466, 0xee2ba6c0678b597f,
	0x94db483840b717f0, 0xba121a4650e4ddec, 0xe896a0d7e51e1566,
	0x915e2486ef32cd60, 0xb5b5ada8aaff80b8, 0xe3231912d5bf60e6,
	0x8df5efabc5979c90, 0xb1736b96b6fd83b4, 0xddd0467c64bce4a1,
	0x8aa22c0dbef60ee4, 0xad4ab7112eb3929e, 0xd89d64d57a607745,
	0x87625f056c7c4a8b, 0xa93af6c6c79b5d2e, 0xd389b47879823479,
	0x843610cb4bf160cc, 0xa54394fe1eedb8ff, 0xce947a3da6a9273e,
	0x811ccc668829b887, 0xa163ff802a3426a9, 0xc9bcff6034c13053,
	0xfc2c3f3841f17c68, 0x9d9ba7832936edc1, 0xc5029163f384a931,
	0xf64335bcf065d37d, 0x99ea0196163fa42e, 0xc06481fb9bcf8d3a,
	0xf07da27a82c37088, 0x964e858c91ba2655, 0xbbe226efb628afeb,
	0xeadab0aba3b2dbe5, 0x92c8ae6b464fc96f, 0xb77ada0617e3bbcb,
	0xe55990879ddcaabe, 0x8f57fa54c2a9eab7, 0xb32df8e9f3546564,
	0xdff9772470297ebd, 0x8bfbea76c619ef36, 0xaefae51477a06b04,
	0xdab99e59958885c5, 0x88b402f7fd75539b, 0xaae103b5fcd2a882,
	0xd59944a37c0752a2, 0x857fcae62d8493a5, 0xa6dfbd9fb8e5b88f,
	0xd097ad07a71f26b2, 0x825ecc24c8737830, 0xa2f67f2dfa90563b,
	0xcbb41ef979346bca, 0xfea126b7d78186bd, 0x9f24b832e6b0f436,
	0xc6ede63fa05d3144, 0xf8a95fcf88747d94, 0x9b69dbe1b548ce7d,
	0xc24452da229b021c, 0xf2d56790ab41c2a3, 0x97c560ba6b0919a6,
	0xbdb6b8e905cb600f, 0xed246723473e3813, 0x9436c0760c86e30c,
	0xb94470938fa89bcf, 0xe7958cb87392c2c3, 0x90bd77f3483bb9ba,
	0xb4ecd5f01a4aa828, 0xe2280b6c20dd5232, 0x8d590723948a535f,
	0xb0af48ec79ace837, 0xdcdb1b2798182245, 0x8a08f0f8bf0f156b,
	0xac8b2d36eed2dac6, 0xd7adf884aa879177, 0x86ccbb52ea94baeb,
	0xa87fea27a539e9a5, 0xd29fe4b18e88640f, 0x83a3eeeef9153e89,
	0xa48ceaaab75a8e2b, 0xcdb02555653131b6, 0x808e17555f3ebf12,
	0xa0b19d2ab70e6ed6, 0xc8de047564d20a8c, 0xfb158592be068d2f,
	0x9ced737bb6c4183d, 0xc428d05aa4751e4d, 0xf53304714d9265e0,
	0x993fe2c6d07b7fac, 0xbf8fdb78849a5f97, 0xef73d256a5c0f77d,
	0x95a8637627989aae, 0xbb127c53b17ec159, 0xe9d71b689dde71b0,
	0x9226712162ab070e, 0xb6b00d69bb55c8d1, 0xe45c10c42a2b3b06,
	0x8eb98a7a9a5b04e3, 0xb267ed1940f1c61c, 0xdf01e85f912e37a3,
	0x8b61313bbabce2c6, 0xae397d8aa96c1b78, 0xd9c7dced53c72256,
	0x881cea14545c7575, 0xaa242499697392d3, 0xd4ad2dbfc3d07788,
	0x84ec3c97da624ab5, 0xa6274bbdd0fadd62, 0xcfb11ead453994ba,
	0x81ceb32c4b43fcf5, 0xa2425ff75e14fc32, 0xcad2f7f5359a3b3e,
	0xfd87b5f28300ca0e, 0x9e74d1b791e07e48, 0xc612062576589ddb,
	0xf79687aed3eec551, 0x9abe14cd44753b53, 0xc16d9a0095928a27,
	0xf1c90080baf72cb1, 0x971da05074da7bef, 0xbce5086492111aeb,
	0xec1e4a7db69561a5, 0x9392ee8e921d5d07, 0xb877aa3236a4b449,
	0xe69594bec44de15b, 0x901d7cf73ab0acd9, 0xb424dc35095cd80f,
	0xe12e13424bb40e13, 0x8cbccc096f5088cc, 0xafebff0bcb24aaff,
	0xdbe6fecebdedd5bf, 0x89705f4136b4a597, 0xabcc77118461cefd,
	0xd6bf94d5e57a42bc, 0x8637bd05af6c69b6, 0xa7c5ac471b478423,
	0xd1b71758e219652c, 0x83126e978d4fdf3b, 0xa3d70a3d70a3d70a,
	0xcccccccccccccccd, 0x8000000000000000, 0xa000000000000000,
	0xc800000000000000, 0xfa00000000000000, 0x9c40000000000000,
	0xc350000000000000, 0xf424000000000000, 0x9896800000000000,
	0xbebc200000000000, 0xee6b280000000000, 0x9502f90000000000,
	0xba43b74000000000, 0xe8d4a51000000000, 0x9184e72a00000000,
	0xb5e620f480000000, 0xe35fa931a0000000, 0x8e1bc9bf04000000,
	0xb1a2bc2ec5000000, 0xde0b6b3a76400000, 0x8ac7230489e80000,
	0xad78ebc5ac620000, 0xd8d726b7177a8000, 0x878678326eac9000,
	0xa968163f0a57b400, 0xd3c21bcecceda100, 0x84595161401484a0,
	0xa56fa5b99019a5c8, 0xcecb8f27f4200f3a, 0x813f3978f8940984,
	0xa18f07d736b90be5, 0xc9f2c9cd04674edf, 0xfc6f7c4045812296,
	0x9dc5ada82b70b59e, 0xc5371912364ce305, 0xf684df56c3e01bc7,
	0x9a130b963a6c115c, 0xc097ce7bc90715b3, 0xf0bdc21abb48db20,
	0x96769950b50d88f4, 0xbc143fa4e250eb31, 0xeb194f8e1ae525fd,
	0x92efd1b8d0cf37be, 0xb7abc627050305ae, 0xe596b7b0c643c719,
	0x8f7e32ce7bea5c70, 0xb35dbf821ae4f38c, 0xe0352f62a19e306f,
	0x8c213d9da502de45, 0xaf298d050e4395d7, 0xdaf3f04651d47b4c,
	0x88d8762bf324cd10, 0xab0e93b6efee0054, 0xd5d238a4abe98068,
	0x85a36366eb71f041, 0xa70c3c40a64e6c52, 0xd0cf4b50cfe20766,
	0x82818f1281ed44a0, 0xa321f2d7226895c8, 0xcbea6f8ceb02bb3a,
	0xfee50b7025c36a08, 0x9f4f2726179a2245, 0xc722f0ef9d80aad6,
	0xf8ebad2b84e0d58c, 0x9b934c3b330c8577, 0xc2781f49ffcfa6d5,
	0xf316271c7fc3908b, 0x97edd871cfda3a57, 0xbde94e8e43d0c8ec,
	0xed63a231d4c4fb27, 0x945e455f24fb1cf9, 0xb975d6b6ee39e437,
	0xe7d34c64a9c85d44, 0x90e40fbeea1d3a4b, 0xb51d13aea4a488dd,
	0xe264589a4dcdab15, 0x8d7eb76070a08aed, 0xb0de65388cc8ada8,
	0xdd15fe86affad912, 0x8a2dbf142dfcc7ab, 0xacb92ed9397bf996,
	0xd7e77a8f87daf7fc, 0x86f0ac99b4e8dafd, 0xa8acd7c0222311bd,
	0xd2d80db02aabd62c, 0x83c7088e1aab65db, 0xa4b8cab1a1563f52,
	0xcde6fd5e09abcf27, 0x80b05e5ac60b6178, 0xa0dc75f1778e39d6,
	0xc913936dd571c84c, 0xfb5878494ace3a5f, 0x9d174b2dcec0e47b,
	0xc45d1df942711d9a, 0xf5746577930d6501, 0x9968bf6abbe85f20,
	0xbfc2ef456ae276e9, 0xefb3ab16c59b14a3, 0x95d04aee3b80ece6,
	0xbb445da9ca61281f, 0xea1575143cf97227, 0x924d692ca61be758,
	0xb6e0c377cfa2e12e, 0xe498f455c38b997a, 0x8edf98b59a373fec,
	0xb2977ee300c50fe7, 0xdf3d5e9bc0f653e1, 0x8b865b215899f46d,
	0xae67f1e9aec07188, 0xda01ee641a708dea, 0x884134fe908658b2,
	0xaa51823e34a7eedf, 0xd4e5e2cdc1d1ea96, 0x850fadc09923329e,
	0xa6539930bf6bff46, 0xcfe87f7cef46ff17, 0x81f14fae158c5f6e,
	0xa26da3999aef774a, 0xcb090c8001ab551c, 0xfdcb4fa002162a63,
	0x9e9f11c4014dda7e, 0xc646d63501a1511e, 0xf7d88bc24209a565,
	0x9ae757596946075f, 0xc1a12d2fc3978937, 0xf209787bb47d6b85,
	0x9745eb4d50ce6333, 0xbd176620a501fc00, 0xec5d3fa8ce427b00,
	0x93ba47c980e98ce0, 0xb8a8d9bbe123f018, 0xe6d3102ad96cec1e,
	0x9043ea1ac7e41393, 0xb454e4a179dd1877, 0xe16a1dc9d8545e95,
	0x8ce2529e2734bb1d, 0xb01ae745b101e9e4, 0xdc21a1171d42645d,
	0x899504ae72497eba, 0xabfa45da0edbde69, 0xd6f8d7509292d603,
	0x865b86925b9bc5c2, 0xa7f26836f282b733, 0xd1ef0244af2364ff,
	0x8335616aed761f1f, 0xa402b9c5a8d3a6e7, 0xcd036837130890a1,
	0x802221226be55a65, 0xa02aa96b06deb0fe, 0xc83553c5c8965d3d,
	0xfa42a8b73abbf48d, 0x9c69a97284b578d8, 0xc38413cf25e2d70e,
	0xf46518c2ef5b8cd1, 0x98bf2f79d5993803, 0xbeeefb584aff8604,
	0xeeaaba2e5dbf6785, 0x952ab45cfa97a0b3, 0xba756174393d88e0,
	0xe912b9d1478ceb17, 0x91abb422ccb812ef, 0xb616a12b7fe617aa,
	0xe39c49765fdf9d95, 0x8e41ade9fbebc27d, 0xb1d219647ae6b31c,
	0xde469fbd99a05fe3, 0x8aec23d680043bee, 0xada72ccc20054aea,
	0xd910f7ff28069da4, 0x87aa9aff79042287, 0xa99541bf57452b28,
	0xd3fa922f2d1675f2, 0x847c9b5d7c2e09b7, 0xa59bc234db398c25,
	0xcf02b2c21207ef2f, 0x8161afb94b44f57d, 0xa1ba1ba79e1632dc,
	0xca28a291859bbf93, 0xfcb2cb35e702af78, 0x9defbf01b061adab,
	0xc56baec21c7a1916, 0xf6c69a72a3989f5c, 0x9a3c2087a63f6399,
	0xc0cb28a98fcf3c80, 0xf0fdf2d3f3c30b9f, 0x969eb7c47859e744,
	0xbc4665b596706115, 0xeb57ff22fc0c795a, 0x9316ff75dd87cbd8,
	0xb7dcbf5354e9bece, 0xe5d3ef282a242e82, 0x8fa475791a569d11,
	0xb38d92d760ec4455, 0xe070f78d3927556b, 0x8c469ab843b89563,
	0xaf58416654a6babb, 0xdb2e51bfe9d0696a, 0x88fcf317f22241e2,
	0xab3c2fddeeaad25b, 0xd60b3bd56a5586f2, 0x85c7056562757457,
	0xa738c6bebb12d16d, 0xd106f86e69d785c8, 0x82a45b450226b39d,
	0xa34d721642b06084, 0xcc20ce9bd35c78a5, 0xff290242c83396ce,
	0x9f79a169bd203e41, 0xc75809c42c684dd1, 0xf92e0c3537826146,
	0x9bbcc7a142b17ccc, 0xc2abf989935ddbfe, 0xf356f7ebf83552fe,
	0x98165af37b2153df, 0xbe1bf1b059e9a8d6, 0xeda2ee1c7064130c,
	0x9485d4d1c63e8be8, 0xb9a74a0637ce2ee1, 0xe8111c87c5c1ba9a,
	0x910ab1d4db9914a0, 0xb54d5e4a127f59c8, 0xe2a0b5dc971f303a,
	0x8da471a9de737e24, 0xb10d8e1456105dad, 0xdd50f1996b947519,
	0x8a5296ffe33cc930, 0xace73cbfdc0bfb7b, 0xd8210befd30efa5a,
	0x8714a775e3e95c78, 0xa8d9d1535ce3b396, 0xd31045a8341ca07c,
	0x83ea2b892091e44e, 0xa4e4b66b68b65d61, 0xce1de40642e3f4b9,
	0x80d2ae83e9ce78f4, 0xa1075a24e4421731, 0xc94930ae1d529cfd,
	0xfb9b7cd9a4a7443c, 0x9d412e0806e88aa6, 0xc491798a08a2ad4f,
	0xf5b5d7ec8acb58a3, 0x9991a6f3d6bf1766, 0xbff610b0cc6edd3f,
	0xeff394dcff8a948f, 0x95f83d0a1fb69cd9, 0xbb764c4ca7a44410,
	0xea53df5fd18d5514, 0x92746b9be2f8552c, 0xb7118682dbb66a77,
	0xe4d5e82392a40515, 0x8f05b1163ba6832d, 0xb2c71d5bca9023f8,
	0xdf78e4b2bd342cf7, 0x8bab8eefb6409c1a, 0xae9672aba3d0c321,
	0xda3c0f568cc4f3e9, 0x8865899617fb1871, 0xaa7eebfb9df9de8e,
	0xd51ea6fa85785631, 0x8533285c936b35df, 0xa67ff273b8460357,
	0xd01fef10a657842c, 0x8213f56a67f6b29c, 0xa298f2c501f45f43,
	0xcb3f2f7642717713, 0xfe0efb53d30dd4d8, 0x9ec95d1463e8a507,
	0xc67bb4597ce2ce49, 0xf81aa16fdc1b81db, 0x9b10a4e5e9913129,
	0xc1d4ce1f63f57d73, 0xf24a01a73cf2dcd0, 0x976e41088617ca02,
	0xbd49d14aa79dbc82, 0xec9c459d51852ba3, 0x93e1ab8252f33b46,
	0xb8da1662e7b00a17, 0xe7109bfba19c0c9d, 0x906a617d450187e2,
	0xb484f9dc9641e9db, 0xe1a63853bbd26451, 0x8d07e33455637eb3,
	0xb049dc016abc5e60, 0xdc5c5301c56b75f7, 0x89b9b3e11b6329bb,
	0xac2820d9623bf429, 0xd732290fbacaf134, 0x867f59a9d4bed6c0,
	0xa81f301449ee8c70, 0xd226fc195c6a2f8c, 0x83585d8fd9c25db8,
	0xa42e74f3d032f526, 0xcd3a1230c43fb26f, 0x80444b5e7aa7cf85,
	0xa0555e361951c367, 0xc86ab5c39fa63441, 0xfa856334878fc151,
	0x9c935e00d4b9d8d2, 0xc3b8358109e84f07, 0xf4a642e14c6262c9,
	0x98e7e9cccfbd7dbe, 0xbf21e44003acdd2d, 0xeeea5d5004981478,
	0x95527a5202df0ccb, 0xbaa718e68396cffe, 0xe950df20247c83fd,
	0x91d28b7416cdd27e, 0xb6472e511c81471e, 0xe3d8f9e563a198e5,
	0x8e679c2f5e44ff8f, 0xb201833b35d63f73, 0xde81e40a034bcf50,
	0x8b112e86420f6192, 0xadd57a27d29339f6, 0xd94ad8b1c7380874,
	0x87cec76f1c830549, 0xa9c2794ae3a3c69b, 0xd433179d9c8cb841,
	0x849feec281d7f329, 0xa5c7ea73224deff3, 0xcf39e50feae16bf0,
	0x81842f29f2cce376, 0xa1e53af46f801c53, 0xca5e89b18b602368,
	0xfcf62c1dee382c42, 0x9e19db92b4e31ba9, 0xc5a05277621be294,
	0xf70867153aa2db39, 0x9a65406d44a5c903, 0xc0fe908895cf3b44,
	0xf13e34aabb430a15, 0x96c6e0eab509e64d, 0xbc789925624c5fe1,
	0xeb96bf6ebadf77d9, 0x933e37a534cbaae8, 0xb80dc58e81fe95a1,
	0xe61136f2227e3b0a, 0x8fcac257558ee4e6, 0xb3bd72ed2af29e20,
	0xe0accfa875af45a8, 0x8c6c01c9498d8b89, 0xaf87023b9bf0ee6b,
	0xdb68c2ca82ed2a06, 0x892179be91d43a44, 0xab69d82e364948d4
};

static const int powers_ten_e[] = {
	-1203, -1200, -1196, -1193, -1190, -1186, -1183, -1180, -1176, -1173,
	-1170, -1166, -1163, -1160, -1156, -1153, -1150, -1146, -1143, -1140,
	-1136, -1133, -1130, -1127, -1123, -1120, -1117, -1113, -1110, -1107,
	-1103, -1100, -1097, -1093, -1090, -1087, -1083, -1080, -1077, -1073,
	-1070, -1067, -1063, -1060, -1057, -1053, -1050, -1047, -1043, -1040,
	-1037, -1034, -1030, -1027, -1024, -1020, -1017, -1014, -1010, -1007,
	-1004, -1000, -997, -994, -990, -987, -984, -980, -977, -974, -970,
	-967, -964, -960, -957, -954, -950, -947, -944, -940, -937, -934, -931,
	-927, -924, -921, -917, -914, -911, -907, -904, -901, -897, -894, -891,
	-887, -884, -881, -877, -874, -871, -867, -864, -861, -857, -854, -851,
	-847, -844, -841, -838, -834, -831, -828, -824, -821, -818, -814, -811,
	-808, -804, -801, -798, -794, -791, -788, -784, -781, -778, -774, -771,
	-768, -764, -761, -758, -754, -751, -748, -744, -741, -738, -735, -731,
	-728, -725, -721, -718, -715, -711, -708, -705, -701, -698, -695, -691,
	-688, -685, -681, -678, -675, -671, -668, -665, -661, -658, -655, -651,
	-648, -645, -642, -638, -635, -632, -628, -625, -622, -618, -615, -612,
	-608, -605, -602, -598, -595, -592, -588, -585, -582, -578, -575, -572,
	-568, -565, -562, -558, -555, -552, -549, -545, -542, -539, -535, -532,
	-529, -525, -522, -519, -515, -512, -509, -505, -502, -499, -495, -492,
	-489, -485, -482, -479, -475, -472, -469, -465, -462, -459, -455, -452,
	-449, -446, -442, -439, -436, -432, -429, -426, -422, -419, -416, -412,
	-409, -406, -402, -399, -396, -392, -389, -386, -382, -379, -376, -372,
	-369, -366, -362, -359, -356, -353, -349, -346, -343, -339, -336, -333,
	-329, -326, -323, -319, -316, -313, -309, -306, -303, -299, -296, -293,
	-289, -286, -283, -279, -276, -273, -269, -266, -263, -259, -256, -253,
	-250, -246, -243, -240, -236, -233, -230, -226, -223, -220, -216, -213,
	-210, -206, -203, -200, -196, -193, -190, -186, -183, -180, -176, -173,
	-170, -166, -163, -160, -157, -153, -150, -147, -143, -140, -137, -133,
	-130, -127, -123, -120, -117, -113, -110, -107, -103, -100, -97, -93,
	-90, -87, -83, -80, -77, -73, -70, -67, -63, -60, -57, -54, -50, -47,
	-44, -40, -37, -34, -30, -27, -24, -20, -17, -14, -10, -7, -4, 0, 3, 6,
	10, 13, 16, 20, 23, 26, 30, 33, 36, 39, 43, 46, 49, 53, 56, 59, 63, 66,
	69, 73, 76, 79, 83, 86, 89, 93, 96, 99, 103, 106, 109, 113, 116, 119,
	123, 126, 129, 132, 136, 139, 142, 146, 149, 152, 156, 159, 162, 166,
	169, 172, 176, 179, 182, 186, 189, 192, 196, 199, 202, 206, 209, 212,
	216, 219, 222, 226, 229, 232, 235, 239, 242, 245, 249, 252, 255, 259,
	262, 265, 269, 272, 275, 279, 282, 285, 289, 292, 295, 299, 302, 305,
	309, 312, 315, 319, 322, 325, 328, 332, 335, 338, 342, 345, 348, 352,
	355, 358, 362, 365, 368, 372, 375, 378, 382, 385, 388, 392, 395, 398,
	402, 405, 408, 412, 415, 418, 422, 425, 428, 431, 435, 438, 441, 445,
	448, 451, 455, 458, 461, 465, 468, 471, 475, 478, 481, 485, 488, 491,
	495, 498, 501, 505, 508, 511, 515, 518, 521, 524, 528, 531, 534, 538,
	541, 544, 548, 551, 554, 558, 561, 564, 568, 571, 574, 578, 581, 584,
	588, 591, 594, 598, 601, 604, 608, 611, 614, 617, 621, 624, 627, 631,
	634, 637, 641, 644, 647, 651, 654, 657, 661, 664, 667, 671, 674, 677,
	681, 684, 687, 691, 694, 697, 701, 704, 707, 711, 714, 717, 720, 724,
	727, 730, 734, 737, 740, 744, 747, 750, 754, 757, 760, 764, 767, 770,
	774, 777, 780, 784, 787, 790, 794, 797, 800, 804, 807, 810, 813, 817,
	820, 823, 827, 830, 833, 837, 840, 843, 847, 850, 853, 857, 860, 863,
	867, 870, 873, 877, 880, 883, 887, 890, 893, 897, 900, 903, 907, 910,
	913, 916, 920, 923, 926, 930, 933, 936, 940, 943, 946, 950, 953, 956,
	960, 963, 966, 970, 973, 976, 980, 983, 986, 990, 993, 996, 1000, 1003,
	1006, 1009, 1013, 1016, 1019, 1023, 1026, 1029, 1033, 1036, 1039, 1043,
	1046, 1049, 1053, 1056, 1059, 1063, 1066, 1069, 1073, 1076
};

static diy_fp_t cached_power(int k)
{
	diy_fp_t res;
	int index = 343 + k;
	res.f = powers_ten[index];
	res.e = powers_ten_e[index];
	return res;
}

static int k_comp(int e, int alpha, int gamma) {
	return ceil((alpha-e+63) * D_1_LOG2_10);
}

static diy_fp_t minus(diy_fp_t x, diy_fp_t y)
{
	diy_fp_t r;
	assert(x.e == y.e);
	assert(x.f >= y.f);
	r.f = x.f - y.f;
	r.e = x.e;
	return r;
}

static diy_fp_t multiply(diy_fp_t x, diy_fp_t y)
{
	uint64_t a,b,c,d,ac,bc,ad,bd,tmp;
	diy_fp_t r;
	uint64_t M32 = 0xFFFFFFFF;
	a = x.f >> 32; b = x.f & M32;
	c = y.f >> 32; d = y.f & M32;
	ac = a*c; bc = b*c; ad = a*d; bd = b*d;
	tmp = (bd>>32) + (ad&M32) + (bc&M32);
	tmp += 1U << 31;
	r.f = ac+(ad>>32)+(bc>>32)+(tmp >>32);
	r.e = x.e + y.e + 64;
	return r;
}

static uint64_t double_to_uint64(double d)
{
	uint64_t n;
	memcpy(&n, &d, 8);
	return n;
}

#define DP_SIGNIFICAND_SIZE 52
#define DP_EXPONENT_BIAS (0x3FF + DP_SIGNIFICAND_SIZE)
#define DP_MIN_EXPONENT (-DP_EXPONENT_BIAS)
#define DP_EXPONENT_MASK 0x7FF0000000000000
#define DP_SIGNIFICAND_MASK 0x000FFFFFFFFFFFFF
#define DP_HIDDEN_BIT 0x0010000000000000

static diy_fp_t double2diy_fp(double d)
{
	uint64_t d64 = double_to_uint64(d);
	int biased_e = (d64 & DP_EXPONENT_MASK) >> DP_SIGNIFICAND_SIZE;
	uint64_t significand = (d64 & DP_SIGNIFICAND_MASK);
	diy_fp_t res;
	if (biased_e != 0) {
		res.f = significand + DP_HIDDEN_BIT;
		res.e = biased_e - DP_EXPONENT_BIAS;
	} else {
		res.f = significand;
		res.e = DP_MIN_EXPONENT + 1;
	}
	return res;
}

static diy_fp_t normalize_boundary(diy_fp_t in)
{
	diy_fp_t res = in;

	while (! (res.f & (DP_HIDDEN_BIT << 1))) {
		res.f <<= 1;
		res.e--;
	}

	res.f <<= (DIY_SIGNIFICAND_SIZE - DP_SIGNIFICAND_SIZE - 2);
	res.e = res.e - (DIY_SIGNIFICAND_SIZE - DP_SIGNIFICAND_SIZE - 2);
	return res;
}

static void normalized_boundaries(double d, diy_fp_t* out_m_minus, diy_fp_t* out_m_plus)
{
	diy_fp_t v = double2diy_fp(d);
	diy_fp_t pl, mi;
	int significand_is_zero = v.f == DP_HIDDEN_BIT;
	pl.f = (v.f << 1) + 1; pl.e = v.e - 1;
	pl = normalize_boundary(pl);
	if (significand_is_zero) {
		mi.f = (v.f << 2) - 1;
		mi.e = v.e - 2;
	} else {
		mi.f = (v.f << 1) - 1;
		mi.e = v.e - 1;
	}
	mi.f <<= mi.e - pl.e;
	mi.e = pl.e;
	*out_m_plus = pl;
	*out_m_minus = mi;
}

#define TEN2 100
static void digit_gen(diy_fp_t Mp, diy_fp_t delta, char* buffer, int* len, int* K)
{
	uint32_t div, p1;
	uint64_t p2;
	int d,kappa;
	diy_fp_t one;
	one.f = ((uint64_t) 1) << -Mp.e; one.e = Mp.e;
	p1 = Mp.f >> -one.e;
	p2 = Mp.f & (one.f - 1);
	*len = 0; kappa = 3; div = TEN2;
	while (kappa > 0) {
		d = p1 / div;
		if (d || *len) buffer[(*len)++] = '0' + d;
		p1 %= div; kappa--; div /= 10;
		if ((((uint64_t)p1)<<-one.e)+p2 <= delta.f) {
			*K += kappa; return;
		}
	}
	do {
		p2 *= 10;
		d = p2 >> -one.e;
		if (d || *len) buffer[(*len)++] = '0' + d;
		p2 &= one.f - 1; kappa--; delta.f *= 10;
	} while (p2 > delta.f);
	*K += kappa;
}

int
js_grisu2(double v, char *buffer, int *K)
{
	int length, mk;
	diy_fp_t w_m, w_p, c_mk, Wp, Wm, delta;
	int q = 64, alpha = -59, gamma = -56;
	normalized_boundaries(v, &w_m, &w_p);
	mk = k_comp(w_p.e + q, alpha, gamma);
	c_mk = cached_power(mk);
	Wp = multiply(w_p, c_mk);
	Wm = multiply(w_m, c_mk);
	Wm.f++; Wp.f--;
	delta = minus(Wp, Wm);
	*K = -mk;
	digit_gen(Wp, delta, buffer, &length, K);
	return length;
}

static int maxExponent = 511;

static double powersOf10[] = {
	10.,
	100.,
	1.0e4,
	1.0e8,
	1.0e16,
	1.0e32,
	1.0e64,
	1.0e128,
	1.0e256
};

double
js_strtod(const char *string, char **endPtr)
{
	int sign, expSign = FALSE;
	double fraction, dblExp, *d;
	register const char *p;
	register int c;

	int exp = 0;

	int fracExp = 0;

	int mantSize;

	int decPt;

	const char *pExp;

	p = string;
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
		p += 1;
	}
	if (*p == '-') {
		sign = TRUE;
		p += 1;
	} else {
		if (*p == '+') {
			p += 1;
		}
		sign = FALSE;
	}

	decPt = -1;
	for (mantSize = 0; ; mantSize += 1)
	{
		c = *p;
		if (!(c>='0'&&c<='9')) {
			if ((c != '.') || (decPt >= 0)) {
				break;
			}
			decPt = mantSize;
		}
		p += 1;
	}

	pExp = p;
	p -= mantSize;
	if (decPt < 0) {
		decPt = mantSize;
	} else {
		mantSize -= 1;
	}
	if (mantSize > 18) {
		fracExp = decPt - 18;
		mantSize = 18;
	} else {
		fracExp = decPt - mantSize;
	}
	if (mantSize == 0) {
		fraction = 0.0;
		p = string;
		goto done;
	} else {
		int frac1, frac2;
		frac1 = 0;
		for ( ; mantSize > 9; mantSize -= 1)
		{
			c = *p;
			p += 1;
			if (c == '.') {
				c = *p;
				p += 1;
			}
			frac1 = 10*frac1 + (c - '0');
		}
		frac2 = 0;
		for (; mantSize > 0; mantSize -= 1)
		{
			c = *p;
			p += 1;
			if (c == '.') {
				c = *p;
				p += 1;
			}
			frac2 = 10*frac2 + (c - '0');
		}
		fraction = (1.0e9 * frac1) + frac2;
	}

	p = pExp;
	if ((*p == 'E') || (*p == 'e')) {
		p += 1;
		if (*p == '-') {
			expSign = TRUE;
			p += 1;
		} else {
			if (*p == '+') {
				p += 1;
			}
			expSign = FALSE;
		}
		while ((*p >= '0') && (*p <= '9') && exp < INT_MAX/100) {
			exp = exp * 10 + (*p - '0');
			p += 1;
		}
		while ((*p >= '0') && (*p <= '9'))
			p += 1;
	}
	if (expSign) {
		exp = fracExp - exp;
	} else {
		exp = fracExp + exp;
	}

	if (exp < -maxExponent) {
		exp = maxExponent;
		expSign = TRUE;
		errno = ERANGE;
	} else if (exp > maxExponent) {
		exp = maxExponent;
		expSign = FALSE;
		errno = ERANGE;
	} else if (exp < 0) {
		expSign = TRUE;
		exp = -exp;
	} else {
		expSign = FALSE;
	}
	dblExp = 1.0;
	for (d = powersOf10; exp != 0; exp >>= 1, d += 1) {
		if (exp & 01) {
			dblExp *= *d;
		}
	}
	if (expSign) {
		fraction /= dblExp;
	} else {
		fraction *= dblExp;
	}

done:
	if (endPtr != NULL) {
		*endPtr = (char *) p;
	}

	if (sign) {
		return -fraction;
	}
	return fraction;
}

#define QQ(X) #X
#define Q(X) QQ(X)

static int jsB_stacktrace(js_State *J, int skip)
{
	char buf[256];
	int n = J->tracetop - skip;
	if (n <= 0)
		return 0;
	for (; n > 0; --n) {
		const char *name = J->trace[n].name;
		const char *file = J->trace[n].file;
		int line = J->trace[n].line;
		if (line > 0) {
			if (name[0])
				snprintf(buf, sizeof buf, "\n\tat %s (%s:%d)", name, file, line);
			else
				snprintf(buf, sizeof buf, "\n\tat %s:%d", file, line);
		} else
			snprintf(buf, sizeof buf, "\n\tat %s (%s)", name, file);
		js_pushstring(J, buf);
		if (n < J->tracetop - skip)
			js_concat(J);
	}
	return 1;
}

static void Ep_toString(js_State *J)
{
	const char *name = "Error";
	const char *message = "";

	if (!js_isobject(J, -1))
		js_typeerror(J, "not an object");

	if (js_hasproperty(J, 0, "name"))
		name = js_tostring(J, -1);
	if (js_hasproperty(J, 0, "message"))
		message = js_tostring(J, -1);

	if (name[0] == 0)
		js_pushstring(J, message);
	else if (message[0] == 0)
		js_pushstring(J, name);
	else {
		js_pushstring(J, name);
		js_pushstring(J, ": ");
		js_concat(J);
		js_pushstring(J, message);
		js_concat(J);
	}
}

static void Ep_get_stack(js_State *J)
{
	Ep_toString(J);
	js_getproperty(J, 0, "stackTrace");
	js_concat(J);
}

static int jsB_ErrorX(js_State *J, js_Object *prototype)
{
	js_pushobject(J, jsV_newobject(J, JS_CERROR, prototype));
	if (js_isdefined(J, 1)) {
		js_pushstring(J, js_tostring(J, 1));
		js_defproperty(J, -2, "message", JS_DONTENUM);
	}
	if (jsB_stacktrace(J, 1))
		js_defproperty(J, -2, "stackTrace", JS_DONTENUM);
	return 1;
}

static void js_newerrorx(js_State *J, const char *message, js_Object *prototype)
{
	js_pushobject(J, jsV_newobject(J, JS_CERROR, prototype));
	js_pushstring(J, message);
	js_setproperty(J, -2, "message");
	if (jsB_stacktrace(J, 0))
		js_setproperty(J, -2, "stackTrace");
}

#define DERROR(name, Name) \
	static void jsB_##Name(js_State *J) { \
		jsB_ErrorX(J, J->Name##_prototype); \
	} \
	void js_new##name(js_State *J, const char *s) { \
		js_newerrorx(J, s, J->Name##_prototype); \
	} \
	void js_##name(js_State *J, const char *fmt, ...) { \
		va_list ap; \
		char buf[256]; \
		va_start(ap, fmt); \
		vsnprintf(buf, sizeof buf, fmt, ap); \
		va_end(ap); \
		js_newerrorx(J, buf, J->Name##_prototype); \
		js_throw(J); \
	}

DERROR(error, Error)
DERROR(evalerror, EvalError)
DERROR(rangeerror, RangeError)
DERROR(referenceerror, ReferenceError)
DERROR(syntaxerror, SyntaxError)
DERROR(typeerror, TypeError)
DERROR(urierror, URIError)

#undef DERROR

void jsB_initerror(js_State *J)
{
	js_pushobject(J, J->Error_prototype);
	{
		jsB_props(J, "name", "Error");
		jsB_propf(J, "Error.prototype.toString", Ep_toString, 0);
		jsB_props(J, "message", "");

		js_newcfunction(J, Ep_get_stack, "stack", 0);
		js_pushnull(J);
		js_defaccessor(J, -3, "stack", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
	}
	js_newcconstructor(J, jsB_Error, jsB_Error, "Error", 1);
	js_defglobal(J, "Error", JS_DONTENUM);

	#define IERROR(NAME) \
		js_pushobject(J, J->NAME##_prototype); \
		jsB_props(J, "name", Q(NAME)); \
		js_newcconstructor(J, jsB_##NAME, jsB_##NAME, Q(NAME), 1); \
		js_defglobal(J, Q(NAME), JS_DONTENUM);

	IERROR(EvalError);
	IERROR(RangeError);
	IERROR(ReferenceError);
	IERROR(SyntaxError);
	IERROR(TypeError);
	IERROR(URIError);

	#undef IERROR
}

static void jsB_Function(js_State *J)
{
	int i, top = js_gettop(J);
	js_Buffer *sb = NULL;
	const char *body;
	js_Ast *parse;
	js_Function *fun;

	if (js_try(J)) {
		js_free(J, sb);
		jsP_freeparse(J);
		js_throw(J);
	}

	if (top > 2) {
		for (i = 1; i < top - 1; ++i) {
			if (i > 1)
				js_putc(J, &sb, ',');
			js_puts(J, &sb, js_tostring(J, i));
		}
		js_putc(J, &sb, ')');
		js_putc(J, &sb, 0);
	}

	body = js_isdefined(J, top - 1) ? js_tostring(J, top - 1) : "";

	parse = jsP_parsefunction(J, "[string]", sb ? sb->s : NULL, body);
	fun = jsC_compilefunction(J, parse);

	js_endtry(J);
	js_free(J, sb);
	jsP_freeparse(J);

	js_newfunction(J, fun, J->GE);
}

static void jsB_Function_prototype(js_State *J)
{
	js_pushundefined(J);
}

static void Fp_toString(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	js_Buffer *sb = NULL;
	int i;

	if (!js_iscallable(J, 0))
		js_typeerror(J, "not a function");

	if (self->type == JS_CFUNCTION || self->type == JS_CSCRIPT) {
		js_Function *F = self->u.f.function;

		if (js_try(J)) {
			js_free(J, sb);
			js_throw(J);
		}

		js_puts(J, &sb, "function ");
		js_puts(J, &sb, F->name);
		js_putc(J, &sb, '(');
		for (i = 0; i < F->numparams; ++i) {
			if (i > 0) js_putc(J, &sb, ',');
			js_puts(J, &sb, F->vartab[i]);
		}
		js_puts(J, &sb, ") { [byte code] }");
		js_putc(J, &sb, 0);

		js_pushstring(J, sb->s);
		js_endtry(J);
		js_free(J, sb);
	} else if (self->type == JS_CCFUNCTION) {
		if (js_try(J)) {
			js_free(J, sb);
			js_throw(J);
		}

		js_puts(J, &sb, "function ");
		js_puts(J, &sb, self->u.c.name);
		js_puts(J, &sb, "() { [native code] }");
		js_putc(J, &sb, 0);

		js_pushstring(J, sb->s);
		js_endtry(J);
		js_free(J, sb);
	} else {
		js_pushliteral(J, "function () { }");
	}
}

static void Fp_apply(js_State *J)
{
	int i, n;

	if (!js_iscallable(J, 0))
		js_typeerror(J, "not a function");

	js_copy(J, 0);
	js_copy(J, 1);

	if (js_isnull(J, 2) || js_isundefined(J, 2)) {
		n = 0;
	} else {
		n = js_getlength(J, 2);
		if (n < 0)
			n = 0;
		for (i = 0; i < n; ++i)
			js_getindex(J, 2, i);
	}

	js_call(J, n);
}

static void Fp_call(js_State *J)
{
	int i, top = js_gettop(J);

	if (!js_iscallable(J, 0))
		js_typeerror(J, "not a function");

	for (i = 0; i < top; ++i)
		js_copy(J, i);

	js_call(J, top - 2);
}

static void callbound(js_State *J)
{
	int top = js_gettop(J);
	int i, fun, args, n;

	fun = js_gettop(J);
	js_currentfunction(J);
	js_getproperty(J, fun, "__TargetFunction__");
	js_getproperty(J, fun, "__BoundThis__");

	args = js_gettop(J);
	js_getproperty(J, fun, "__BoundArguments__");
	n = js_getlength(J, args);
	if (n < 0)
		n = 0;
	for (i = 0; i < n; ++i)
		js_getindex(J, args, i);
	js_remove(J, args);

	for (i = 1; i < top; ++i)
		js_copy(J, i);

	js_call(J, n + top - 1);
}

static void constructbound(js_State *J)
{
	int top = js_gettop(J);
	int i, fun, args, n;

	fun = js_gettop(J);
	js_currentfunction(J);
	js_getproperty(J, fun, "__TargetFunction__");

	args = js_gettop(J);
	js_getproperty(J, fun, "__BoundArguments__");
	n = js_getlength(J, args);
	if (n < 0)
		n = 0;
	for (i = 0; i < n; ++i)
		js_getindex(J, args, i);
	js_remove(J, args);

	for (i = 1; i < top; ++i)
		js_copy(J, i);

	js_construct(J, n + top - 1);
}

static void Fp_bind(js_State *J)
{
	int i, top = js_gettop(J);
	int n;

	if (!js_iscallable(J, 0))
		js_typeerror(J, "not a function");

	n = js_getlength(J, 0);
	if (n > top - 2)
		n -= top - 2;
	else
		n = 0;

	js_getproperty(J, 0, "prototype");
	js_newcconstructor(J, callbound, constructbound, "[bind]", n);

	js_copy(J, 0);
	js_defproperty(J, -2, "__TargetFunction__", JS_READONLY | JS_DONTENUM | JS_DONTCONF);

	js_copy(J, 1);
	js_defproperty(J, -2, "__BoundThis__", JS_READONLY | JS_DONTENUM | JS_DONTCONF);

	js_newarray(J);
	for (i = 2; i < top; ++i) {
		js_copy(J, i);
		js_setindex(J, -2, i - 2);
	}
	js_defproperty(J, -2, "__BoundArguments__", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
}

void jsB_initfunction(js_State *J)
{
	J->Function_prototype->u.c.name = "Function.prototype";
	J->Function_prototype->u.c.function = jsB_Function_prototype;
	J->Function_prototype->u.c.constructor = NULL;
	J->Function_prototype->u.c.length = 0;

	js_pushobject(J, J->Function_prototype);
	{
		jsB_propf(J, "Function.prototype.toString", Fp_toString, 2);
		jsB_propf(J, "Function.prototype.apply", Fp_apply, 2);
		jsB_propf(J, "Function.prototype.call", Fp_call, 1);
		jsB_propf(J, "Function.prototype.bind", Fp_bind, 1);
	}
	js_newcconstructor(J, jsB_Function, jsB_Function, "Function", 1);
	js_defglobal(J, "Function", JS_DONTENUM);
}

static void jsG_freeenvironment(js_State *J, js_Environment *env)
{
	js_free(J, env);
}

static void jsG_freefunction(js_State *J, js_Function *fun)
{
	js_free(J, fun->funtab);
	js_free(J, fun->vartab);
	js_free(J, fun->code);
	js_free(J, fun);
}

static void jsG_freeproperty(js_State *J, js_Property *node)
{
	if (node->left->level) jsG_freeproperty(J, node->left);
	if (node->right->level) jsG_freeproperty(J, node->right);
	js_free(J, node);
}

static void jsG_freeiterator(js_State *J, js_Iterator *node)
{
	while (node) {
		js_Iterator *next = node->next;
		js_free(J, node);
		node = next;
	}
}

static void jsG_freeobject(js_State *J, js_Object *obj)
{
	if (obj->properties->level)
		jsG_freeproperty(J, obj->properties);
	if (obj->type == JS_CREGEXP) {
		js_free(J, obj->u.r.source);
		js_regfreex(J->alloc, J->actx, obj->u.r.prog);
	}
	if (obj->type == JS_CSTRING) {
		if (obj->u.s.string != obj->u.s.shrstr)
			js_free(J, obj->u.s.string);
	}
	if (obj->type == JS_CARRAY && obj->u.a.simple)
		js_free(J, obj->u.a.array);
	if (obj->type == JS_CITERATOR)
		jsG_freeiterator(J, obj->u.iter.head);
	if (obj->type == JS_CUSERDATA && obj->u.user.finalize)
		obj->u.user.finalize(J, obj->u.user.data);
	if (obj->type == JS_CCFUNCTION && obj->u.c.finalize)
		obj->u.c.finalize(J, obj->u.c.data);
	js_free(J, obj);
}

static void jsG_markobject(js_State *J, int mark, js_Object *obj)
{
	obj->gcmark = mark;
	obj->gcroot = J->gcroot;
	J->gcroot = obj;
}

static void jsG_markfunction(js_State *J, int mark, js_Function *fun)
{
	int i;
	fun->gcmark = mark;
	for (i = 0; i < fun->funlen; ++i)
		if (fun->funtab[i]->gcmark != mark)
			jsG_markfunction(J, mark, fun->funtab[i]);
}

static void jsG_markenvironment(js_State *J, int mark, js_Environment *env)
{
	do {
		env->gcmark = mark;
		if (env->variables->gcmark != mark)
			jsG_markobject(J, mark, env->variables);
		env = env->outer;
	} while (env && env->gcmark != mark);
}

static void jsG_markproperty(js_State *J, int mark, js_Property *node)
{
	if (node->left->level) jsG_markproperty(J, mark, node->left);
	if (node->right->level) jsG_markproperty(J, mark, node->right);

	if (node->value.t.type == JS_TMEMSTR && node->value.u.memstr->gcmark != mark)
		node->value.u.memstr->gcmark = mark;
	if (node->value.t.type == JS_TOBJECT && node->value.u.object->gcmark != mark)
		jsG_markobject(J, mark, node->value.u.object);
	if (node->getter && node->getter->gcmark != mark)
		jsG_markobject(J, mark, node->getter);
	if (node->setter && node->setter->gcmark != mark)
		jsG_markobject(J, mark, node->setter);
}

static void jsG_scanobject(js_State *J, int mark, js_Object *obj)
{
	if (obj->properties->level)
		jsG_markproperty(J, mark, obj->properties);
	if (obj->prototype && obj->prototype->gcmark != mark)
		jsG_markobject(J, mark, obj->prototype);
	if (obj->type == JS_CARRAY && obj->u.a.simple) {
		int i;
		for (i = 0; i < obj->u.a.flat_length; ++i) {
			js_Value *v = &obj->u.a.array[i];
			if (v->t.type == JS_TMEMSTR && v->u.memstr->gcmark != mark)
				v->u.memstr->gcmark = mark;
			if (v->t.type == JS_TOBJECT && v->u.object->gcmark != mark)
				jsG_markobject(J, mark, v->u.object);
		}
	}
	if (obj->type == JS_CITERATOR && obj->u.iter.target->gcmark != mark) {
		jsG_markobject(J, mark, obj->u.iter.target);
	}
	if (obj->type == JS_CFUNCTION || obj->type == JS_CSCRIPT) {
		if (obj->u.f.scope && obj->u.f.scope->gcmark != mark)
			jsG_markenvironment(J, mark, obj->u.f.scope);
		if (obj->u.f.function && obj->u.f.function->gcmark != mark)
			jsG_markfunction(J, mark, obj->u.f.function);
	}
}

static void jsG_markstack(js_State *J, int mark)
{
	js_Value *v = J->stack;
	int n = J->top;
	while (n--) {
		if (v->t.type == JS_TMEMSTR && v->u.memstr->gcmark != mark)
			v->u.memstr->gcmark = mark;
		if (v->t.type == JS_TOBJECT && v->u.object->gcmark != mark)
			jsG_markobject(J, mark, v->u.object);
		++v;
	}
}

void js_gc(js_State *J, int report)
{
	js_Function *fun, *nextfun, **prevnextfun;
	js_Object *obj, *nextobj, **prevnextobj;
	js_String *str, *nextstr, **prevnextstr;
	js_Environment *env, *nextenv, **prevnextenv;
	unsigned int nenv = 0, nfun = 0, nobj = 0, nstr = 0, nprop = 0;
	unsigned int genv = 0, gfun = 0, gobj = 0, gstr = 0, gprop = 0;
	int mark;
	int i;

	mark = J->gcmark = J->gcmark == 1 ? 2 : 1;

	jsG_markobject(J, mark, J->Object_prototype);
	jsG_markobject(J, mark, J->Array_prototype);
	jsG_markobject(J, mark, J->Function_prototype);
	jsG_markobject(J, mark, J->Boolean_prototype);
	jsG_markobject(J, mark, J->Number_prototype);
	jsG_markobject(J, mark, J->String_prototype);
	jsG_markobject(J, mark, J->RegExp_prototype);
	jsG_markobject(J, mark, J->Date_prototype);

	jsG_markobject(J, mark, J->Error_prototype);
	jsG_markobject(J, mark, J->EvalError_prototype);
	jsG_markobject(J, mark, J->RangeError_prototype);
	jsG_markobject(J, mark, J->ReferenceError_prototype);
	jsG_markobject(J, mark, J->SyntaxError_prototype);
	jsG_markobject(J, mark, J->TypeError_prototype);
	jsG_markobject(J, mark, J->URIError_prototype);

	jsG_markobject(J, mark, J->R);
	jsG_markobject(J, mark, J->G);

	jsG_markstack(J, mark);

	jsG_markenvironment(J, mark, J->E);
	jsG_markenvironment(J, mark, J->GE);
	for (i = 0; i < J->envtop; ++i)
		jsG_markenvironment(J, mark, J->envstack[i]);

	while ((obj = J->gcroot) != NULL) {
		J->gcroot = obj->gcroot;
		obj->gcroot = NULL;
		jsG_scanobject(J, mark, obj);
	}

	prevnextenv = &J->gcenv;
	for (env = J->gcenv; env; env = nextenv) {
		nextenv = env->gcnext;
		if (env->gcmark != mark) {
			*prevnextenv = nextenv;
			jsG_freeenvironment(J, env);
			++genv;
		} else {
			prevnextenv = &env->gcnext;
		}
		++nenv;
	}

	prevnextfun = &J->gcfun;
	for (fun = J->gcfun; fun; fun = nextfun) {
		nextfun = fun->gcnext;
		if (fun->gcmark != mark) {
			*prevnextfun = nextfun;
			jsG_freefunction(J, fun);
			++gfun;
		} else {
			prevnextfun = &fun->gcnext;
		}
		++nfun;
	}

	prevnextobj = &J->gcobj;
	for (obj = J->gcobj; obj; obj = nextobj) {
		nprop += obj->count;
		nextobj = obj->gcnext;
		if (obj->gcmark != mark) {
			gprop += obj->count;
			*prevnextobj = nextobj;
			jsG_freeobject(J, obj);
			++gobj;
		} else {
			prevnextobj = &obj->gcnext;
		}
		++nobj;
	}

	prevnextstr = &J->gcstr;
	for (str = J->gcstr; str; str = nextstr) {
		nextstr = str->gcnext;
		if (str->gcmark != mark) {
			*prevnextstr = nextstr;
			js_free(J, str);
			++gstr;
		} else {
			prevnextstr = &str->gcnext;
		}
		++nstr;
	}

	unsigned int ntot = nenv + nfun + nobj + nstr + nprop;
	unsigned int gtot = genv + gfun + gobj + gstr + gprop;
	unsigned int remaining = ntot - gtot;

	J->gccounter = remaining;
	J->gcthresh = remaining * JS_GCFACTOR;

	if (report) {
		char buf[256];
		snprintf(buf, sizeof buf, "garbage collected (%d%%): %d/%d envs, %d/%d funs, %d/%d objs, %d/%d props, %d/%d strs",
			100*gtot/ntot, genv, nenv, gfun, nfun, gobj, nobj, gprop, nprop, gstr, nstr);
		js_report(J, buf);
	}
}

void js_freestate(js_State *J)
{
	js_Function *fun, *nextfun;
	js_Object *obj, *nextobj;
	js_Environment *env, *nextenv;
	js_String *str, *nextstr;

	if (!J)
		return;

	for (env = J->gcenv; env; env = nextenv)
		nextenv = env->gcnext, jsG_freeenvironment(J, env);
	for (fun = J->gcfun; fun; fun = nextfun)
		nextfun = fun->gcnext, jsG_freefunction(J, fun);
	for (obj = J->gcobj; obj; obj = nextobj)
		nextobj = obj->gcnext, jsG_freeobject(J, obj);
	for (str = J->gcstr; str; str = nextstr)
		nextstr = str->gcnext, js_free(J, str);

	jsS_freestrings(J);

	js_free(J, J->lexbuf.text);
	J->alloc(J->actx, J->stack, 0);
	J->alloc(J->actx, J, 0);
}

void js_putc(js_State *J, js_Buffer **sbp, int c)
{
	js_Buffer *sb = *sbp;
	if (!sb) {
		sb = js_malloc(J, sizeof *sb);
		sb->n = 0;
		sb->m = sizeof sb->s;
		*sbp = sb;
	} else if (sb->n == sb->m) {
		sb = js_realloc(J, sb, (sb->m *= 2) + soffsetof(js_Buffer, s));
		*sbp = sb;
	}
	sb->s[sb->n++] = c;
}

void js_puts(js_State *J, js_Buffer **sb, const char *s)
{
	while (*s)
		js_putc(J, sb, *s++);
}

void js_putm(js_State *J, js_Buffer **sb, const char *s, const char *e)
{
	while (s < e)
		js_putc(J, sb, *s++);
}

struct js_StringNode
{
	js_StringNode *left, *right;
	int level;
	char string[1];
};

static js_StringNode jsS_sentinel = { &jsS_sentinel, &jsS_sentinel, 0, ""};

static js_StringNode *jsS_newstringnode(js_State *J, const char *string, const char **result)
{
	size_t n = strlen(string);
	if (n > JS_STRLIMIT)
		js_rangeerror(J, "invalid string length");
	js_StringNode *node = js_malloc(J, soffsetof(js_StringNode, string) + n + 1);
	node->left = node->right = &jsS_sentinel;
	node->level = 1;
	memcpy(node->string, string, n + 1);
	return *result = node->string, node;
}

static js_StringNode *jsS_skew(js_StringNode *node)
{
	if (node->left->level == node->level) {
		js_StringNode *temp = node;
		node = node->left;
		temp->left = node->right;
		node->right = temp;
	}
	return node;
}

static js_StringNode *jsS_split(js_StringNode *node)
{
	if (node->right->right->level == node->level) {
		js_StringNode *temp = node;
		node = node->right;
		temp->right = node->left;
		node->left = temp;
		++node->level;
	}
	return node;
}

static js_StringNode *jsS_insert(js_State *J, js_StringNode *node, const char *string, const char **result)
{
	if (node != &jsS_sentinel) {
		int c = strcmp(string, node->string);
		if (c < 0)
			node->left = jsS_insert(J, node->left, string, result);
		else if (c > 0)
			node->right = jsS_insert(J, node->right, string, result);
		else
			return *result = node->string, node;
		node = jsS_skew(node);
		node = jsS_split(node);
		return node;
	}
	return jsS_newstringnode(J, string, result);
}

static void dumpstringnode(js_StringNode *node, int level)
{
	int i;
	if (node->left != &jsS_sentinel)
		dumpstringnode(node->left, level + 1);
	printf("%d: ", node->level);
	for (i = 0; i < level; ++i)
		putchar('\t');
	printf("'%s'\n", node->string);
	if (node->right != &jsS_sentinel)
		dumpstringnode(node->right, level + 1);
}

void jsS_dumpstrings(js_State *J)
{
	js_StringNode *root = J->strings;
	printf("interned strings {\n");
	if (root && root != &jsS_sentinel)
		dumpstringnode(root, 1);
	printf("}\n");
}

static void jsS_freestringnode(js_State *J, js_StringNode *node)
{
	if (node->left != &jsS_sentinel) jsS_freestringnode(J, node->left);
	if (node->right != &jsS_sentinel) jsS_freestringnode(J, node->right);
	js_free(J, node);
}

void jsS_freestrings(js_State *J)
{
	if (J->strings && J->strings != &jsS_sentinel)
		jsS_freestringnode(J, J->strings);
}

const char *js_intern(js_State *J, const char *s)
{
	const char *result;
	if (!J->strings)
		J->strings = &jsS_sentinel;
	J->strings = jsS_insert(J, J->strings, s, &result);
	return result;
}

#ifndef js_utf_h
#define js_utf_h

typedef int Rune;

#define chartorune	jsU_chartorune
#define runetochar	jsU_runetochar
#define runelen		jsU_runelen

#define isalpharune	jsU_isalpharune
#define islowerrune	jsU_islowerrune
#define isupperrune	jsU_isupperrune
#define tolowerrune	jsU_tolowerrune
#define toupperrune	jsU_toupperrune
#define tolowerrune_full	jsU_tolowerrune_full
#define toupperrune_full	jsU_toupperrune_full

enum
{
	UTFmax		= 4,
	Runesync	= 0x80,
	Runeself	= 0x80,
	Runeerror	= 0xFFFD,
	Runemax		= 0x10FFFF,
};

int	chartorune(Rune *rune, const char *str);
int	runetochar(char *str, const Rune *rune);
int	runelen(int c);

int		isalpharune(Rune c);
int		islowerrune(Rune c);
int		isupperrune(Rune c);
Rune		tolowerrune(Rune c);
Rune		toupperrune(Rune c);
const Rune*	tolowerrune_full(Rune c);
const Rune*	toupperrune_full(Rune c);

#endif

JS_NORETURN static void jsY_error(js_State *J, const char *fmt, ...) JS_PRINTFLIKE(2,3);

static void jsY_error(js_State *J, const char *fmt, ...)
{
	va_list ap;
	char buf[512];
	char msgbuf[256];

	va_start(ap, fmt);
	vsnprintf(msgbuf, 256, fmt, ap);
	va_end(ap);

	snprintf(buf, 256, "%s:%d: ", J->filename, J->lexline);
	strcat(buf, msgbuf);

	js_newsyntaxerror(J, buf);
	js_throw(J);
}

static const char *tokenstring[] = {
	"(end-of-file)",
	"'\\x01'", "'\\x02'", "'\\x03'", "'\\x04'", "'\\x05'", "'\\x06'", "'\\x07'",
	"'\\x08'", "'\\x09'", "'\\x0A'", "'\\x0B'", "'\\x0C'", "'\\x0D'", "'\\x0E'", "'\\x0F'",
	"'\\x10'", "'\\x11'", "'\\x12'", "'\\x13'", "'\\x14'", "'\\x15'", "'\\x16'", "'\\x17'",
	"'\\x18'", "'\\x19'", "'\\x1A'", "'\\x1B'", "'\\x1C'", "'\\x1D'", "'\\x1E'", "'\\x1F'",
	"' '", "'!'", "'\"'", "'#'", "'$'", "'%'", "'&'", "'\\''",
	"'('", "')'", "'*'", "'+'", "','", "'-'", "'.'", "'/'",
	"'0'", "'1'", "'2'", "'3'", "'4'", "'5'", "'6'", "'7'",
	"'8'", "'9'", "':'", "';'", "'<'", "'='", "'>'", "'?'",
	"'@'", "'A'", "'B'", "'C'", "'D'", "'E'", "'F'", "'G'",
	"'H'", "'I'", "'J'", "'K'", "'L'", "'M'", "'N'", "'O'",
	"'P'", "'Q'", "'R'", "'S'", "'T'", "'U'", "'V'", "'W'",
	"'X'", "'Y'", "'Z'", "'['", "'\'", "']'", "'^'", "'_'",
	"'`'", "'a'", "'b'", "'c'", "'d'", "'e'", "'f'", "'g'",
	"'h'", "'i'", "'j'", "'k'", "'l'", "'m'", "'n'", "'o'",
	"'p'", "'q'", "'r'", "'s'", "'t'", "'u'", "'v'", "'w'",
	"'x'", "'y'", "'z'", "'{'", "'|'", "'}'", "'~'", "'\\x7F'",

	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

	"(identifier)", "(number)", "(string)", "(regexp)",

	"'<='", "'>='", "'=='", "'!='", "'==='", "'!=='",
	"'<<'", "'>>'", "'>>>'", "'&&'", "'||'",
	"'+='", "'-='", "'*='", "'/='", "'%='",
	"'<<='", "'>>='", "'>>>='", "'&='", "'|='", "'^='",
	"'++'", "'--'",

	"'break'", "'case'", "'catch'", "'continue'", "'debugger'",
	"'default'", "'delete'", "'do'", "'else'", "'false'", "'finally'", "'for'",
	"'function'", "'if'", "'in'", "'instanceof'", "'new'", "'null'", "'return'",
	"'switch'", "'this'", "'throw'", "'true'", "'try'", "'typeof'", "'var'",
	"'void'", "'while'", "'with'",
};

const char *jsY_tokenstring(int token)
{
	if (token >= 0 && token < (int)nelem(tokenstring))
		if (tokenstring[token])
			return tokenstring[token];
	return "<unknown>";
}

static const char *keywords[] = {
	"break", "case", "catch", "continue", "debugger", "default", "delete",
	"do", "else", "false", "finally", "for", "function", "if", "in",
	"instanceof", "new", "null", "return", "switch", "this", "throw",
	"true", "try", "typeof", "var", "void", "while", "with",
};

int jsY_findword(const char *s, const char **list, int num)
{
	int l = 0;
	int r = num - 1;
	while (l <= r) {
		int m = (l + r) >> 1;
		int c = strcmp(s, list[m]);
		if (c < 0)
			r = m - 1;
		else if (c > 0)
			l = m + 1;
		else
			return m;
	}
	return -1;
}

static int jsY_findkeyword(js_State *J, const char *s)
{
	int i = jsY_findword(s, keywords, nelem(keywords));
	if (i >= 0) {
		J->text = keywords[i];
		return TK_BREAK + i;
	}
	J->text = s;
	return TK_IDENTIFIER;
}

int jsY_iswhite(int c)
{
	return c == 0x9 || c == 0xB || c == 0xC || c == 0x20 || c == 0xA0 || c == 0xFEFF;
}

int jsY_isnewline(int c)
{
	return c == 0xA || c == 0xD || c == 0x2028 || c == 0x2029;
}

#ifndef isalpha
#define isalpha(c) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
#endif
#ifndef isdigit
#define isdigit(c) (c >= '0' && c <= '9')
#endif
#ifndef ishex
#define ishex(c) ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
#endif

static int jsY_isidentifierstart(int c)
{
	return isalpha(c) || c == '$' || c == '_' || isalpharune(c);
}

static int jsY_isidentifierpart(int c)
{
	return isdigit(c) || isalpha(c) || c == '$' || c == '_' || isalpharune(c);
}

static int jsY_isdec(int c)
{
	return isdigit(c);
}

int jsY_ishex(int c)
{
	return isdigit(c) || ishex(c);
}

int jsY_tohex(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 0xA;
	if (c >= 'A' && c <= 'F') return c - 'A' + 0xA;
	return 0;
}

static void jsY_next(js_State *J)
{
	Rune c;
	if (*J->source == 0) {
		J->lexchar = EOF;
		return;
	}
	J->source += chartorune(&c, J->source);

	if (c == '\r' && *J->source == '\n')
		++J->source;
	if (jsY_isnewline(c)) {
		J->line++;
		c = '\n';
	}
	J->lexchar = c;
}

#define jsY_accept(J, x) (J->lexchar == x ? (jsY_next(J), 1) : 0)

#define jsY_expect(J, x) if (!jsY_accept(J, x)) jsY_error(J, "expected '%c'", x)

static void jsY_unescape(js_State *J)
{
	if (jsY_accept(J, '\\')) {
		if (jsY_accept(J, 'u')) {
			int x = 0;
			if (!jsY_ishex(J->lexchar)) { goto error; } x |= jsY_tohex(J->lexchar) << 12; jsY_next(J);
			if (!jsY_ishex(J->lexchar)) { goto error; } x |= jsY_tohex(J->lexchar) << 8; jsY_next(J);
			if (!jsY_ishex(J->lexchar)) { goto error; } x |= jsY_tohex(J->lexchar) << 4; jsY_next(J);
			if (!jsY_ishex(J->lexchar)) { goto error; } x |= jsY_tohex(J->lexchar);
			J->lexchar = x;
			return;
		}
error:
		jsY_error(J, "unexpected escape sequence");
	}
}

static void textinit(js_State *J)
{
	if (!J->lexbuf.text) {
		J->lexbuf.cap = 4096;
		J->lexbuf.text = js_malloc(J, J->lexbuf.cap);
	}
	J->lexbuf.len = 0;
}

static void textpush(js_State *J, Rune c)
{
	int n, newcap;
	if (c == EOF)
		n = 1;
	else
		n = runelen(c);
	if (J->lexbuf.len + n > J->lexbuf.cap) {
		newcap = J->lexbuf.cap * 2;
		J->lexbuf.text = js_realloc(J, J->lexbuf.text, newcap);
		J->lexbuf.cap = newcap;
	}
	if (c == EOF)
		J->lexbuf.text[J->lexbuf.len++] = 0;
	else
		J->lexbuf.len += runetochar(J->lexbuf.text + J->lexbuf.len, &c);
}

static char *textend(js_State *J)
{
	textpush(J, EOF);
	return J->lexbuf.text;
}

static void lexlinecomment(js_State *J)
{
	while (J->lexchar != EOF && J->lexchar != '\n')
		jsY_next(J);
}

static int lexcomment(js_State *J)
{

	while (J->lexchar != EOF) {
		if (jsY_accept(J, '*')) {
			while (J->lexchar == '*')
				jsY_next(J);
			if (jsY_accept(J, '/'))
				return 0;
		}
		else
			jsY_next(J);
	}
	return -1;
}

static double lexhex(js_State *J)
{
	double n = 0;
	if (!jsY_ishex(J->lexchar))
		jsY_error(J, "malformed hexadecimal number");
	while (jsY_ishex(J->lexchar)) {
		n = n * 16 + jsY_tohex(J->lexchar);
		jsY_next(J);
	}
	return n;
}

#if 0

static double lexinteger(js_State *J)
{
	double n = 0;
	if (!jsY_isdec(J->lexchar))
		jsY_error(J, "malformed number");
	while (jsY_isdec(J->lexchar)) {
		n = n * 10 + (J->lexchar - '0');
		jsY_next(J);
	}
	return n;
}

static double lexfraction(js_State *J)
{
	double n = 0;
	double d = 1;
	while (jsY_isdec(J->lexchar)) {
		n = n * 10 + (J->lexchar - '0');
		d = d * 10;
		jsY_next(J);
	}
	return n / d;
}

static double lexexponent(js_State *J)
{
	double sign;
	if (jsY_accept(J, 'e') || jsY_accept(J, 'E')) {
		if (jsY_accept(J, '-')) sign = -1;
		else if (jsY_accept(J, '+')) sign = 1;
		else sign = 1;
		return sign * lexinteger(J);
	}
	return 0;
}

static int lexnumber(js_State *J)
{
	double n;
	double e;

	if (jsY_accept(J, '0')) {
		if (jsY_accept(J, 'x') || jsY_accept(J, 'X')) {
			J->number = lexhex(J);
			return TK_NUMBER;
		}
		if (jsY_isdec(J->lexchar))
			jsY_error(J, "number with leading zero");
		n = 0;
		if (jsY_accept(J, '.'))
			n += lexfraction(J);
	} else if (jsY_accept(J, '.')) {
		if (!jsY_isdec(J->lexchar))
			return '.';
		n = lexfraction(J);
	} else {
		n = lexinteger(J);
		if (jsY_accept(J, '.'))
			n += lexfraction(J);
	}

	e = lexexponent(J);
	if (e < 0)
		n /= pow(10, -e);
	else if (e > 0)
		n *= pow(10, e);

	if (jsY_isidentifierstart(J->lexchar))
		jsY_error(J, "number with letter suffix");

	J->number = n;
	return TK_NUMBER;
}

#else

static int lexnumber(js_State *J)
{
	const char *s = J->source - 1;

	if (jsY_accept(J, '0')) {
		if (jsY_accept(J, 'x') || jsY_accept(J, 'X')) {
			J->number = lexhex(J);
			return TK_NUMBER;
		}
		if (jsY_isdec(J->lexchar))
			jsY_error(J, "number with leading zero");
		if (jsY_accept(J, '.')) {
			while (jsY_isdec(J->lexchar))
				jsY_next(J);
		}
	} else if (jsY_accept(J, '.')) {
		if (!jsY_isdec(J->lexchar))
			return '.';
		while (jsY_isdec(J->lexchar))
			jsY_next(J);
	} else {
		while (jsY_isdec(J->lexchar))
			jsY_next(J);
		if (jsY_accept(J, '.')) {
			while (jsY_isdec(J->lexchar))
				jsY_next(J);
		}
	}

	if (jsY_accept(J, 'e') || jsY_accept(J, 'E')) {
		if (J->lexchar == '-' || J->lexchar == '+')
			jsY_next(J);
		if (jsY_isdec(J->lexchar))
			while (jsY_isdec(J->lexchar))
				jsY_next(J);
		else
			jsY_error(J, "missing exponent");
	}

	if (jsY_isidentifierstart(J->lexchar))
		jsY_error(J, "number with letter suffix");

	J->number = js_strtod(s, NULL);
	return TK_NUMBER;
}

#endif

static int lexescape(js_State *J)
{
	int x = 0;

	if (jsY_accept(J, '\n'))
		return 0;

	switch (J->lexchar) {
	case EOF: jsY_error(J, "unterminated escape sequence");
	case 'u':
		jsY_next(J);
		if (!jsY_ishex(J->lexchar)) return 1; else { x |= jsY_tohex(J->lexchar) << 12; jsY_next(J); }
		if (!jsY_ishex(J->lexchar)) return 1; else { x |= jsY_tohex(J->lexchar) << 8; jsY_next(J); }
		if (!jsY_ishex(J->lexchar)) return 1; else { x |= jsY_tohex(J->lexchar) << 4; jsY_next(J); }
		if (!jsY_ishex(J->lexchar)) return 1; else { x |= jsY_tohex(J->lexchar); jsY_next(J); }
		textpush(J, x);
		break;
	case 'x':
		jsY_next(J);
		if (!jsY_ishex(J->lexchar)) return 1; else { x |= jsY_tohex(J->lexchar) << 4; jsY_next(J); }
		if (!jsY_ishex(J->lexchar)) return 1; else { x |= jsY_tohex(J->lexchar); jsY_next(J); }
		textpush(J, x);
		break;
	case '0': textpush(J, 0); jsY_next(J); break;
	case '\\': textpush(J, '\\'); jsY_next(J); break;
	case '\'': textpush(J, '\''); jsY_next(J); break;
	case '"': textpush(J, '"'); jsY_next(J); break;
	case 'b': textpush(J, '\b'); jsY_next(J); break;
	case 'f': textpush(J, '\f'); jsY_next(J); break;
	case 'n': textpush(J, '\n'); jsY_next(J); break;
	case 'r': textpush(J, '\r'); jsY_next(J); break;
	case 't': textpush(J, '\t'); jsY_next(J); break;
	case 'v': textpush(J, '\v'); jsY_next(J); break;
	default: textpush(J, J->lexchar); jsY_next(J); break;
	}
	return 0;
}

static int lexstring(js_State *J)
{
	const char *s;

	int q = J->lexchar;
	jsY_next(J);

	textinit(J);

	while (J->lexchar != q) {
		if (J->lexchar == EOF || J->lexchar == '\n')
			jsY_error(J, "string not terminated");
		if (jsY_accept(J, '\\')) {
			if (lexescape(J))
				jsY_error(J, "malformed escape sequence");
		} else {
			textpush(J, J->lexchar);
			jsY_next(J);
		}
	}
	jsY_expect(J, q);

	s = textend(J);

	J->text = s;
	return TK_STRING;
}

static int isregexpcontext(int last)
{
	switch (last) {
	case ']':
	case ')':
	case '}':
	case TK_IDENTIFIER:
	case TK_NUMBER:
	case TK_STRING:
	case TK_FALSE:
	case TK_NULL:
	case TK_THIS:
	case TK_TRUE:
		return 0;
	default:
		return 1;
	}
}

static int lexregexp(js_State *J)
{
	const char *s;
	int g, m, i, flags;
	int inclass = 0;

	textinit(J);

	while (J->lexchar != '/' || inclass) {
		if (J->lexchar == EOF || J->lexchar == '\n') {
			jsY_error(J, "regular expression not terminated");
		} else if (jsY_accept(J, '\\')) {
			if (jsY_accept(J, '/')) {
				textpush(J, '/');
			} else {
				textpush(J, '\\');
				if (J->lexchar == EOF || J->lexchar == '\n')
					jsY_error(J, "regular expression not terminated");
				textpush(J, J->lexchar);
				jsY_next(J);
			}
		} else {
			if (J->lexchar == '[' && !inclass)
				inclass = 1;
			if (J->lexchar == ']' && inclass)
				inclass = 0;
			textpush(J, J->lexchar);
			jsY_next(J);
		}
	}
	jsY_expect(J, '/');

	s = textend(J);

	g = i = m = 0;

	while (jsY_isidentifierpart(J->lexchar)) {
		if (jsY_accept(J, 'g')) ++g;
		else if (jsY_accept(J, 'i')) ++i;
		else if (jsY_accept(J, 'm')) ++m;
		else jsY_error(J, "illegal flag in regular expression: %c", J->lexchar);
	}

	if (g > 1 || i > 1 || m > 1)
		jsY_error(J, "duplicated flag in regular expression");

	J->text = s;

	flags = 0;
	if (g) flags |= JS_REGEXP_G;
	if (i) flags |= JS_REGEXP_I;
	if (m) flags |= JS_REGEXP_M;
	J->number = flags;
	return TK_REGEXP;
}

static int isnlthcontext(int last)
{
	switch (last) {
	case TK_BREAK:
	case TK_CONTINUE:
	case TK_RETURN:
	case TK_THROW:
		return 1;
	default:
		return 0;
	}
}

static int jsY_lexx(js_State *J)
{
	J->newline = 0;

	while (1) {
		J->lexline = J->line;

		while (jsY_iswhite(J->lexchar))
			jsY_next(J);

		if (jsY_accept(J, '\n')) {
			J->newline = 1;
			if (isnlthcontext(J->lasttoken))
				return ';';
			continue;
		}

		if (jsY_accept(J, '/')) {
			if (jsY_accept(J, '/')) {
				lexlinecomment(J);
				continue;
			} else if (jsY_accept(J, '*')) {
				if (lexcomment(J))
					jsY_error(J, "multi-line comment not terminated");
				continue;
			} else if (isregexpcontext(J->lasttoken)) {
				return lexregexp(J);
			} else if (jsY_accept(J, '=')) {
				return TK_DIV_ASS;
			} else {
				return '/';
			}
		}

		if (J->lexchar >= '0' && J->lexchar <= '9') {
			return lexnumber(J);
		}

		switch (J->lexchar) {
		case '(': jsY_next(J); return '(';
		case ')': jsY_next(J); return ')';
		case ',': jsY_next(J); return ',';
		case ':': jsY_next(J); return ':';
		case ';': jsY_next(J); return ';';
		case '?': jsY_next(J); return '?';
		case '[': jsY_next(J); return '[';
		case ']': jsY_next(J); return ']';
		case '{': jsY_next(J); return '{';
		case '}': jsY_next(J); return '}';
		case '~': jsY_next(J); return '~';

		case '\'':
		case '"':
			return lexstring(J);

		case '.':
			return lexnumber(J);

		case '<':
			jsY_next(J);
			if (jsY_accept(J, '<')) {
				if (jsY_accept(J, '='))
					return TK_SHL_ASS;
				return TK_SHL;
			}
			if (jsY_accept(J, '='))
				return TK_LE;
			return '<';

		case '>':
			jsY_next(J);
			if (jsY_accept(J, '>')) {
				if (jsY_accept(J, '>')) {
					if (jsY_accept(J, '='))
						return TK_USHR_ASS;
					return TK_USHR;
				}
				if (jsY_accept(J, '='))
					return TK_SHR_ASS;
				return TK_SHR;
			}
			if (jsY_accept(J, '='))
				return TK_GE;
			return '>';

		case '=':
			jsY_next(J);
			if (jsY_accept(J, '=')) {
				if (jsY_accept(J, '='))
					return TK_STRICTEQ;
				return TK_EQ;
			}
			return '=';

		case '!':
			jsY_next(J);
			if (jsY_accept(J, '=')) {
				if (jsY_accept(J, '='))
					return TK_STRICTNE;
				return TK_NE;
			}
			return '!';

		case '+':
			jsY_next(J);
			if (jsY_accept(J, '+'))
				return TK_INC;
			if (jsY_accept(J, '='))
				return TK_ADD_ASS;
			return '+';

		case '-':
			jsY_next(J);
			if (jsY_accept(J, '-'))
				return TK_DEC;
			if (jsY_accept(J, '='))
				return TK_SUB_ASS;
			return '-';

		case '*':
			jsY_next(J);
			if (jsY_accept(J, '='))
				return TK_MUL_ASS;
			return '*';

		case '%':
			jsY_next(J);
			if (jsY_accept(J, '='))
				return TK_MOD_ASS;
			return '%';

		case '&':
			jsY_next(J);
			if (jsY_accept(J, '&'))
				return TK_AND;
			if (jsY_accept(J, '='))
				return TK_AND_ASS;
			return '&';

		case '|':
			jsY_next(J);
			if (jsY_accept(J, '|'))
				return TK_OR;
			if (jsY_accept(J, '='))
				return TK_OR_ASS;
			return '|';

		case '^':
			jsY_next(J);
			if (jsY_accept(J, '='))
				return TK_XOR_ASS;
			return '^';

		case EOF:
			return 0;
		}

		jsY_unescape(J);
		if (jsY_isidentifierstart(J->lexchar)) {
			textinit(J);
			textpush(J, J->lexchar);

			jsY_next(J);
			jsY_unescape(J);
			while (jsY_isidentifierpart(J->lexchar)) {
				textpush(J, J->lexchar);
				jsY_next(J);
				jsY_unescape(J);
			}

			textend(J);

			return jsY_findkeyword(J, J->lexbuf.text);
		}

		if (J->lexchar >= 0x20 && J->lexchar <= 0x7E)
			jsY_error(J, "unexpected character: '%c'", J->lexchar);
		jsY_error(J, "unexpected character: \\u%04X", J->lexchar);
	}
}

void jsY_initlex(js_State *J, const char *filename, const char *source)
{
	J->filename = filename;
	J->source = source;
	J->line = 1;
	J->lasttoken = 0;
	jsY_next(J);
}

int jsY_lex(js_State *J)
{
	return J->lasttoken = jsY_lexx(J);
}

static int lexjsonnumber(js_State *J)
{
	const char *s = J->source - 1;

	if (J->lexchar == '-')
		jsY_next(J);

	if (J->lexchar == '0')
		jsY_next(J);
	else if (J->lexchar >= '1' && J->lexchar <= '9')
		while (isdigit(J->lexchar))
			jsY_next(J);
	else
		jsY_error(J, "unexpected non-digit");

	if (jsY_accept(J, '.')) {
		if (isdigit(J->lexchar))
			while (isdigit(J->lexchar))
				jsY_next(J);
		else
			jsY_error(J, "missing digits after decimal point");
	}

	if (jsY_accept(J, 'e') || jsY_accept(J, 'E')) {
		if (J->lexchar == '-' || J->lexchar == '+')
			jsY_next(J);
		if (isdigit(J->lexchar))
			while (isdigit(J->lexchar))
				jsY_next(J);
		else
			jsY_error(J, "missing digits after exponent indicator");
	}

	J->number = js_strtod(s, NULL);
	return TK_NUMBER;
}

static int lexjsonescape(js_State *J)
{
	int x = 0;

	switch (J->lexchar) {
	default: jsY_error(J, "invalid escape sequence");
	case 'u':
		jsY_next(J);
		if (!jsY_ishex(J->lexchar)) return 1; else { x |= jsY_tohex(J->lexchar) << 12; jsY_next(J); }
		if (!jsY_ishex(J->lexchar)) return 1; else { x |= jsY_tohex(J->lexchar) << 8; jsY_next(J); }
		if (!jsY_ishex(J->lexchar)) return 1; else { x |= jsY_tohex(J->lexchar) << 4; jsY_next(J); }
		if (!jsY_ishex(J->lexchar)) return 1; else { x |= jsY_tohex(J->lexchar); jsY_next(J); }
		textpush(J, x);
		break;
	case '"': textpush(J, '"'); jsY_next(J); break;
	case '\\': textpush(J, '\\'); jsY_next(J); break;
	case '/': textpush(J, '/'); jsY_next(J); break;
	case 'b': textpush(J, '\b'); jsY_next(J); break;
	case 'f': textpush(J, '\f'); jsY_next(J); break;
	case 'n': textpush(J, '\n'); jsY_next(J); break;
	case 'r': textpush(J, '\r'); jsY_next(J); break;
	case 't': textpush(J, '\t'); jsY_next(J); break;
	}
	return 0;
}

static int lexjsonstring(js_State *J)
{
	const char *s;

	textinit(J);

	while (J->lexchar != '"') {
		if (J->lexchar == EOF)
			jsY_error(J, "unterminated string");
		else if (J->lexchar < 32)
			jsY_error(J, "invalid control character in string");
		else if (jsY_accept(J, '\\'))
			lexjsonescape(J);
		else {
			textpush(J, J->lexchar);
			jsY_next(J);
		}
	}
	jsY_expect(J, '"');

	s = textend(J);

	J->text = s;
	return TK_STRING;
}

int jsY_lexjson(js_State *J)
{
	while (1) {
		J->lexline = J->line;

		while (jsY_iswhite(J->lexchar) || J->lexchar == '\n')
			jsY_next(J);

		if ((J->lexchar >= '0' && J->lexchar <= '9') || J->lexchar == '-')
			return lexjsonnumber(J);

		switch (J->lexchar) {
		case ',': jsY_next(J); return ',';
		case ':': jsY_next(J); return ':';
		case '[': jsY_next(J); return '[';
		case ']': jsY_next(J); return ']';
		case '{': jsY_next(J); return '{';
		case '}': jsY_next(J); return '}';

		case '"':
			jsY_next(J);
			return lexjsonstring(J);

		case 'f':
			jsY_next(J); jsY_expect(J, 'a'); jsY_expect(J, 'l'); jsY_expect(J, 's'); jsY_expect(J, 'e');
			return TK_FALSE;

		case 'n':
			jsY_next(J); jsY_expect(J, 'u'); jsY_expect(J, 'l'); jsY_expect(J, 'l');
			return TK_NULL;

		case 't':
			jsY_next(J); jsY_expect(J, 'r'); jsY_expect(J, 'u'); jsY_expect(J, 'e');
			return TK_TRUE;

		case EOF:
			return 0;
		}

		if (J->lexchar >= 0x20 && J->lexchar <= 0x7E)
			jsY_error(J, "unexpected character: '%c'", J->lexchar);
		jsY_error(J, "unexpected character: \\u%04X", J->lexchar);
	}
}

#if defined(_MSC_VER) && (_MSC_VER < 1700)
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

#include <time.h>

static double jsM_round(double x)
{
	if (isnan(x)) return x;
	if (isinf(x)) return x;
	if (x == 0) return x;
	if (x > 0 && x < 0.5) return 0;
	if (x < 0 && x >= -0.5) return -0;
	return floor(x + 0.5);
}

static void Math_abs(js_State *J)
{
	js_pushnumber(J, fabs(js_tonumber(J, 1)));
}

static void Math_acos(js_State *J)
{
	js_pushnumber(J, acos(js_tonumber(J, 1)));
}

static void Math_asin(js_State *J)
{
	js_pushnumber(J, asin(js_tonumber(J, 1)));
}

static void Math_atan(js_State *J)
{
	js_pushnumber(J, atan(js_tonumber(J, 1)));
}

static void Math_atan2(js_State *J)
{
	double y = js_tonumber(J, 1);
	double x = js_tonumber(J, 2);
	js_pushnumber(J, atan2(y, x));
}

static void Math_ceil(js_State *J)
{
	js_pushnumber(J, ceil(js_tonumber(J, 1)));
}

static void Math_cos(js_State *J)
{
	js_pushnumber(J, cos(js_tonumber(J, 1)));
}

static void Math_exp(js_State *J)
{
	js_pushnumber(J, exp(js_tonumber(J, 1)));
}

static void Math_floor(js_State *J)
{
	js_pushnumber(J, floor(js_tonumber(J, 1)));
}

static void Math_log(js_State *J)
{
	js_pushnumber(J, log(js_tonumber(J, 1)));
}

static void Math_pow(js_State *J)
{
	double x = js_tonumber(J, 1);
	double y = js_tonumber(J, 2);
	if (!isfinite(y) && fabs(x) == 1)
		js_pushnumber(J, NAN);
	else
		js_pushnumber(J, pow(x,y));
}

static void Math_random(js_State *J)
{

	J->seed = (uint64_t) J->seed * 48271 % 0x7fffffff;
	js_pushnumber(J, (double) J->seed / 0x7fffffff);
}

static void Math_init_random(js_State *J)
{

	J->seed = time(0) + 123;
	J->seed ^= J->seed << 13;
	J->seed ^= J->seed >> 17;
	J->seed ^= J->seed << 5;
	J->seed %= 0x7fffffff;
}

static void Math_round(js_State *J)
{
	double x = js_tonumber(J, 1);
	js_pushnumber(J, jsM_round(x));
}

static void Math_sin(js_State *J)
{
	js_pushnumber(J, sin(js_tonumber(J, 1)));
}

static void Math_sqrt(js_State *J)
{
	js_pushnumber(J, sqrt(js_tonumber(J, 1)));
}

static void Math_tan(js_State *J)
{
	js_pushnumber(J, tan(js_tonumber(J, 1)));
}

static void Math_max(js_State *J)
{
	int i, n = js_gettop(J);
	double x = -INFINITY;
	for (i = 1; i < n; ++i) {
		double y = js_tonumber(J, i);
		if (isnan(y)) {
			x = y;
			break;
		}
		if (signbit(x) == signbit(y))
			x = x > y ? x : y;
		else if (signbit(x))
			x = y;
	}
	js_pushnumber(J, x);
}

static void Math_min(js_State *J)
{
	int i, n = js_gettop(J);
	double x = INFINITY;
	for (i = 1; i < n; ++i) {
		double y = js_tonumber(J, i);
		if (isnan(y)) {
			x = y;
			break;
		}
		if (signbit(x) == signbit(y))
			x = x < y ? x : y;
		else if (signbit(y))
			x = y;
	}
	js_pushnumber(J, x);
}

void jsB_initmath(js_State *J)
{
	Math_init_random(J);
	js_pushobject(J, jsV_newobject(J, JS_CMATH, J->Object_prototype));
	{
		jsB_propn(J, "E", 2.7182818284590452354);
		jsB_propn(J, "LN10", 2.302585092994046);
		jsB_propn(J, "LN2", 0.6931471805599453);
		jsB_propn(J, "LOG2E", 1.4426950408889634);
		jsB_propn(J, "LOG10E", 0.4342944819032518);
		jsB_propn(J, "PI", 3.1415926535897932);
		jsB_propn(J, "SQRT1_2", 0.7071067811865476);
		jsB_propn(J, "SQRT2", 1.4142135623730951);

		jsB_propf(J, "Math.abs", Math_abs, 1);
		jsB_propf(J, "Math.acos", Math_acos, 1);
		jsB_propf(J, "Math.asin", Math_asin, 1);
		jsB_propf(J, "Math.atan", Math_atan, 1);
		jsB_propf(J, "Math.atan2", Math_atan2, 2);
		jsB_propf(J, "Math.ceil", Math_ceil, 1);
		jsB_propf(J, "Math.cos", Math_cos, 1);
		jsB_propf(J, "Math.exp", Math_exp, 1);
		jsB_propf(J, "Math.floor", Math_floor, 1);
		jsB_propf(J, "Math.log", Math_log, 1);
		jsB_propf(J, "Math.max", Math_max, 0);
		jsB_propf(J, "Math.min", Math_min, 0);
		jsB_propf(J, "Math.pow", Math_pow, 2);
		jsB_propf(J, "Math.random", Math_random, 0);
		jsB_propf(J, "Math.round", Math_round, 1);
		jsB_propf(J, "Math.sin", Math_sin, 1);
		jsB_propf(J, "Math.sqrt", Math_sqrt, 1);
		jsB_propf(J, "Math.tan", Math_tan, 1);
	}
	js_defglobal(J, "Math", JS_DONTENUM);
}

#if defined(_MSC_VER) && (_MSC_VER < 1700)
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

static void jsB_new_Number(js_State *J)
{
	js_newnumber(J, js_gettop(J) > 1 ? js_tonumber(J, 1) : 0);
}

static void jsB_Number(js_State *J)
{
	js_pushnumber(J, js_gettop(J) > 1 ? js_tonumber(J, 1) : 0);
}

static void Np_valueOf(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	if (self->type != JS_CNUMBER) js_typeerror(J, "not a number");
	js_pushnumber(J, self->u.number);
}

static void Np_toString(js_State *J)
{
	char buf[100];
	js_Object *self = js_toobject(J, 0);
	int radix = js_isundefined(J, 1) ? 10 : js_tointeger(J, 1);
	double x = 0;
	if (self->type != JS_CNUMBER)
		js_typeerror(J, "not a number");
	x = self->u.number;
	if (radix == 10) {
		js_pushstring(J, jsV_numbertostring(J, buf, x));
		return;
	}
	if (radix < 2 || radix > 36)
		js_rangeerror(J, "invalid radix");

	{
		static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
		double number = x;
		int sign = x < 0;
		js_Buffer *sb = NULL;
		uint64_t u, limit = ((uint64_t)1<<52);

		int ndigits, exp, point;

		if (number == 0) { js_pushstring(J, "0"); return; }
		if (isnan(number)) { js_pushstring(J, "NaN"); return; }
		if (isinf(number)) { js_pushstring(J, sign ? "-Infinity" : "Infinity"); return; }

		if (sign)
			number = -number;

		exp = 0;
		while (number * pow(radix, exp) > limit)
			--exp;
		while (number * pow(radix, exp+1) < limit)
			++exp;
		u = number * pow(radix, exp) + 0.5;

		while (u > 0 && (u % radix) == 0) {
			u /= radix;
			--exp;
		}

		ndigits = 0;
		while (u > 0) {
			buf[ndigits++] = digits[u % radix];
			u /= radix;
		}
		point = ndigits - exp;

		if (js_try(J)) {
			js_free(J, sb);
			js_throw(J);
		}

		if (sign)
			js_putc(J, &sb, '-');

		if (point <= 0) {
			js_putc(J, &sb, '0');
			js_putc(J, &sb, '.');
			while (point++ < 0)
				js_putc(J, &sb, '0');
			while (ndigits-- > 0)
				js_putc(J, &sb, buf[ndigits]);
		} else {
			while (ndigits-- > 0) {
				js_putc(J, &sb, buf[ndigits]);
				if (--point == 0 && ndigits > 0)
					js_putc(J, &sb, '.');
			}
			while (point-- > 0)
				js_putc(J, &sb, '0');
		}

		js_putc(J, &sb, 0);
		js_pushstring(J, sb->s);

		js_endtry(J);
		js_free(J, sb);
	}
}

static void numtostr(js_State *J, const char *fmt, int w, double n)
{

	char buf[50], *e;
	sprintf(buf, fmt, w, n);
	e = strchr(buf, 'e');
	if (e) {
		int exp = atoi(e+1);
		sprintf(e, "e%+d", exp);
	}
	js_pushstring(J, buf);
}

static void Np_toFixed(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	int width = js_tointeger(J, 1);
	char buf[32];
	double x;
	if (self->type != JS_CNUMBER) js_typeerror(J, "not a number");
	if (width < 0) js_rangeerror(J, "precision %d out of range", width);
	if (width > 20) js_rangeerror(J, "precision %d out of range", width);
	x = self->u.number;
	if (isnan(x) || isinf(x) || x <= -1e21 || x >= 1e21)
		js_pushstring(J, jsV_numbertostring(J, buf, x));
	else
		numtostr(J, "%.*f", width, x);
}

static void Np_toExponential(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	int width = js_tointeger(J, 1);
	char buf[32];
	double x;
	if (self->type != JS_CNUMBER) js_typeerror(J, "not a number");
	if (width < 0) js_rangeerror(J, "precision %d out of range", width);
	if (width > 20) js_rangeerror(J, "precision %d out of range", width);
	x = self->u.number;
	if (isnan(x) || isinf(x))
		js_pushstring(J, jsV_numbertostring(J, buf, x));
	else
		numtostr(J, "%.*e", width, x);
}

static void Np_toPrecision(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	int width = js_tointeger(J, 1);
	char buf[32];
	double x;
	if (self->type != JS_CNUMBER) js_typeerror(J, "not a number");
	if (width < 1) js_rangeerror(J, "precision %d out of range", width);
	if (width > 21) js_rangeerror(J, "precision %d out of range", width);
	x = self->u.number;
	if (isnan(x) || isinf(x))
		js_pushstring(J, jsV_numbertostring(J, buf, x));
	else
		numtostr(J, "%.*g", width, x);
}

void jsB_initnumber(js_State *J)
{
	J->Number_prototype->u.number = 0;

	js_pushobject(J, J->Number_prototype);
	{
		jsB_propf(J, "Number.prototype.valueOf", Np_valueOf, 0);
		jsB_propf(J, "Number.prototype.toString", Np_toString, 1);
		jsB_propf(J, "Number.prototype.toLocaleString", Np_toString, 0);
		jsB_propf(J, "Number.prototype.toFixed", Np_toFixed, 1);
		jsB_propf(J, "Number.prototype.toExponential", Np_toExponential, 1);
		jsB_propf(J, "Number.prototype.toPrecision", Np_toPrecision, 1);
	}
	js_newcconstructor(J, jsB_Number, jsB_new_Number, "Number", 0);
	{
		jsB_propn(J, "MAX_VALUE", 1.7976931348623157e+308);
		jsB_propn(J, "MIN_VALUE", 5e-324);
		jsB_propn(J, "NaN", NAN);
		jsB_propn(J, "NEGATIVE_INFINITY", -INFINITY);
		jsB_propn(J, "POSITIVE_INFINITY", INFINITY);
	}
	js_defglobal(J, "Number", JS_DONTENUM);
}

static void jsB_new_Object(js_State *J)
{
	if (js_isundefined(J, 1) || js_isnull(J, 1))
		js_newobject(J);
	else
		js_pushobject(J, js_toobject(J, 1));
}

static void jsB_Object(js_State *J)
{
	if (js_isundefined(J, 1) || js_isnull(J, 1))
		js_newobject(J);
	else
		js_pushobject(J, js_toobject(J, 1));
}

static void Op_toString(js_State *J)
{
	if (js_isundefined(J, 0))
		js_pushliteral(J, "[object Undefined]");
	else if (js_isnull(J, 0))
		js_pushliteral(J, "[object Null]");
	else {
		js_Object *self = js_toobject(J, 0);
		switch (self->type) {
		case JS_COBJECT: js_pushliteral(J, "[object Object]"); break;
		case JS_CARRAY: js_pushliteral(J, "[object Array]"); break;
		case JS_CFUNCTION: js_pushliteral(J, "[object Function]"); break;
		case JS_CSCRIPT: js_pushliteral(J, "[object Function]"); break;
		case JS_CCFUNCTION: js_pushliteral(J, "[object Function]"); break;
		case JS_CERROR: js_pushliteral(J, "[object Error]"); break;
		case JS_CBOOLEAN: js_pushliteral(J, "[object Boolean]"); break;
		case JS_CNUMBER: js_pushliteral(J, "[object Number]"); break;
		case JS_CSTRING: js_pushliteral(J, "[object String]"); break;
		case JS_CREGEXP: js_pushliteral(J, "[object RegExp]"); break;
		case JS_CDATE: js_pushliteral(J, "[object Date]"); break;
		case JS_CMATH: js_pushliteral(J, "[object Math]"); break;
		case JS_CJSON: js_pushliteral(J, "[object JSON]"); break;
		case JS_CARGUMENTS: js_pushliteral(J, "[object Arguments]"); break;
		case JS_CITERATOR: js_pushliteral(J, "[object Iterator]"); break;
		case JS_CUSERDATA:
			js_pushliteral(J, "[object ");
			js_pushliteral(J, self->u.user.tag);
			js_concat(J);
			js_pushliteral(J, "]");
			js_concat(J);
			break;
		}
	}
}

static void Op_valueOf(js_State *J)
{
	js_copy(J, 0);
}

static void Op_hasOwnProperty(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	const char *name = js_tostring(J, 1);
	js_Property *ref;
	int k;

	if (self->type == JS_CSTRING) {
		if (js_isarrayindex(J, name, &k) && k >= 0 && k < self->u.s.length) {
			js_pushboolean(J, 1);
			return;
		}
	}

	if (self->type == JS_CARRAY && self->u.a.simple) {
		if (js_isarrayindex(J, name, &k) && k >= 0 && k < self->u.a.flat_length) {
			js_pushboolean(J, 1);
			return;
		}
	}

	ref = jsV_getownproperty(J, self, name);
	js_pushboolean(J, ref != NULL);
}

static void Op_isPrototypeOf(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	if (js_isobject(J, 1)) {
		js_Object *V = js_toobject(J, 1);
		do {
			V = V->prototype;
			if (V == self) {
				js_pushboolean(J, 1);
				return;
			}
		} while (V);
	}
	js_pushboolean(J, 0);
}

static void Op_propertyIsEnumerable(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	const char *name = js_tostring(J, 1);
	js_Property *ref = jsV_getownproperty(J, self, name);
	js_pushboolean(J, ref && !(ref->atts & JS_DONTENUM));
}

static void O_getPrototypeOf(js_State *J)
{
	js_Object *obj;
	if (!js_isobject(J, 1))
		js_typeerror(J, "not an object");
	obj = js_toobject(J, 1);
	if (obj->prototype)
		js_pushobject(J, obj->prototype);
	else
		js_pushnull(J);
}

static void O_getOwnPropertyDescriptor(js_State *J)
{
	js_Object *obj;
	js_Property *ref;
	if (!js_isobject(J, 1))
		js_typeerror(J, "not an object");
	obj = js_toobject(J, 1);
	ref = jsV_getproperty(J, obj, js_tostring(J, 2));
	if (!ref) {

		js_pushundefined(J);
	} else {
		js_newobject(J);
		if (!ref->getter && !ref->setter) {
			js_pushvalue(J, ref->value);
			js_defproperty(J, -2, "value", 0);
			js_pushboolean(J, !(ref->atts & JS_READONLY));
			js_defproperty(J, -2, "writable", 0);
		} else {
			if (ref->getter)
				js_pushobject(J, ref->getter);
			else
				js_pushundefined(J);
			js_defproperty(J, -2, "get", 0);
			if (ref->setter)
				js_pushobject(J, ref->setter);
			else
				js_pushundefined(J);
			js_defproperty(J, -2, "set", 0);
		}
		js_pushboolean(J, !(ref->atts & JS_DONTENUM));
		js_defproperty(J, -2, "enumerable", 0);
		js_pushboolean(J, !(ref->atts & JS_DONTCONF));
		js_defproperty(J, -2, "configurable", 0);
	}
}

static int O_getOwnPropertyNames_walk(js_State *J, js_Property *ref, int i)
{
	if (ref->left->level)
		i = O_getOwnPropertyNames_walk(J, ref->left, i);
	js_pushstring(J, ref->name);
	js_setindex(J, -2, i++);
	if (ref->right->level)
		i = O_getOwnPropertyNames_walk(J, ref->right, i);
	return i;
}

static void O_getOwnPropertyNames(js_State *J)
{
	js_Object *obj;
	char name[32];
	int k;
	int i;

	if (!js_isobject(J, 1))
		js_typeerror(J, "not an object");
	obj = js_toobject(J, 1);

	js_newarray(J);

	if (obj->properties->level)
		i = O_getOwnPropertyNames_walk(J, obj->properties, 0);
	else
		i = 0;

	if (obj->type == JS_CARRAY) {
		js_pushliteral(J, "length");
		js_setindex(J, -2, i++);
		if (obj->u.a.simple) {
			for (k = 0; k < obj->u.a.flat_length; ++k) {
				js_itoa(name, k);
				js_pushstring(J, name);
				js_setindex(J, -2, i++);
			}
		}
	}

	if (obj->type == JS_CSTRING) {
		js_pushliteral(J, "length");
		js_setindex(J, -2, i++);
		for (k = 0; k < obj->u.s.length; ++k) {
			js_itoa(name, k);
			js_pushstring(J, name);
			js_setindex(J, -2, i++);
		}
	}

	if (obj->type == JS_CREGEXP) {
		js_pushliteral(J, "source");
		js_setindex(J, -2, i++);
		js_pushliteral(J, "global");
		js_setindex(J, -2, i++);
		js_pushliteral(J, "ignoreCase");
		js_setindex(J, -2, i++);
		js_pushliteral(J, "multiline");
		js_setindex(J, -2, i++);
		js_pushliteral(J, "lastIndex");
		js_setindex(J, -2, i++);
	}
}

static void ToPropertyDescriptor(js_State *J, js_Object *obj, const char *name, js_Object *desc)
{
	int haswritable = 0;
	int hasvalue = 0;
	int enumerable = 0;
	int configurable = 0;
	int writable = 0;
	int atts = 0;

	js_pushobject(J, obj);
	js_pushobject(J, desc);

	if (js_hasproperty(J, -1, "writable")) {
		haswritable = 1;
		writable = js_toboolean(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, -1, "enumerable")) {
		enumerable = js_toboolean(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, -1, "configurable")) {
		configurable = js_toboolean(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, -1, "value")) {
		hasvalue = 1;
		js_defproperty(J, -3, name, 0);
	}

	if (!writable) atts |= JS_READONLY;
	if (!enumerable) atts |= JS_DONTENUM;
	if (!configurable) atts |= JS_DONTCONF;

	if (js_hasproperty(J, -1, "get")) {
		if (haswritable || hasvalue)
			js_typeerror(J, "value/writable and get/set attributes are exclusive");
	} else {
		js_pushundefined(J);
	}

	if (js_hasproperty(J, -2, "set")) {
		if (haswritable || hasvalue)
			js_typeerror(J, "value/writable and get/set attributes are exclusive");
	} else {
		js_pushundefined(J);
	}

	js_defaccessor(J, -4, name, atts);

	js_pop(J, 2);
}

static void O_defineProperty(js_State *J)
{
	if (!js_isobject(J, 1)) js_typeerror(J, "not an object");
	if (!js_isobject(J, 3)) js_typeerror(J, "not an object");
	ToPropertyDescriptor(J, js_toobject(J, 1), js_tostring(J, 2), js_toobject(J, 3));
	js_copy(J, 1);
}

static int O_defineProperties_walk(js_State *J, js_Property *ref, int i)
{
	if (ref->left->level)
		i = O_defineProperties_walk(J, ref->left, i);
	if (!(ref->atts & JS_DONTENUM)) {
		if (ref->value.t.type != JS_TOBJECT)
			js_typeerror(J, "not an object");
		js_pushstring(J, ref->name);
		js_setindex(J, -2, i++);
	}
	if (ref->right->level)
		i = O_defineProperties_walk(J, ref->right, i);
	return i;
}

static void O_defineProperties_imp(js_State *J, js_Object *obj)
{
	js_Object *props;
	const char *name;
	int i, n;

	if (!js_isobject(J, 2)) js_typeerror(J, "not an object");

	props = js_toobject(J, 2);
	if (props->properties->level) {
		js_newarray(J);
		n = O_defineProperties_walk(J, props->properties, 0);
		for (i = 0; i < n; ++i) {
			js_getindex(J, -1, i);
			name = js_tostring(J, -1);
			if (js_hasproperty(J, 2, name)) {
				ToPropertyDescriptor(J, obj, name, js_toobject(J, -1));
				js_pop(J, 1);
			}
			js_pop(J, 1);
		}
		js_pop(J, 1);
	}
}

static void O_defineProperties(js_State *J)
{
	js_Object *obj;
	if (!js_isobject(J, 1)) js_typeerror(J, "not an object");
	obj = js_toobject(J, 1);
	O_defineProperties_imp(J, obj);
	js_copy(J, 1);
}

static void O_create(js_State *J)
{
	js_Object *obj;
	js_Object *proto;

	if (js_isobject(J, 1))
		proto = js_toobject(J, 1);
	else if (js_isnull(J, 1))
		proto = NULL;
	else
		js_typeerror(J, "not an object or null");

	obj = jsV_newobject(J, JS_COBJECT, proto);
	js_pushobject(J, obj);

	if (js_isdefined(J, 2)) {
		O_defineProperties_imp(J, obj);
	}
}

static int O_keys_walk(js_State *J, js_Property *ref, int i)
{
	if (ref->left->level)
		i = O_keys_walk(J, ref->left, i);
	if (!(ref->atts & JS_DONTENUM)) {
		js_pushstring(J, ref->name);
		js_setindex(J, -2, i++);
	}
	if (ref->right->level)
		i = O_keys_walk(J, ref->right, i);
	return i;
}

static void O_keys(js_State *J)
{
	js_Object *obj;
	char name[32];
	int i, k;

	if (!js_isobject(J, 1))
		js_typeerror(J, "not an object");
	obj = js_toobject(J, 1);

	js_newarray(J);

	if (obj->properties->level)
		i = O_keys_walk(J, obj->properties, 0);
	else
		i = 0;

	if (obj->type == JS_CSTRING) {
		for (k = 0; k < obj->u.s.length; ++k) {
			js_itoa(name, k);
			js_pushstring(J, name);
			js_setindex(J, -2, i++);
		}
	}

	if (obj->type == JS_CARRAY && obj->u.a.simple) {
		for (k = 0; k < obj->u.a.flat_length; ++k) {
			js_itoa(name, k);
			js_pushstring(J, name);
			js_setindex(J, -2, i++);
		}
	}
}

static void O_preventExtensions(js_State *J)
{
	js_Object *obj;
	if (!js_isobject(J, 1))
		js_typeerror(J, "not an object");
	obj = js_toobject(J, 1);
	jsR_unflattenarray(J, obj);
	obj->extensible = 0;
	js_copy(J, 1);
}

static void O_isExtensible(js_State *J)
{
	if (!js_isobject(J, 1))
		js_typeerror(J, "not an object");
	js_pushboolean(J, js_toobject(J, 1)->extensible);
}

static void O_seal_walk(js_State *J, js_Property *ref)
{
	if (ref->left->level)
		O_seal_walk(J, ref->left);
	ref->atts |= JS_DONTCONF;
	if (ref->right->level)
		O_seal_walk(J, ref->right);
}

static void O_seal(js_State *J)
{
	js_Object *obj;

	if (!js_isobject(J, 1))
		js_typeerror(J, "not an object");

	obj = js_toobject(J, 1);
	jsR_unflattenarray(J, obj);
	obj->extensible = 0;

	if (obj->properties->level)
		O_seal_walk(J, obj->properties);

	js_copy(J, 1);
}

static int O_isSealed_walk(js_State *J, js_Property *ref)
{
	if (ref->left->level)
		if (!O_isSealed_walk(J, ref->left))
			return 0;
	if (!(ref->atts & JS_DONTCONF))
		return 0;
	if (ref->right->level)
		if (!O_isSealed_walk(J, ref->right))
			return 0;
	return 1;
}

static void O_isSealed(js_State *J)
{
	js_Object *obj;

	if (!js_isobject(J, 1))
		js_typeerror(J, "not an object");

	obj = js_toobject(J, 1);
	if (obj->extensible) {
		js_pushboolean(J, 0);
		return;
	}

	if (obj->properties->level)
		js_pushboolean(J, O_isSealed_walk(J, obj->properties));
	else
		js_pushboolean(J, 1);
}

static void O_freeze_walk(js_State *J, js_Property *ref)
{
	if (ref->left->level)
		O_freeze_walk(J, ref->left);
	ref->atts |= JS_READONLY | JS_DONTCONF;
	if (ref->right->level)
		O_freeze_walk(J, ref->right);
}

static void O_freeze(js_State *J)
{
	js_Object *obj;

	if (!js_isobject(J, 1))
		js_typeerror(J, "not an object");

	obj = js_toobject(J, 1);
	jsR_unflattenarray(J, obj);
	obj->extensible = 0;

	if (obj->properties->level)
		O_freeze_walk(J, obj->properties);

	js_copy(J, 1);
}

static int O_isFrozen_walk(js_State *J, js_Property *ref)
{
	if (ref->left->level)
		if (!O_isFrozen_walk(J, ref->left))
			return 0;
	if (!(ref->atts & JS_READONLY))
		return 0;
	if (!(ref->atts & JS_DONTCONF))
		return 0;
	if (ref->right->level)
		if (!O_isFrozen_walk(J, ref->right))
			return 0;
	return 1;
}

static void O_isFrozen(js_State *J)
{
	js_Object *obj;

	if (!js_isobject(J, 1))
		js_typeerror(J, "not an object");

	obj = js_toobject(J, 1);

	if (obj->properties->level) {
		if (!O_isFrozen_walk(J, obj->properties)) {
			js_pushboolean(J, 0);
			return;
		}
	}

	js_pushboolean(J, !obj->extensible);
}

void jsB_initobject(js_State *J)
{
	js_pushobject(J, J->Object_prototype);
	{
		jsB_propf(J, "Object.prototype.toString", Op_toString, 0);
		jsB_propf(J, "Object.prototype.toLocaleString", Op_toString, 0);
		jsB_propf(J, "Object.prototype.valueOf", Op_valueOf, 0);
		jsB_propf(J, "Object.prototype.hasOwnProperty", Op_hasOwnProperty, 1);
		jsB_propf(J, "Object.prototype.isPrototypeOf", Op_isPrototypeOf, 1);
		jsB_propf(J, "Object.prototype.propertyIsEnumerable", Op_propertyIsEnumerable, 1);
	}
	js_newcconstructor(J, jsB_Object, jsB_new_Object, "Object", 1);
	{

		jsB_propf(J, "Object.getPrototypeOf", O_getPrototypeOf, 1);
		jsB_propf(J, "Object.getOwnPropertyDescriptor", O_getOwnPropertyDescriptor, 2);
		jsB_propf(J, "Object.getOwnPropertyNames", O_getOwnPropertyNames, 1);
		jsB_propf(J, "Object.create", O_create, 2);
		jsB_propf(J, "Object.defineProperty", O_defineProperty, 3);
		jsB_propf(J, "Object.defineProperties", O_defineProperties, 2);
		jsB_propf(J, "Object.seal", O_seal, 1);
		jsB_propf(J, "Object.freeze", O_freeze, 1);
		jsB_propf(J, "Object.preventExtensions", O_preventExtensions, 1);
		jsB_propf(J, "Object.isSealed", O_isSealed, 1);
		jsB_propf(J, "Object.isFrozen", O_isFrozen, 1);
		jsB_propf(J, "Object.isExtensible", O_isExtensible, 1);
		jsB_propf(J, "Object.keys", O_keys, 1);
	}
	js_defglobal(J, "Object", JS_DONTENUM);
}

int js_isnumberobject(js_State *J, int idx)
{
	return js_isobject(J, idx) && js_toobject(J, idx)->type == JS_CNUMBER;
}

int js_isstringobject(js_State *J, int idx)
{
	return js_isobject(J, idx) && js_toobject(J, idx)->type == JS_CSTRING;
}

int js_isbooleanobject(js_State *J, int idx)
{
	return js_isobject(J, idx) && js_toobject(J, idx)->type == JS_CBOOLEAN;
}

int js_isdateobject(js_State *J, int idx)
{
	return js_isobject(J, idx) && js_toobject(J, idx)->type == JS_CDATE;
}

static void jsonnext(js_State *J)
{
	J->lookahead = jsY_lexjson(J);
}

static int jsonaccept(js_State *J, int t)
{
	if (J->lookahead == t) {
		jsonnext(J);
		return 1;
	}
	return 0;
}

static void jsonexpect(js_State *J, int t)
{
	if (!jsonaccept(J, t))
		js_syntaxerror(J, "JSON: unexpected token: %s (expected %s)",
				jsY_tokenstring(J->lookahead), jsY_tokenstring(t));
}

static void jsonvalue(js_State *J)
{
	int i;

	switch (J->lookahead) {
	case TK_STRING:
		js_pushstring(J, J->text);
		jsonnext(J);
		break;

	case TK_NUMBER:
		js_pushnumber(J, J->number);
		jsonnext(J);
		break;

	case '{':
		js_newobject(J);
		jsonnext(J);
		if (jsonaccept(J, '}'))
			return;
		do {
			if (J->lookahead != TK_STRING)
				js_syntaxerror(J, "JSON: unexpected token: %s (expected string)", jsY_tokenstring(J->lookahead));
			js_pushstring(J, J->text);
			jsonnext(J);
			jsonexpect(J, ':');
			jsonvalue(J);
			js_setproperty(J, -3, js_tostring(J, -2));
			js_pop(J, 1);
		} while (jsonaccept(J, ','));
		jsonexpect(J, '}');
		break;

	case '[':
		js_newarray(J);
		jsonnext(J);
		i = 0;
		if (jsonaccept(J, ']'))
			return;
		do {
			jsonvalue(J);
			js_setindex(J, -2, i++);
		} while (jsonaccept(J, ','));
		jsonexpect(J, ']');
		break;

	case TK_TRUE:
		js_pushboolean(J, 1);
		jsonnext(J);
		break;

	case TK_FALSE:
		js_pushboolean(J, 0);
		jsonnext(J);
		break;

	case TK_NULL:
		js_pushnull(J);
		jsonnext(J);
		break;

	default:
		js_syntaxerror(J, "JSON: unexpected token: %s", jsY_tokenstring(J->lookahead));
	}
}

static void jsonrevive(js_State *J, const char *name)
{
	const char *key;
	char buf[32];

	js_getproperty(J, -1, name);

	if (js_isobject(J, -1)) {
		if (js_isarray(J, -1)) {
			int i = 0;
			int n = js_getlength(J, -1);
			for (i = 0; i < n; ++i) {
				jsonrevive(J, js_itoa(buf, i));
				if (js_isundefined(J, -1)) {
					js_pop(J, 1);
					js_delproperty(J, -1, buf);
				} else {
					js_setproperty(J, -2, buf);
				}
			}
		} else {
			js_pushiterator(J, -1, 1);
			while ((key = js_nextiterator(J, -1))) {
				js_rot2(J);
				jsonrevive(J, key);
				if (js_isundefined(J, -1)) {
					js_pop(J, 1);
					js_delproperty(J, -1, key);
				} else {
					js_setproperty(J, -2, key);
				}
				js_rot2(J);
			}
			js_pop(J, 1);
		}
	}

	js_copy(J, 2);
	js_copy(J, -3);
	js_pushstring(J, name);
	js_copy(J, -4);
	js_call(J, 2);
	js_rot2pop1(J);
}

static void JSON_parse(js_State *J)
{
	const char *source = js_tostring(J, 1);
	jsY_initlex(J, "JSON", source);
	jsonnext(J);

	if (js_iscallable(J, 2)) {
		js_newobject(J);
		jsonvalue(J);
		js_defproperty(J, -2, "", 0);
		jsonrevive(J, "");
	} else {
		jsonvalue(J);
	}
}

static void fmtnum(js_State *J, js_Buffer **sb, double n)
{
	if (isnan(n)) js_puts(J, sb, "null");
	else if (isinf(n)) js_puts(J, sb, "null");
	else if (n == 0) js_puts(J, sb, "0");
	else {
		char buf[40];
		js_puts(J, sb, jsV_numbertostring(J, buf, n));
	}
}

static void fmtstr(js_State *J, js_Buffer **sb, const char *s)
{
	static const char *HEX = "0123456789abcdef";
	int i, n;
	Rune c;
	js_putc(J, sb, '"');
	while (*s) {
		n = chartorune(&c, s);
		switch (c) {
		case '"': js_puts(J, sb, "\\\""); break;
		case '\\': js_puts(J, sb, "\\\\"); break;
		case '\b': js_puts(J, sb, "\\b"); break;
		case '\f': js_puts(J, sb, "\\f"); break;
		case '\n': js_puts(J, sb, "\\n"); break;
		case '\r': js_puts(J, sb, "\\r"); break;
		case '\t': js_puts(J, sb, "\\t"); break;
		default:
			if (c < ' ' || (c >= 0xd800 && c <= 0xdfff)) {
				js_putc(J, sb, '\\');
				js_putc(J, sb, 'u');
				js_putc(J, sb, HEX[(c>>12)&15]);
				js_putc(J, sb, HEX[(c>>8)&15]);
				js_putc(J, sb, HEX[(c>>4)&15]);
				js_putc(J, sb, HEX[c&15]);
			} else if (c < 128) {
				js_putc(J, sb, c);
			} else {
				for (i = 0; i < n; ++i)
					js_putc(J, sb, s[i]);
			}
			break;
		}
		s += n;
	}
	js_putc(J, sb, '"');
}

static void fmtindent(js_State *J, js_Buffer **sb, const char *gap, int level)
{
	js_putc(J, sb, '\n');
	while (level--)
		js_puts(J, sb, gap);
}

static int fmtvalue(js_State *J, js_Buffer **sb, const char *key, const char *gap, int level);

static int filterprop(js_State *J, const char *key)
{
	int i, n, found;

	if (js_isarray(J, 2)) {
		found = 0;
		n = js_getlength(J, 2);
		for (i = 0; i < n && !found; ++i) {
			js_getindex(J, 2, i);
			if (js_isstring(J, -1) || js_isnumber(J, -1) ||
				js_isstringobject(J, -1) || js_isnumberobject(J, -1))
				found = !strcmp(key, js_tostring(J, -1));
			js_pop(J, 1);
		}
		return found;
	}
	return 1;
}

static void fmtobject(js_State *J, js_Buffer **sb, js_Object *obj, const char *gap, int level)
{
	const char *key;
	int save;
	int i, n;

	n = js_gettop(J) - 1;
	for (i = 4; i < n; ++i)
		if (js_isobject(J, i))
			if (js_toobject(J, i) == js_toobject(J, -1))
				js_typeerror(J, "cyclic object value");

	n = 0;
	js_putc(J, sb, '{');
	js_pushiterator(J, -1, 1);
	while ((key = js_nextiterator(J, -1))) {
		if (filterprop(J, key)) {
			save = (*sb)->n;
			if (n) js_putc(J, sb, ',');
			if (gap) fmtindent(J, sb, gap, level + 1);
			fmtstr(J, sb, key);
			js_putc(J, sb, ':');
			if (gap)
				js_putc(J, sb, ' ');
			js_rot2(J);
			if (!fmtvalue(J, sb, key, gap, level + 1))
				(*sb)->n = save;
			else
				++n;
			js_rot2(J);
		}
	}
	js_pop(J, 1);
	if (gap && n) fmtindent(J, sb, gap, level);
	js_putc(J, sb, '}');
}

static void fmtarray(js_State *J, js_Buffer **sb, const char *gap, int level)
{
	int n, i;
	char buf[32];

	n = js_gettop(J) - 1;
	for (i = 4; i < n; ++i)
		if (js_isobject(J, i))
			if (js_toobject(J, i) == js_toobject(J, -1))
				js_typeerror(J, "cyclic object value");

	js_putc(J, sb, '[');
	n = js_getlength(J, -1);
	for (i = 0; i < n; ++i) {
		if (i) js_putc(J, sb, ',');
		if (gap) fmtindent(J, sb, gap, level + 1);
		if (!fmtvalue(J, sb, js_itoa(buf, i), gap, level + 1))
			js_puts(J, sb, "null");
	}
	if (gap && n) fmtindent(J, sb, gap, level);
	js_putc(J, sb, ']');
}

static int fmtvalue(js_State *J, js_Buffer **sb, const char *key, const char *gap, int level)
{

	js_getproperty(J, -1, key);

	if (js_isobject(J, -1)) {
		if (js_hasproperty(J, -1, "toJSON")) {
			if (js_iscallable(J, -1)) {
				js_copy(J, -2);
				js_pushstring(J, key);
				js_call(J, 1);
				js_rot2pop1(J);
			} else {
				js_pop(J, 1);
			}
		}
	}

	if (js_iscallable(J, 2)) {
		js_copy(J, 2);
		js_copy(J, -3);
		js_pushstring(J, key);
		js_copy(J, -4);
		js_call(J, 2);
		js_rot2pop1(J);
	}

	if (js_isobject(J, -1) && !js_iscallable(J, -1)) {
		js_Object *obj = js_toobject(J, -1);
		switch (obj->type) {
		case JS_CNUMBER: fmtnum(J, sb, obj->u.number); break;
		case JS_CSTRING: fmtstr(J, sb, obj->u.s.string); break;
		case JS_CBOOLEAN: js_puts(J, sb, obj->u.boolean ? "true" : "false"); break;
		case JS_CARRAY: fmtarray(J, sb, gap, level); break;
		default: fmtobject(J, sb, obj, gap, level); break;
		}
	}
	else if (js_isboolean(J, -1))
		js_puts(J, sb, js_toboolean(J, -1) ? "true" : "false");
	else if (js_isnumber(J, -1))
		fmtnum(J, sb, js_tonumber(J, -1));
	else if (js_isstring(J, -1))
		fmtstr(J, sb, js_tostring(J, -1));
	else if (js_isnull(J, -1))
		js_puts(J, sb, "null");
	else {
		js_pop(J, 1);
		return 0;
	}

	js_pop(J, 1);
	return 1;
}

static void JSON_stringify(js_State *J)
{
	js_Buffer *sb = NULL;
	char buf[12];

	const char * volatile gap;
	const char *s;
	int n;

	gap = NULL;

	if (js_isnumber(J, 3) || js_isnumberobject(J, 3)) {
		n = js_tointeger(J, 3);
		if (n < 0) n = 0;
		if (n > 10) n = 10;
		memset(buf, ' ', n);
		buf[n] = 0;
		if (n > 0) gap = buf;
	} else if (js_isstring(J, 3) || js_isstringobject(J, 3)) {
		s = js_tostring(J, 3);
		n = strlen(s);
		if (n > 10) n = 10;
		memcpy(buf, s, n);
		buf[n] = 0;
		if (n > 0) gap = buf;
	}

	if (js_try(J)) {
		js_free(J, sb);
		js_throw(J);
	}

	js_newobject(J);
	js_copy(J, 1);
	js_defproperty(J, -2, "", 0);
	if (!fmtvalue(J, &sb, "", gap, 0)) {
		js_pushundefined(J);
	} else {
		js_putc(J, &sb, 0);
		js_pushstring(J, sb ? sb->s : "");
		js_rot2pop1(J);
	}

	js_endtry(J);
	js_free(J, sb);
}

void jsB_initjson(js_State *J)
{
	js_pushobject(J, jsV_newobject(J, JS_CJSON, J->Object_prototype));
	{
		jsB_propf(J, "JSON.parse", JSON_parse, 2);
		jsB_propf(J, "JSON.stringify", JSON_stringify, 3);
	}
	js_defglobal(J, "JSON", JS_DONTENUM);
}

#define LIST(h)			jsP_newnode(J, AST_LIST, 0, h, 0, 0, 0)

#define EXP0(x)			jsP_newnode(J, EXP_ ## x, line, 0, 0, 0, 0)
#define EXP1(x,a)		jsP_newnode(J, EXP_ ## x, line, a, 0, 0, 0)
#define EXP2(x,a,b)		jsP_newnode(J, EXP_ ## x, line, a, b, 0, 0)
#define EXP3(x,a,b,c)		jsP_newnode(J, EXP_ ## x, line, a, b, c, 0)

#define STM0(x)			jsP_newnode(J, STM_ ## x, line, 0, 0, 0, 0)
#define STM1(x,a)		jsP_newnode(J, STM_ ## x, line, a, 0, 0, 0)
#define STM2(x,a,b)		jsP_newnode(J, STM_ ## x, line, a, b, 0, 0)
#define STM3(x,a,b,c)		jsP_newnode(J, STM_ ## x, line, a, b, c, 0)
#define STM4(x,a,b,c,d)		jsP_newnode(J, STM_ ## x, line, a, b, c, d)

static js_Ast *expression(js_State *J, int notin);
static js_Ast *assignment(js_State *J, int notin);
static js_Ast *memberexp(js_State *J);
static js_Ast *statement(js_State *J);
static js_Ast *funbody(js_State *J);

JS_NORETURN static void jsP_error(js_State *J, const char *fmt, ...) JS_PRINTFLIKE(2,3);

#define INCREC() if (++J->astdepth > JS_ASTLIMIT) jsP_error(J, "too much recursion")
#define DECREC() --J->astdepth
#define SAVEREC() int SAVE=J->astdepth
#define POPREC() J->astdepth=SAVE

static void jsP_error(js_State *J, const char *fmt, ...)
{
	va_list ap;
	char buf[512];
	char msgbuf[256];

	va_start(ap, fmt);
	vsnprintf(msgbuf, 256, fmt, ap);
	va_end(ap);

	snprintf(buf, 256, "%s:%d: ", J->filename, J->lexline);
	strcat(buf, msgbuf);

	js_newsyntaxerror(J, buf);
	js_throw(J);
}

static void jsP_warning(js_State *J, const char *fmt, ...)
{
	va_list ap;
	char buf[512];
	char msg[256];

	va_start(ap, fmt);
	vsnprintf(msg, sizeof msg, fmt, ap);
	va_end(ap);

	snprintf(buf, sizeof buf, "%s:%d: warning: %s", J->filename, J->lexline, msg);
	js_report(J, buf);
}

static js_Ast *jsP_newnode(js_State *J, enum js_AstType type, int line, js_Ast *a, js_Ast *b, js_Ast *c, js_Ast *d)
{
	js_Ast *node = js_malloc(J, sizeof *node);

	node->type = type;
	node->line = line;
	node->a = a;
	node->b = b;
	node->c = c;
	node->d = d;
	node->number = 0;
	node->string = NULL;
	node->jumps = NULL;
	node->casejump = 0;

	node->parent = NULL;
	if (a) a->parent = node;
	if (b) b->parent = node;
	if (c) c->parent = node;
	if (d) d->parent = node;

	node->gcnext = J->gcast;
	J->gcast = node;

	return node;
}

static js_Ast *jsP_list(js_Ast *head)
{

	js_Ast *prev = head, *node = head->b;
	while (node) {
		node->parent = prev;
		prev = node;
		node = node->b;
	}
	return head;
}

static js_Ast *jsP_newstrnode(js_State *J, enum js_AstType type, const char *s)
{
	js_Ast *node = jsP_newnode(J, type, J->lexline, 0, 0, 0, 0);
	node->string = js_intern(J, s);
	return node;
}

static js_Ast *jsP_newnumnode(js_State *J, enum js_AstType type, double n)
{
	js_Ast *node = jsP_newnode(J, type, J->lexline, 0, 0, 0, 0);
	node->number = n;
	return node;
}

static void jsP_freejumps(js_State *J, js_JumpList *node)
{
	while (node) {
		js_JumpList *next = node->next;
		js_free(J, node);
		node = next;
	}
}

void jsP_freeparse(js_State *J)
{
	js_Ast *node = J->gcast;
	while (node) {
		js_Ast *next = node->gcnext;
		jsP_freejumps(J, node->jumps);
		js_free(J, node);
		node = next;
	}
	J->gcast = NULL;
}

static void jsP_next(js_State *J)
{
	J->lookahead = jsY_lex(J);
}

#define jsP_accept(J,x) (J->lookahead == x ? (jsP_next(J), 1) : 0)

#define jsP_expect(J,x) if (!jsP_accept(J, x)) jsP_error(J, "unexpected token: %s (expected %s)", jsY_tokenstring(J->lookahead), jsY_tokenstring(x))

static void semicolon(js_State *J)
{
	if (J->lookahead == ';') {
		jsP_next(J);
		return;
	}
	if (J->newline || J->lookahead == '}' || J->lookahead == 0)
		return;
	jsP_error(J, "unexpected token: %s (expected ';')", jsY_tokenstring(J->lookahead));
}

static js_Ast *identifier(js_State *J)
{
	js_Ast *a;
	if (J->lookahead == TK_IDENTIFIER) {
		a = jsP_newstrnode(J, AST_IDENTIFIER, J->text);
		jsP_next(J);
		return a;
	}
	jsP_error(J, "unexpected token: %s (expected identifier)", jsY_tokenstring(J->lookahead));
}

static js_Ast *identifieropt(js_State *J)
{
	if (J->lookahead == TK_IDENTIFIER)
		return identifier(J);
	return NULL;
}

static js_Ast *identifiername(js_State *J)
{
	if (J->lookahead == TK_IDENTIFIER || J->lookahead >= TK_BREAK) {
		js_Ast *a = jsP_newstrnode(J, AST_IDENTIFIER, J->text);
		jsP_next(J);
		return a;
	}
	jsP_error(J, "unexpected token: %s (expected identifier or keyword)", jsY_tokenstring(J->lookahead));
}

static js_Ast *arrayelement(js_State *J)
{
	int line = J->lexline;
	if (J->lookahead == ',')
		return EXP0(ELISION);
	return assignment(J, 0);
}

static js_Ast *arrayliteral(js_State *J)
{
	js_Ast *head, *tail;
	if (J->lookahead == ']')
		return NULL;
	head = tail = LIST(arrayelement(J));
	while (jsP_accept(J, ',')) {
		if (J->lookahead != ']')
			tail = tail->b = LIST(arrayelement(J));
	}
	return jsP_list(head);
}

static js_Ast *propname(js_State *J)
{
	js_Ast *name;
	if (J->lookahead == TK_NUMBER) {
		name = jsP_newnumnode(J, EXP_NUMBER, J->number);
		jsP_next(J);
	} else if (J->lookahead == TK_STRING) {
		name = jsP_newstrnode(J, EXP_STRING, J->text);
		jsP_next(J);
	} else {
		name = identifiername(J);
	}
	return name;
}

static js_Ast *propassign(js_State *J)
{
	js_Ast *name, *value, *arg, *body;
	int line = J->lexline;

	name = propname(J);

	if (J->lookahead != ':' && name->type == AST_IDENTIFIER) {
		if (!strcmp(name->string, "get")) {
			name = propname(J);
			jsP_expect(J, '(');
			jsP_expect(J, ')');
			body = funbody(J);
			return EXP3(PROP_GET, name, NULL, body);
		}
		if (!strcmp(name->string, "set")) {
			name = propname(J);
			jsP_expect(J, '(');
			arg = identifier(J);
			jsP_expect(J, ')');
			body = funbody(J);
			return EXP3(PROP_SET, name, LIST(arg), body);
		}
	}

	jsP_expect(J, ':');
	value = assignment(J, 0);
	return EXP2(PROP_VAL, name, value);
}

static js_Ast *objectliteral(js_State *J)
{
	js_Ast *head, *tail;
	if (J->lookahead == '}')
		return NULL;
	head = tail = LIST(propassign(J));
	while (jsP_accept(J, ',')) {
		if (J->lookahead == '}')
			break;
		tail = tail->b = LIST(propassign(J));
	}
	return jsP_list(head);
}

static js_Ast *parameters(js_State *J)
{
	js_Ast *head, *tail;
	if (J->lookahead == ')')
		return NULL;
	head = tail = LIST(identifier(J));
	while (jsP_accept(J, ',')) {
		tail = tail->b = LIST(identifier(J));
	}
	return jsP_list(head);
}

static js_Ast *fundec(js_State *J, int line)
{
	js_Ast *a, *b, *c;
	a = identifier(J);
	jsP_expect(J, '(');
	b = parameters(J);
	jsP_expect(J, ')');
	c = funbody(J);
	return jsP_newnode(J, AST_FUNDEC, line, a, b, c, 0);
}

static js_Ast *funstm(js_State *J, int line)
{
	js_Ast *a, *b, *c;
	a = identifier(J);
	jsP_expect(J, '(');
	b = parameters(J);
	jsP_expect(J, ')');
	c = funbody(J);

	return STM1(VAR, LIST(EXP2(VAR, a, EXP3(FUN, a, b, c))));
}

static js_Ast *funexp(js_State *J, int line)
{
	js_Ast *a, *b, *c;
	a = identifieropt(J);
	jsP_expect(J, '(');
	b = parameters(J);
	jsP_expect(J, ')');
	c = funbody(J);
	return EXP3(FUN, a, b, c);
}

static js_Ast *primary(js_State *J)
{
	js_Ast *a;
	int line = J->lexline;

	if (J->lookahead == TK_IDENTIFIER) {
		a = jsP_newstrnode(J, EXP_IDENTIFIER, J->text);
		jsP_next(J);
		return a;
	}
	if (J->lookahead == TK_STRING) {
		a = jsP_newstrnode(J, EXP_STRING, J->text);
		jsP_next(J);
		return a;
	}
	if (J->lookahead == TK_REGEXP) {
		a = jsP_newstrnode(J, EXP_REGEXP, J->text);
		a->number = J->number;
		jsP_next(J);
		return a;
	}
	if (J->lookahead == TK_NUMBER) {
		a = jsP_newnumnode(J, EXP_NUMBER, J->number);
		jsP_next(J);
		return a;
	}

	if (jsP_accept(J, TK_THIS)) return EXP0(THIS);
	if (jsP_accept(J, TK_NULL)) return EXP0(NULL);
	if (jsP_accept(J, TK_TRUE)) return EXP0(TRUE);
	if (jsP_accept(J, TK_FALSE)) return EXP0(FALSE);
	if (jsP_accept(J, '{')) {
		a = EXP1(OBJECT, objectliteral(J));
		jsP_expect(J, '}');
		return a;
	}
	if (jsP_accept(J, '[')) {
		a = EXP1(ARRAY, arrayliteral(J));
		jsP_expect(J, ']');
		return a;
	}
	if (jsP_accept(J, '(')) {
		a = expression(J, 0);
		jsP_expect(J, ')');
		return a;
	}

	jsP_error(J, "unexpected token in expression: %s", jsY_tokenstring(J->lookahead));
}

static js_Ast *arguments(js_State *J)
{
	js_Ast *head, *tail;
	if (J->lookahead == ')')
		return NULL;
	head = tail = LIST(assignment(J, 0));
	while (jsP_accept(J, ',')) {
		tail = tail->b = LIST(assignment(J, 0));
	}
	return jsP_list(head);
}

static js_Ast *newexp(js_State *J)
{
	js_Ast *a, *b;
	int line = J->lexline;

	if (jsP_accept(J, TK_NEW)) {
		a = memberexp(J);
		if (jsP_accept(J, '(')) {
			b = arguments(J);
			jsP_expect(J, ')');
			return EXP2(NEW, a, b);
		}
		return EXP1(NEW, a);
	}

	if (jsP_accept(J, TK_FUNCTION))
		return funexp(J, line);

	return primary(J);
}

static js_Ast *memberexp(js_State *J)
{
	js_Ast *a = newexp(J);
	int line;
	SAVEREC();
loop:
	INCREC();
	line = J->lexline;
	if (jsP_accept(J, '.')) { a = EXP2(MEMBER, a, identifiername(J)); goto loop; }
	if (jsP_accept(J, '[')) { a = EXP2(INDEX, a, expression(J, 0)); jsP_expect(J, ']'); goto loop; }
	POPREC();
	return a;
}

static js_Ast *callexp(js_State *J)
{
	js_Ast *a = newexp(J);
	int line;
	SAVEREC();
loop:
	INCREC();
	line = J->lexline;
	if (jsP_accept(J, '.')) { a = EXP2(MEMBER, a, identifiername(J)); goto loop; }
	if (jsP_accept(J, '[')) { a = EXP2(INDEX, a, expression(J, 0)); jsP_expect(J, ']'); goto loop; }
	if (jsP_accept(J, '(')) { a = EXP2(CALL, a, arguments(J)); jsP_expect(J, ')'); goto loop; }
	POPREC();
	return a;
}

static js_Ast *postfix(js_State *J)
{
	js_Ast *a = callexp(J);
	int line = J->lexline;
	if (!J->newline && jsP_accept(J, TK_INC)) return EXP1(POSTINC, a);
	if (!J->newline && jsP_accept(J, TK_DEC)) return EXP1(POSTDEC, a);
	return a;
}

static js_Ast *unary(js_State *J)
{
	js_Ast *a;
	int line = J->lexline;
	INCREC();
	if (jsP_accept(J, TK_DELETE)) a = EXP1(DELETE, unary(J));
	else if (jsP_accept(J, TK_VOID)) a = EXP1(VOID, unary(J));
	else if (jsP_accept(J, TK_TYPEOF)) a = EXP1(TYPEOF, unary(J));
	else if (jsP_accept(J, TK_INC)) a = EXP1(PREINC, unary(J));
	else if (jsP_accept(J, TK_DEC)) a = EXP1(PREDEC, unary(J));
	else if (jsP_accept(J, '+')) a = EXP1(POS, unary(J));
	else if (jsP_accept(J, '-')) a = EXP1(NEG, unary(J));
	else if (jsP_accept(J, '~')) a = EXP1(BITNOT, unary(J));
	else if (jsP_accept(J, '!')) a = EXP1(LOGNOT, unary(J));
	else a = postfix(J);
	DECREC();
	return a;
}

static js_Ast *multiplicative(js_State *J)
{
	js_Ast *a = unary(J);
	int line;
	SAVEREC();
loop:
	INCREC();
	line = J->lexline;
	if (jsP_accept(J, '*')) { a = EXP2(MUL, a, unary(J)); goto loop; }
	if (jsP_accept(J, '/')) { a = EXP2(DIV, a, unary(J)); goto loop; }
	if (jsP_accept(J, '%')) { a = EXP2(MOD, a, unary(J)); goto loop; }
	POPREC();
	return a;
}

static js_Ast *additive(js_State *J)
{
	js_Ast *a = multiplicative(J);
	int line;
	SAVEREC();
loop:
	INCREC();
	line = J->lexline;
	if (jsP_accept(J, '+')) { a = EXP2(ADD, a, multiplicative(J)); goto loop; }
	if (jsP_accept(J, '-')) { a = EXP2(SUB, a, multiplicative(J)); goto loop; }
	POPREC();
	return a;
}

static js_Ast *shift(js_State *J)
{
	js_Ast *a = additive(J);
	int line;
	SAVEREC();
loop:
	INCREC();
	line = J->lexline;
	if (jsP_accept(J, TK_SHL)) { a = EXP2(SHL, a, additive(J)); goto loop; }
	if (jsP_accept(J, TK_SHR)) { a = EXP2(SHR, a, additive(J)); goto loop; }
	if (jsP_accept(J, TK_USHR)) { a = EXP2(USHR, a, additive(J)); goto loop; }
	POPREC();
	return a;
}

static js_Ast *relational(js_State *J, int notin)
{
	js_Ast *a = shift(J);
	int line;
	SAVEREC();
loop:
	INCREC();
	line = J->lexline;
	if (jsP_accept(J, '<')) { a = EXP2(LT, a, shift(J)); goto loop; }
	if (jsP_accept(J, '>')) { a = EXP2(GT, a, shift(J)); goto loop; }
	if (jsP_accept(J, TK_LE)) { a = EXP2(LE, a, shift(J)); goto loop; }
	if (jsP_accept(J, TK_GE)) { a = EXP2(GE, a, shift(J)); goto loop; }
	if (jsP_accept(J, TK_INSTANCEOF)) { a = EXP2(INSTANCEOF, a, shift(J)); goto loop; }
	if (!notin && jsP_accept(J, TK_IN)) { a = EXP2(IN, a, shift(J)); goto loop; }
	POPREC();
	return a;
}

static js_Ast *equality(js_State *J, int notin)
{
	js_Ast *a = relational(J, notin);
	int line;
	SAVEREC();
loop:
	INCREC();
	line = J->lexline;
	if (jsP_accept(J, TK_EQ)) { a = EXP2(EQ, a, relational(J, notin)); goto loop; }
	if (jsP_accept(J, TK_NE)) { a = EXP2(NE, a, relational(J, notin)); goto loop; }
	if (jsP_accept(J, TK_STRICTEQ)) { a = EXP2(STRICTEQ, a, relational(J, notin)); goto loop; }
	if (jsP_accept(J, TK_STRICTNE)) { a = EXP2(STRICTNE, a, relational(J, notin)); goto loop; }
	POPREC();
	return a;
}

static js_Ast *bitand(js_State *J, int notin)
{
	js_Ast *a = equality(J, notin);
	SAVEREC();
	int line = J->lexline;
	while (jsP_accept(J, '&')) {
		INCREC();
		a = EXP2(BITAND, a, equality(J, notin));
		line = J->lexline;
	}
	POPREC();
	return a;
}

static js_Ast *bitxor(js_State *J, int notin)
{
	js_Ast *a = bitand(J, notin);
	SAVEREC();
	int line = J->lexline;
	while (jsP_accept(J, '^')) {
		INCREC();
		a = EXP2(BITXOR, a, bitand(J, notin));
		line = J->lexline;
	}
	POPREC();
	return a;
}

static js_Ast *bitor(js_State *J, int notin)
{
	js_Ast *a = bitxor(J, notin);
	SAVEREC();
	int line = J->lexline;
	while (jsP_accept(J, '|')) {
		INCREC();
		a = EXP2(BITOR, a, bitxor(J, notin));
		line = J->lexline;
	}
	POPREC();
	return a;
}

static js_Ast *logand(js_State *J, int notin)
{
	js_Ast *a = bitor(J, notin);
	int line = J->lexline;
	if (jsP_accept(J, TK_AND)) {
		INCREC();
		a = EXP2(LOGAND, a, logand(J, notin));
		DECREC();
	}
	return a;
}

static js_Ast *logor(js_State *J, int notin)
{
	js_Ast *a = logand(J, notin);
	int line = J->lexline;
	if (jsP_accept(J, TK_OR)) {
		INCREC();
		a = EXP2(LOGOR, a, logor(J, notin));
		DECREC();
	}
	return a;
}

static js_Ast *conditional(js_State *J, int notin)
{
	js_Ast *a = logor(J, notin);
	int line = J->lexline;
	if (jsP_accept(J, '?')) {
		js_Ast *b, *c;
		INCREC();
		b = assignment(J, 0);
		jsP_expect(J, ':');
		c = assignment(J, notin);
		DECREC();
		return EXP3(COND, a, b, c);
	}
	return a;
}

static js_Ast *assignment(js_State *J, int notin)
{
	js_Ast *a = conditional(J, notin);
	int line = J->lexline;
	INCREC();
	if (jsP_accept(J, '=')) a = EXP2(ASS, a, assignment(J, notin));
	else if (jsP_accept(J, TK_MUL_ASS)) a = EXP2(ASS_MUL, a, assignment(J, notin));
	else if (jsP_accept(J, TK_DIV_ASS)) a = EXP2(ASS_DIV, a, assignment(J, notin));
	else if (jsP_accept(J, TK_MOD_ASS)) a = EXP2(ASS_MOD, a, assignment(J, notin));
	else if (jsP_accept(J, TK_ADD_ASS)) a = EXP2(ASS_ADD, a, assignment(J, notin));
	else if (jsP_accept(J, TK_SUB_ASS)) a = EXP2(ASS_SUB, a, assignment(J, notin));
	else if (jsP_accept(J, TK_SHL_ASS)) a = EXP2(ASS_SHL, a, assignment(J, notin));
	else if (jsP_accept(J, TK_SHR_ASS)) a = EXP2(ASS_SHR, a, assignment(J, notin));
	else if (jsP_accept(J, TK_USHR_ASS)) a = EXP2(ASS_USHR, a, assignment(J, notin));
	else if (jsP_accept(J, TK_AND_ASS)) a = EXP2(ASS_BITAND, a, assignment(J, notin));
	else if (jsP_accept(J, TK_XOR_ASS)) a = EXP2(ASS_BITXOR, a, assignment(J, notin));
	else if (jsP_accept(J, TK_OR_ASS)) a = EXP2(ASS_BITOR, a, assignment(J, notin));
	DECREC();
	return a;
}

static js_Ast *expression(js_State *J, int notin)
{
	js_Ast *a = assignment(J, notin);
	SAVEREC();
	int line = J->lexline;
	while (jsP_accept(J, ',')) {
		INCREC();
		a = EXP2(COMMA, a, assignment(J, notin));
		line = J->lexline;
	}
	POPREC();
	return a;
}

static js_Ast *vardec(js_State *J, int notin)
{
	js_Ast *a = identifier(J);
	int line = J->lexline;
	if (jsP_accept(J, '='))
		return EXP2(VAR, a, assignment(J, notin));
	return EXP1(VAR, a);
}

static js_Ast *vardeclist(js_State *J, int notin)
{
	js_Ast *head, *tail;
	head = tail = LIST(vardec(J, notin));
	while (jsP_accept(J, ','))
		tail = tail->b = LIST(vardec(J, notin));
	return jsP_list(head);
}

static js_Ast *statementlist(js_State *J)
{
	js_Ast *head, *tail;
	if (J->lookahead == '}' || J->lookahead == TK_CASE || J->lookahead == TK_DEFAULT)
		return NULL;
	head = tail = LIST(statement(J));
	while (J->lookahead != '}' && J->lookahead != TK_CASE && J->lookahead != TK_DEFAULT)
		tail = tail->b = LIST(statement(J));
	return jsP_list(head);
}

static js_Ast *caseclause(js_State *J)
{
	js_Ast *a, *b;
	int line = J->lexline;

	if (jsP_accept(J, TK_CASE)) {
		a = expression(J, 0);
		jsP_expect(J, ':');
		b = statementlist(J);
		return STM2(CASE, a, b);
	}

	if (jsP_accept(J, TK_DEFAULT)) {
		jsP_expect(J, ':');
		a = statementlist(J);
		return STM1(DEFAULT, a);
	}

	jsP_error(J, "unexpected token in switch: %s (expected 'case' or 'default')", jsY_tokenstring(J->lookahead));
}

static js_Ast *caselist(js_State *J)
{
	js_Ast *head, *tail;
	if (J->lookahead == '}')
		return NULL;
	head = tail = LIST(caseclause(J));
	while (J->lookahead != '}')
		tail = tail->b = LIST(caseclause(J));
	return jsP_list(head);
}

static js_Ast *block(js_State *J)
{
	js_Ast *a;
	int line = J->lexline;
	jsP_expect(J, '{');
	a = statementlist(J);
	jsP_expect(J, '}');
	return STM1(BLOCK, a);
}

static js_Ast *forexpression(js_State *J, int end)
{
	js_Ast *a = NULL;
	if (J->lookahead != end)
		a = expression(J, 0);
	jsP_expect(J, end);
	return a;
}

static js_Ast *forstatement(js_State *J, int line)
{
	js_Ast *a, *b, *c, *d;
	jsP_expect(J, '(');
	if (jsP_accept(J, TK_VAR)) {
		a = vardeclist(J, 1);
		if (jsP_accept(J, ';')) {
			b = forexpression(J, ';');
			c = forexpression(J, ')');
			d = statement(J);
			return STM4(FOR_VAR, a, b, c, d);
		}
		if (jsP_accept(J, TK_IN)) {
			b = expression(J, 0);
			jsP_expect(J, ')');
			c = statement(J);
			return STM3(FOR_IN_VAR, a, b, c);
		}
		jsP_error(J, "unexpected token in for-var-statement: %s", jsY_tokenstring(J->lookahead));
	}

	if (J->lookahead != ';')
		a = expression(J, 1);
	else
		a = NULL;
	if (jsP_accept(J, ';')) {
		b = forexpression(J, ';');
		c = forexpression(J, ')');
		d = statement(J);
		return STM4(FOR, a, b, c, d);
	}
	if (jsP_accept(J, TK_IN)) {
		b = expression(J, 0);
		jsP_expect(J, ')');
		c = statement(J);
		return STM3(FOR_IN, a, b, c);
	}
	jsP_error(J, "unexpected token in for-statement: %s", jsY_tokenstring(J->lookahead));
}

static js_Ast *statement(js_State *J)
{
	js_Ast *a, *b, *c, *d;
	js_Ast *stm;
	int line = J->lexline;

	INCREC();

	if (J->lookahead == '{') {
		stm = block(J);
	}

	else if (jsP_accept(J, TK_VAR)) {
		a = vardeclist(J, 0);
		semicolon(J);
		stm = STM1(VAR, a);
	}

	else if (jsP_accept(J, ';')) {
		stm = STM0(EMPTY);
	}

	else if (jsP_accept(J, TK_IF)) {
		jsP_expect(J, '(');
		a = expression(J, 0);
		jsP_expect(J, ')');
		b = statement(J);
		if (jsP_accept(J, TK_ELSE))
			c = statement(J);
		else
			c = NULL;
		stm = STM3(IF, a, b, c);
	}

	else if (jsP_accept(J, TK_DO)) {
		a = statement(J);
		jsP_expect(J, TK_WHILE);
		jsP_expect(J, '(');
		b = expression(J, 0);
		jsP_expect(J, ')');
		semicolon(J);
		stm = STM2(DO, a, b);
	}

	else if (jsP_accept(J, TK_WHILE)) {
		jsP_expect(J, '(');
		a = expression(J, 0);
		jsP_expect(J, ')');
		b = statement(J);
		stm = STM2(WHILE, a, b);
	}

	else if (jsP_accept(J, TK_FOR)) {
		stm = forstatement(J, line);
	}

	else if (jsP_accept(J, TK_CONTINUE)) {
		a = identifieropt(J);
		semicolon(J);
		stm = STM1(CONTINUE, a);
	}

	else if (jsP_accept(J, TK_BREAK)) {
		a = identifieropt(J);
		semicolon(J);
		stm = STM1(BREAK, a);
	}

	else if (jsP_accept(J, TK_RETURN)) {
		if (J->lookahead != ';' && J->lookahead != '}' && J->lookahead != 0)
			a = expression(J, 0);
		else
			a = NULL;
		semicolon(J);
		stm = STM1(RETURN, a);
	}

	else if (jsP_accept(J, TK_WITH)) {
		jsP_expect(J, '(');
		a = expression(J, 0);
		jsP_expect(J, ')');
		b = statement(J);
		stm = STM2(WITH, a, b);
	}

	else if (jsP_accept(J, TK_SWITCH)) {
		jsP_expect(J, '(');
		a = expression(J, 0);
		jsP_expect(J, ')');
		jsP_expect(J, '{');
		b = caselist(J);
		jsP_expect(J, '}');
		stm = STM2(SWITCH, a, b);
	}

	else if (jsP_accept(J, TK_THROW)) {
		a = expression(J, 0);
		semicolon(J);
		stm = STM1(THROW, a);
	}

	else if (jsP_accept(J, TK_TRY)) {
		a = block(J);
		b = c = d = NULL;
		if (jsP_accept(J, TK_CATCH)) {
			jsP_expect(J, '(');
			b = identifier(J);
			jsP_expect(J, ')');
			c = block(J);
		}
		if (jsP_accept(J, TK_FINALLY)) {
			d = block(J);
		}
		if (!b && !d)
			jsP_error(J, "unexpected token in try: %s (expected 'catch' or 'finally')", jsY_tokenstring(J->lookahead));
		stm = STM4(TRY, a, b, c, d);
	}

	else if (jsP_accept(J, TK_DEBUGGER)) {
		semicolon(J);
		stm = STM0(DEBUGGER);
	}

	else if (jsP_accept(J, TK_FUNCTION)) {
		jsP_warning(J, "function statements are not standard");
		stm = funstm(J, line);
	}

	else if (J->lookahead == TK_IDENTIFIER) {
		a = expression(J, 0);
		if (a->type == EXP_IDENTIFIER && jsP_accept(J, ':')) {
			a->type = AST_IDENTIFIER;
			b = statement(J);
			stm = STM2(LABEL, a, b);
		} else {
			semicolon(J);
			stm = a;
		}
	}

	else {
		stm = expression(J, 0);
		semicolon(J);
	}

	DECREC();
	return stm;
}

static js_Ast *scriptelement(js_State *J)
{
	int line = J->lexline;
	if (jsP_accept(J, TK_FUNCTION))
		return fundec(J, line);
	return statement(J);
}

static js_Ast *script(js_State *J, int terminator)
{
	js_Ast *head, *tail;
	if (J->lookahead == terminator)
		return NULL;
	head = tail = LIST(scriptelement(J));
	while (J->lookahead != terminator)
		tail = tail->b = LIST(scriptelement(J));
	return jsP_list(head);
}

static js_Ast *funbody(js_State *J)
{
	js_Ast *a;
	jsP_expect(J, '{');
	a = script(J, '}');
	jsP_expect(J, '}');
	return a;
}

static int toint32(double d)
{
	double two32 = 4294967296.0;
	double two31 = 2147483648.0;

	if (!isfinite(d) || d == 0)
		return 0;

	d = fmod(d, two32);
	d = d >= 0 ? floor(d) : ceil(d) + two32;
	if (d >= two31)
		return d - two32;
	else
		return d;
}

static unsigned int touint32(double d)
{
	return (unsigned int)toint32(d);
}

static int jsP_setnumnode(js_Ast *node, double x)
{
	node->type = EXP_NUMBER;
	node->number = x;
	node->a = node->b = node->c = node->d = NULL;
	return 1;
}

static int jsP_foldconst(js_Ast *node)
{
	double x, y;
	int a, b;

	if (node->type == AST_LIST) {
		while (node) {
			jsP_foldconst(node->a);
			node = node->b;
		}
		return 0;
	}

	if (node->type == EXP_NUMBER)
		return 1;

	a = node->a ? jsP_foldconst(node->a) : 0;
	b = node->b ? jsP_foldconst(node->b) : 0;
	if (node->c) jsP_foldconst(node->c);
	if (node->d) jsP_foldconst(node->d);

	if (a) {
		x = node->a->number;
		switch (node->type) {
		default: break;
		case EXP_NEG: return jsP_setnumnode(node, -x);
		case EXP_POS: return jsP_setnumnode(node, x);
		case EXP_BITNOT: return jsP_setnumnode(node, ~toint32(x));
		}

		if (b) {
			y = node->b->number;
			switch (node->type) {
			default: break;
			case EXP_MUL: return jsP_setnumnode(node, x * y);
			case EXP_DIV: return jsP_setnumnode(node, x / y);
			case EXP_MOD: return jsP_setnumnode(node, fmod(x, y));
			case EXP_ADD: return jsP_setnumnode(node, x + y);
			case EXP_SUB: return jsP_setnumnode(node, x - y);
			case EXP_SHL: return jsP_setnumnode(node, toint32(x) << (touint32(y) & 0x1F));
			case EXP_SHR: return jsP_setnumnode(node, toint32(x) >> (touint32(y) & 0x1F));
			case EXP_USHR: return jsP_setnumnode(node, touint32(x) >> (touint32(y) & 0x1F));
			case EXP_BITAND: return jsP_setnumnode(node, toint32(x) & toint32(y));
			case EXP_BITXOR: return jsP_setnumnode(node, toint32(x) ^ toint32(y));
			case EXP_BITOR: return jsP_setnumnode(node, toint32(x) | toint32(y));
			}
		}
	}

	return 0;
}

js_Ast *jsP_parse(js_State *J, const char *filename, const char *source)
{
	js_Ast *p;

	jsY_initlex(J, filename, source);
	jsP_next(J);
	J->astdepth = 0;
	p = script(J, 0);
	if (p)
		jsP_foldconst(p);

	return p;
}

js_Ast *jsP_parsefunction(js_State *J, const char *filename, const char *params, const char *body)
{
	js_Ast *p = NULL;
	int line = 0;
	if (params) {
		jsY_initlex(J, filename, params);
		jsP_next(J);
		J->astdepth = 0;
		p = parameters(J);
	}
	return EXP3(FUN, NULL, p, jsP_parse(J, filename, body));
}

#include <assert.h>

static js_Property sentinel = {
	&sentinel, &sentinel,
	0, 0,
	{ { {0}, JS_TUNDEFINED } },
	NULL, NULL, ""
};

static js_Property *newproperty(js_State *J, js_Object *obj, const char *name)
{
	int n = strlen(name) + 1;
	js_Property *node = js_malloc(J, offsetof(js_Property, name) + n);
	node->left = node->right = &sentinel;
	node->level = 1;
	node->atts = 0;
	node->value.t.type = JS_TUNDEFINED;
	node->value.u.number = 0;
	node->getter = NULL;
	node->setter = NULL;
	memcpy(node->name, name, n);
	++obj->count;
	++J->gccounter;
	return node;
}

static js_Property *lookup(js_Property *node, const char *name)
{
	while (node != &sentinel) {
		int c = strcmp(name, node->name);
		if (c == 0)
			return node;
		else if (c < 0)
			node = node->left;
		else
			node = node->right;
	}
	return NULL;
}

static js_Property *skew(js_Property *node)
{
	if (node->left->level == node->level) {
		js_Property *temp = node;
		node = node->left;
		temp->left = node->right;
		node->right = temp;
	}
	return node;
}

static js_Property *split(js_Property *node)
{
	if (node->right->right->level == node->level) {
		js_Property *temp = node;
		node = node->right;
		temp->right = node->left;
		node->left = temp;
		++node->level;
	}
	return node;
}

static js_Property *insert(js_State *J, js_Object *obj, js_Property *node, const char *name, js_Property **result)
{
	if (node != &sentinel) {
		int c = strcmp(name, node->name);
		if (c < 0)
			node->left = insert(J, obj, node->left, name, result);
		else if (c > 0)
			node->right = insert(J, obj, node->right, name, result);
		else
			return *result = node;
		node = skew(node);
		node = split(node);
		return node;
	}
	return *result = newproperty(J, obj, name);
}

static void freeproperty(js_State *J, js_Object *obj, js_Property *node)
{
	js_free(J, node);
	--obj->count;
}

static js_Property *unlinkproperty(js_Property *node, const char *name, js_Property **garbage)
{
	js_Property *temp, *a, *b;
	if (node != &sentinel) {
		int c = strcmp(name, node->name);
		if (c < 0) {
			node->left = unlinkproperty(node->left, name, garbage);
		} else if (c > 0) {
			node->right = unlinkproperty(node->right, name, garbage);
		} else {
			*garbage = node;
			if (node->left == &sentinel && node->right == &sentinel) {
				return &sentinel;
			}
			else if (node->left == &sentinel) {
				a = node->right;
				while (a->left != &sentinel)
					a = a->left;
				b = unlinkproperty(node->right, a->name, &temp);
				temp->level = node->level;
				temp->left = node->left;
				temp->right = b;
				node = temp;
			}
			else {
				a = node->left;
				while (a->right != &sentinel)
					a = a->right;
				b = unlinkproperty(node->left, a->name, &temp);
				temp->level = node->level;
				temp->left = b;
				temp->right = node->right;
				node = temp;
			}
		}

		if (node->left->level < node->level - 1 || node->right->level < node->level - 1)
		{
			if (node->right->level > --node->level)
				node->right->level = node->level;
			node = skew(node);
			node->right = skew(node->right);
			node->right->right = skew(node->right->right);
			node = split(node);
			node->right = split(node->right);
		}
	}
	return node;
}

static js_Property *deleteproperty(js_State *J, js_Object *obj, js_Property *tree, const char *name)
{
	js_Property *garbage = &sentinel;
	tree = unlinkproperty(tree, name, &garbage);
	if (garbage != &sentinel)
		freeproperty(J, obj, garbage);
	return tree;
}

js_Object *jsV_newobject(js_State *J, enum js_Class type, js_Object *prototype)
{
	js_Object *obj = js_malloc(J, sizeof *obj);
	memset(obj, 0, sizeof *obj);
	obj->gcmark = 0;
	obj->gcnext = J->gcobj;
	J->gcobj = obj;
	++J->gccounter;

	obj->type = type;
	obj->properties = &sentinel;
	obj->prototype = prototype;
	obj->extensible = 1;
	return obj;
}

js_Property *jsV_getownproperty(js_State *J, js_Object *obj, const char *name)
{
	return lookup(obj->properties, name);
}

js_Property *jsV_getpropertyx(js_State *J, js_Object *obj, const char *name, int *own)
{
	*own = 1;
	do {
		js_Property *ref = lookup(obj->properties, name);
		if (ref)
			return ref;
		obj = obj->prototype;
		*own = 0;
	} while (obj);
	return NULL;
}

js_Property *jsV_getproperty(js_State *J, js_Object *obj, const char *name)
{
	do {
		js_Property *ref = lookup(obj->properties, name);
		if (ref)
			return ref;
		obj = obj->prototype;
	} while (obj);
	return NULL;
}

static js_Property *jsV_getenumproperty(js_State *J, js_Object *obj, const char *name)
{
	do {
		js_Property *ref = lookup(obj->properties, name);
		if (ref && !(ref->atts & JS_DONTENUM))
			return ref;
		obj = obj->prototype;
	} while (obj);
	return NULL;
}

js_Property *jsV_setproperty(js_State *J, js_Object *obj, const char *name)
{
	js_Property *result;

	if (!obj->extensible) {
		result = lookup(obj->properties, name);
		if (J->strict && !result)
			js_typeerror(J, "object is non-extensible");
		return result;
	}

	obj->properties = insert(J, obj, obj->properties, name, &result);

	return result;
}

void jsV_delproperty(js_State *J, js_Object *obj, const char *name)
{
	obj->properties = deleteproperty(J, obj, obj->properties, name);
}

static js_Iterator *itnewnode(js_State *J, const char *name, js_Iterator *next) {
	int n = strlen(name) + 1;
	js_Iterator *node = js_malloc(J, offsetof(js_Iterator, name) + n);
	node->next = next;
	memcpy(node->name, name, n);
	return node;
}

static js_Iterator *itwalk(js_State *J, js_Iterator *iter, js_Property *prop, js_Object *seen)
{
	if (prop->right != &sentinel)
		iter = itwalk(J, iter, prop->right, seen);
	if (!(prop->atts & JS_DONTENUM)) {
		if (!seen || !jsV_getenumproperty(J, seen, prop->name)) {
			iter = itnewnode(J, prop->name, iter);
		}
	}
	if (prop->left != &sentinel)
		iter = itwalk(J, iter, prop->left, seen);
	return iter;
}

static js_Iterator *itflatten(js_State *J, js_Object *obj)
{
	js_Iterator *iter = NULL;
	if (obj->prototype)
		iter = itflatten(J, obj->prototype);
	if (obj->properties != &sentinel)
		iter = itwalk(J, iter, obj->properties, obj->prototype);
	return iter;
}

js_Object *jsV_newiterator(js_State *J, js_Object *obj, int own)
{
	js_Object *io = jsV_newobject(J, JS_CITERATOR, NULL);
	io->u.iter.target = obj;
	io->u.iter.i = 0;
	io->u.iter.n = 0;
	if (own) {
		io->u.iter.head = NULL;
		if (obj->properties != &sentinel)
			io->u.iter.head = itwalk(J, io->u.iter.head, obj->properties, NULL);
	} else {
		io->u.iter.head = itflatten(J, obj);
	}
	io->u.iter.current = io->u.iter.head;

	if (obj->type == JS_CSTRING)
		io->u.iter.n = obj->u.s.length;

	if (obj->type == JS_CARRAY && obj->u.a.simple)
		io->u.iter.n = obj->u.a.flat_length;

	return io;
}

const char *jsV_nextiterator(js_State *J, js_Object *io)
{
	if (io->type != JS_CITERATOR)
		js_typeerror(J, "not an iterator");
	if (io->u.iter.i < io->u.iter.n) {
		js_itoa(J->scratch, io->u.iter.i);
		io->u.iter.i++;
		return J->scratch;
	}
	while (io->u.iter.current) {
		const char *name = io->u.iter.current->name;
		io->u.iter.current = io->u.iter.current->next;
		if (jsV_getproperty(J, io->u.iter.target, name))
			return name;
	}
	return NULL;
}

void jsV_resizearray(js_State *J, js_Object *obj, int newlen)
{
	char buf[32];
	const char *s;
	int k;
	assert(!obj->u.a.simple);
	if (newlen < obj->u.a.length) {
		if (obj->u.a.length > obj->count * 2) {
			js_Object *it = jsV_newiterator(J, obj, 1);
			while ((s = jsV_nextiterator(J, it))) {
				k = jsV_numbertointeger(jsV_stringtonumber(J, s));
				if (k >= newlen && !strcmp(s, jsV_numbertostring(J, buf, k)))
					jsV_delproperty(J, obj, s);
			}
		} else {
			for (k = newlen; k < obj->u.a.length; ++k) {
				jsV_delproperty(J, obj, js_itoa(buf, k));
			}
		}
	}
	obj->u.a.length = newlen;
}

static char *escaperegexp(js_State *J, const char *pattern) {
	char *copy, *p;
	const char *s;
	int n = 0;
	for (s = pattern; *s; ++s) {
		if (*s == '/')
			++n;
		++n;
	}
	copy = p = js_malloc(J, n+1);
	for (s = pattern; *s; ++s) {
		if (*s == '/')
			*p++ = '\\';
		*p++ = *s;
	}
	*p = 0;
	return copy;
}

static void js_newregexpx(js_State *J, const char *pattern, int flags, int is_clone)
{
	const char *error;
	js_Object *obj;
	Reprog *prog;
	int opts;

	obj = jsV_newobject(J, JS_CREGEXP, J->RegExp_prototype);

	opts = 0;
	if (flags & JS_REGEXP_I) opts |= REG_ICASE;
	if (flags & JS_REGEXP_M) opts |= REG_NEWLINE;

	prog = js_regcompx(J->alloc, J->actx, pattern, opts, &error);
	if (!prog)
		js_syntaxerror(J, "regular expression: %s", error);

	obj->u.r.prog = prog;
	obj->u.r.source = is_clone ? js_strdup(J, pattern) : escaperegexp(J, pattern);
	obj->u.r.flags = flags;
	obj->u.r.last = 0;
	js_pushobject(J, obj);
}

void js_newregexp(js_State *J, const char *pattern, int flags)
{
	js_newregexpx(J, pattern, flags, 0);
}

void js_RegExp_prototype_exec(js_State *J, js_Regexp *re, const char *text)
{
	const char *haystack;
	int result;
	int i;
	int opts;
	Resub m;

	haystack = text;
	opts = 0;
	if (re->flags & JS_REGEXP_G) {
		if (re->last > strlen(haystack)) {
			re->last = 0;
			js_pushnull(J);
			return;
		}
		if (re->last > 0) {
			haystack = text + re->last;
			if (!(re->flags & JS_REGEXP_M) || haystack[-1] != '\n')
				opts |= REG_NOTBOL;
		}
	}

	result = js_regexec(re->prog, haystack, &m, opts);
	if (result < 0)
		js_error(J, "regexec failed");
	if (result == 0) {
		js_newarray(J);
		js_pushstring(J, text);
		js_setproperty(J, -2, "input");
		js_pushnumber(J, js_utfptrtoidx(text, m.sub[0].sp));
		js_setproperty(J, -2, "index");
		for (i = 0; i < m.nsub; ++i) {
			js_pushlstring(J, m.sub[i].sp, m.sub[i].ep - m.sub[i].sp);
			js_setindex(J, -2, i);
		}
		if (re->flags & JS_REGEXP_G)
			re->last = m.sub[0].ep - text;
		return;
	}

	if (re->flags & JS_REGEXP_G)
		re->last = 0;

	js_pushnull(J);
}

static void Rp_test(js_State *J)
{
	js_Regexp *re;
	const char *text;
	int result;
	int opts;
	Resub m;

	re = js_toregexp(J, 0);
	text = js_tostring(J, 1);

	opts = 0;
	if (re->flags & JS_REGEXP_G) {
		if (re->last > strlen(text)) {
			re->last = 0;
			js_pushboolean(J, 0);
			return;
		}
		if (re->last > 0) {
			text += re->last;
			if (!(re->flags & JS_REGEXP_M) || text[-1] != '\n')
				opts |= REG_NOTBOL;
		}
	}

	result = js_regexec(re->prog, text, &m, opts);
	if (result < 0)
		js_error(J, "regexec failed");
	if (result == 0) {
		if (re->flags & JS_REGEXP_G)
			re->last = re->last + (m.sub[0].ep - text);
		js_pushboolean(J, 1);
		return;
	}

	if (re->flags & JS_REGEXP_G)
		re->last = 0;

	js_pushboolean(J, 0);
}

static void jsB_new_RegExp(js_State *J)
{
	js_Regexp *old;
	const char *pattern;
	int flags;
	int is_clone = 0;

	if (js_isregexp(J, 1)) {
		if (js_isdefined(J, 2))
			js_typeerror(J, "cannot supply flags when creating one RegExp from another");
		old = js_toregexp(J, 1);
		pattern = old->source;
		flags = old->flags;
		is_clone = 1;
	} else if (js_isundefined(J, 1)) {
		pattern = "(?:)";
		flags = 0;
	} else {
		pattern = js_tostring(J, 1);
		flags = 0;
	}

	if (strlen(pattern) == 0)
		pattern = "(?:)";

	if (js_isdefined(J, 2)) {
		const char *s = js_tostring(J, 2);
		int g = 0, i = 0, m = 0;
		while (*s) {
			if (*s == 'g') ++g;
			else if (*s == 'i') ++i;
			else if (*s == 'm') ++m;
			else js_syntaxerror(J, "invalid regular expression flag: '%c'", *s);
			++s;
		}
		if (g > 1) js_syntaxerror(J, "invalid regular expression flag: 'g'");
		if (i > 1) js_syntaxerror(J, "invalid regular expression flag: 'i'");
		if (m > 1) js_syntaxerror(J, "invalid regular expression flag: 'm'");
		if (g) flags |= JS_REGEXP_G;
		if (i) flags |= JS_REGEXP_I;
		if (m) flags |= JS_REGEXP_M;
	}

	js_newregexpx(J, pattern, flags, is_clone);
}

static void jsB_RegExp(js_State *J)
{
	if (js_isregexp(J, 1))
		return;
	jsB_new_RegExp(J);
}

static void Rp_toString(js_State *J)
{
	js_Regexp *re;
	char * volatile out = NULL;

	re = js_toregexp(J, 0);

	if (js_try(J)) {
		js_free(J, out);
		js_throw(J);
	}

	out = js_malloc(J, strlen(re->source) + 6);
	strcpy(out, "/");
	strcat(out, re->source);
	strcat(out, "/");
	if (re->flags & JS_REGEXP_G) strcat(out, "g");
	if (re->flags & JS_REGEXP_I) strcat(out, "i");
	if (re->flags & JS_REGEXP_M) strcat(out, "m");

	js_pop(J, 0);
	js_pushstring(J, out);
	js_endtry(J);
	js_free(J, out);
}

static void Rp_exec(js_State *J)
{
	js_RegExp_prototype_exec(J, js_toregexp(J, 0), js_tostring(J, 1));
}

void jsB_initregexp(js_State *J)
{
	js_pushobject(J, J->RegExp_prototype);
	{
		jsB_propf(J, "RegExp.prototype.toString", Rp_toString, 0);
		jsB_propf(J, "RegExp.prototype.test", Rp_test, 0);
		jsB_propf(J, "RegExp.prototype.exec", Rp_exec, 0);
	}
	js_newcconstructor(J, jsB_RegExp, jsB_new_RegExp, "RegExp", 1);
	js_defglobal(J, "RegExp", JS_DONTENUM);
}

static void reprvalue(js_State *J, js_Buffer **sb);

static void reprnum(js_State *J, js_Buffer **sb, double n)
{
	char buf[40];
	if (n == 0 && signbit(n))
		js_puts(J, sb, "-0");
	else
		js_puts(J, sb, jsV_numbertostring(J, buf, n));
}

static void reprstr(js_State *J, js_Buffer **sb, const char *s)
{
	static const char *HEX = "0123456789ABCDEF";
	int i, n;
	Rune c;
	js_putc(J, sb, '"');
	while (*s) {
		n = chartorune(&c, s);
		switch (c) {
		case '"': js_puts(J, sb, "\\\""); break;
		case '\\': js_puts(J, sb, "\\\\"); break;
		case '\b': js_puts(J, sb, "\\b"); break;
		case '\f': js_puts(J, sb, "\\f"); break;
		case '\n': js_puts(J, sb, "\\n"); break;
		case '\r': js_puts(J, sb, "\\r"); break;
		case '\t': js_puts(J, sb, "\\t"); break;
		default:
			if (c < ' ') {
				js_putc(J, sb, '\\');
				js_putc(J, sb, 'x');
				js_putc(J, sb, HEX[(c>>4)&15]);
				js_putc(J, sb, HEX[c&15]);
			} else if (c < 128) {
				js_putc(J, sb, c);
			} else if (c < 0x10000) {
				js_putc(J, sb, '\\');
				js_putc(J, sb, 'u');
				js_putc(J, sb, HEX[(c>>12)&15]);
				js_putc(J, sb, HEX[(c>>8)&15]);
				js_putc(J, sb, HEX[(c>>4)&15]);
				js_putc(J, sb, HEX[c&15]);
			} else {
				for (i = 0; i < n; ++i)
					js_putc(J, sb, s[i]);
			}
			break;
		}
		s += n;
	}
	js_putc(J, sb, '"');
}

#ifndef isalpha
#define isalpha(c) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
#endif
#ifndef isdigit
#define isdigit(c) (c >= '0' && c <= '9')
#endif

static void reprident(js_State *J, js_Buffer **sb, const char *name)
{
	const char *p = name;
	if (isdigit(*p))
		while (isdigit(*p))
			++p;
	else if (isalpha(*p) || *p == '_')
		while (isdigit(*p) || isalpha(*p) || *p == '_')
			++p;
	if (p > name && *p == 0)
		js_puts(J, sb, name);
	else
		reprstr(J, sb, name);
}

static void reprobject(js_State *J, js_Buffer **sb)
{
	const char *key;
	int i, n;

	n = js_gettop(J) - 1;
	for (i = 0; i < n; ++i) {
		if (js_isobject(J, i)) {
			if (js_toobject(J, i) == js_toobject(J, -1)) {
				js_puts(J, sb, "{}");
				return;
			}
		}
	}

	n = 0;
	js_putc(J, sb, '{');
	js_pushiterator(J, -1, 1);
	while ((key = js_nextiterator(J, -1))) {
		if (n++ > 0)
			js_puts(J, sb, ", ");
		reprident(J, sb, key);
		js_puts(J, sb, ": ");
		js_getproperty(J, -2, key);
		reprvalue(J, sb);
		js_pop(J, 1);
	}
	js_pop(J, 1);
	js_putc(J, sb, '}');
}

static void reprarray(js_State *J, js_Buffer **sb)
{
	int n, i;

	n = js_gettop(J) - 1;
	for (i = 0; i < n; ++i) {
		if (js_isobject(J, i)) {
			if (js_toobject(J, i) == js_toobject(J, -1)) {
				js_puts(J, sb, "[]");
				return;
			}
		}
	}

	js_putc(J, sb, '[');
	n = js_getlength(J, -1);
	for (i = 0; i < n; ++i) {
		if (i > 0)
			js_puts(J, sb, ", ");
		if (js_hasindex(J, -1, i)) {
			reprvalue(J, sb);
			js_pop(J, 1);
		}
	}
	js_putc(J, sb, ']');
}

static void reprfun(js_State *J, js_Buffer **sb, js_Function *fun)
{
	int i;
	js_puts(J, sb, "function ");
	js_puts(J, sb, fun->name);
	js_putc(J, sb, '(');
	for (i = 0; i < fun->numparams; ++i) {
		if (i > 0)
			js_puts(J, sb, ", ");
		js_puts(J, sb, fun->vartab[i]);
	}
	js_puts(J, sb, ") { [byte code] }");
}

static void reprvalue(js_State *J, js_Buffer **sb)
{
	if (js_isundefined(J, -1))
		js_puts(J, sb, "undefined");
	else if (js_isnull(J, -1))
		js_puts(J, sb, "null");
	else if (js_isboolean(J, -1))
		js_puts(J, sb, js_toboolean(J, -1) ? "true" : "false");
	else if (js_isnumber(J, -1))
		reprnum(J, sb, js_tonumber(J, -1));
	else if (js_isstring(J, -1))
		reprstr(J, sb, js_tostring(J, -1));
	else if (js_isobject(J, -1)) {
		js_Object *obj = js_toobject(J, -1);
		switch (obj->type) {
		default:
			reprobject(J, sb);
			break;
		case JS_CARRAY:
			reprarray(J, sb);
			break;
		case JS_CFUNCTION:
		case JS_CSCRIPT:
			reprfun(J, sb, obj->u.f.function);
			break;
		case JS_CCFUNCTION:
			js_puts(J, sb, "function ");
			js_puts(J, sb, obj->u.c.name);
			js_puts(J, sb, "() { [native code] }");
			break;
		case JS_CBOOLEAN:
			js_puts(J, sb, "(new Boolean(");
			js_puts(J, sb, obj->u.boolean ? "true" : "false");
			js_puts(J, sb, "))");
			break;
		case JS_CNUMBER:
			js_puts(J, sb, "(new Number(");
			reprnum(J, sb, obj->u.number);
			js_puts(J, sb, "))");
			break;
		case JS_CSTRING:
			js_puts(J, sb, "(new String(");
			reprstr(J, sb, obj->u.s.string);
			js_puts(J, sb, "))");
			break;
		case JS_CREGEXP:
			js_putc(J, sb, '/');
			js_puts(J, sb, obj->u.r.source);
			js_putc(J, sb, '/');
			if (obj->u.r.flags & JS_REGEXP_G) js_putc(J, sb, 'g');
			if (obj->u.r.flags & JS_REGEXP_I) js_putc(J, sb, 'i');
			if (obj->u.r.flags & JS_REGEXP_M) js_putc(J, sb, 'm');
			break;
		case JS_CDATE:
			{
				char buf[40];
				js_puts(J, sb, "(new Date(");
				js_puts(J, sb, jsV_numbertostring(J, buf, obj->u.number));
				js_puts(J, sb, "))");
			}
			break;
		case JS_CERROR:
			js_puts(J, sb, "(new ");
			js_getproperty(J, -1, "name");
			js_puts(J, sb, js_tostring(J, -1));
			js_pop(J, 1);
			js_putc(J, sb, '(');
			if (js_hasproperty(J, -1, "message")) {
				reprvalue(J, sb);
				js_pop(J, 1);
			}
			js_puts(J, sb, "))");
			break;
		case JS_CMATH:
			js_puts(J, sb, "Math");
			break;
		case JS_CJSON:
			js_puts(J, sb, "JSON");
			break;
		case JS_CITERATOR:
			js_puts(J, sb, "[iterator ");
			break;
		case JS_CUSERDATA:
			js_puts(J, sb, "[userdata ");
			js_puts(J, sb, obj->u.user.tag);
			js_putc(J, sb, ']');
			break;
		}
	}
}

void js_repr(js_State *J, int idx)
{
	js_Buffer *sb = NULL;
	int savebot;

	if (js_try(J)) {
		js_free(J, sb);
		js_throw(J);
	}

	js_copy(J, idx);

	savebot = J->bot;
	J->bot = J->top - 1;
	reprvalue(J, &sb);
	J->bot = savebot;

	js_pop(J, 1);

	js_putc(J, &sb, 0);
	js_pushstring(J, sb ? sb->s : "undefined");

	js_endtry(J);
	js_free(J, sb);
}

const char *js_torepr(js_State *J, int idx)
{
	js_repr(J, idx);
	js_replace(J, idx < 0 ? idx-1 : idx);
	return js_tostring(J, idx);
}

const char *js_tryrepr(js_State *J, int idx, const char *error)
{
	const char *s;
	if (js_try(J)) {
		js_pop(J, 1);
		return error;
	}
	s = js_torepr(J, idx);
	js_endtry(J);
	return s;
}

#include <assert.h>

static void jsR_run(js_State *J, js_Function *F);

#define STACK (J->stack)
#define TOP (J->top)
#define BOT (J->bot)

static void js_trystackoverflow(js_State *J)
{
	STACK[TOP].t.type = JS_TLITSTR;
	STACK[TOP].u.litstr = "exception stack overflow";
	++TOP;
	js_throw(J);
}

static void js_stackoverflow(js_State *J)
{
	STACK[TOP].t.type = JS_TLITSTR;
	STACK[TOP].u.litstr = "stack overflow";
	++TOP;
	js_throw(J);
}

static void js_outofmemory(js_State *J)
{
	STACK[TOP].t.type = JS_TLITSTR;
	STACK[TOP].u.litstr = "out of memory";
	++TOP;
	js_throw(J);
}

static void js_runlimit(js_State *J)
{
	STACK[TOP].t.type = JS_TLITSTR;
	STACK[TOP].u.litstr = "script ran too long";
	++TOP;
	js_throw(J);
}

void js_setlimit(js_State *J, int runlimit, int memlimit)
{
	J->runlimit = runlimit;
	J->memlimit = memlimit;
}

void *js_malloc(js_State *J, int size)
{
	void *ptr;
	if (J->memlimit > 0) {
		if (size >= J->memlimit)
			js_outofmemory(J);
		J->memlimit -= size;
	}
	ptr = J->alloc(J->actx, NULL, size);
	if (!ptr)
		js_outofmemory(J);
	return ptr;
}

void *js_realloc(js_State *J, void *ptr, int size)
{
	if (J->memlimit > 0) {

		if (size >= J->memlimit)
			js_outofmemory(J);
		J->memlimit -= size;
	}
	ptr = J->alloc(J->actx, ptr, size);
	if (!ptr)
		js_outofmemory(J);
	return ptr;
}

char *js_strdup(js_State *J, const char *s)
{
	int n = strlen(s) + 1;
	char *p = js_malloc(J, n);
	memcpy(p, s, n);
	return p;
}

void js_free(js_State *J, void *ptr)
{

	J->alloc(J->actx, ptr, 0);
}

js_String *jsV_newmemstring(js_State *J, const char *s, int n)
{
	js_String *v = js_malloc(J, soffsetof(js_String, p) + n + 1);
	memcpy(v->p, s, n);
	v->p[n] = 0;
	v->gcmark = 0;
	v->gcnext = J->gcstr;
	J->gcstr = v;
	++J->gccounter;
	return v;
}

#define CHECKSTACK(n) if (TOP + n >= JS_STACKSIZE) js_stackoverflow(J)

void js_pushvalue(js_State *J, js_Value v)
{
	CHECKSTACK(1);
	STACK[TOP] = v;
	++TOP;
}

void js_pushundefined(js_State *J)
{
	CHECKSTACK(1);
	STACK[TOP].t.type = JS_TUNDEFINED;
	++TOP;
}

void js_pushnull(js_State *J)
{
	CHECKSTACK(1);
	STACK[TOP].t.type = JS_TNULL;
	++TOP;
}

void js_pushboolean(js_State *J, int v)
{
	CHECKSTACK(1);
	STACK[TOP].t.type = JS_TBOOLEAN;
	STACK[TOP].u.boolean = !!v;
	++TOP;
}

void js_pushnumber(js_State *J, double v)
{
	CHECKSTACK(1);
	STACK[TOP].t.type = JS_TNUMBER;
	STACK[TOP].u.number = v;
	++TOP;
}

void js_pushstring(js_State *J, const char *v)
{
	size_t n = strlen(v);
	if (n > JS_STRLIMIT)
		js_rangeerror(J, "invalid string length");
	CHECKSTACK(1);
	if (n <= soffsetof(js_Value, t.type)) {
		char *s = STACK[TOP].u.shrstr;
		while (n--) *s++ = *v++;
		*s = 0;
		STACK[TOP].t.type = JS_TSHRSTR;
	} else {
		STACK[TOP].t.type = JS_TMEMSTR;
		STACK[TOP].u.memstr = jsV_newmemstring(J, v, n);
	}
	++TOP;
}

void js_pushlstring(js_State *J, const char *v, int n)
{
	if (n > JS_STRLIMIT)
		js_rangeerror(J, "invalid string length");
	CHECKSTACK(1);
	if (n <= soffsetof(js_Value, t.type)) {
		char *s = STACK[TOP].u.shrstr;
		while (n--) *s++ = *v++;
		*s = 0;
		STACK[TOP].t.type = JS_TSHRSTR;
	} else {
		STACK[TOP].t.type = JS_TMEMSTR;
		STACK[TOP].u.memstr = jsV_newmemstring(J, v, n);
	}
	++TOP;
}

void js_pushliteral(js_State *J, const char *v)
{
	CHECKSTACK(1);
	STACK[TOP].t.type = JS_TLITSTR;
	STACK[TOP].u.litstr = v;
	++TOP;
}

void js_pushobject(js_State *J, js_Object *v)
{
	CHECKSTACK(1);
	STACK[TOP].t.type = JS_TOBJECT;
	STACK[TOP].u.object = v;
	++TOP;
}

void js_pushglobal(js_State *J)
{
	js_pushobject(J, J->G);
}

void js_currentfunction(js_State *J)
{
	CHECKSTACK(1);
	if (BOT > 0)
		STACK[TOP] = STACK[BOT-1];
	else
		STACK[TOP].t.type = JS_TUNDEFINED;
	++TOP;
}

void *js_currentfunctiondata(js_State *J)
{
	if (BOT > 0)
		return STACK[BOT-1].u.object->u.c.data;
	return NULL;
}

static js_Value *stackidx(js_State *J, int idx)
{
	static js_Value undefined = { { {0}, JS_TUNDEFINED } };
	idx = idx < 0 ? TOP + idx : BOT + idx;
	if (idx < 0 || idx >= TOP)
		return &undefined;
	return STACK + idx;
}

js_Value *js_tovalue(js_State *J, int idx)
{
	return stackidx(J, idx);
}

int js_isdefined(js_State *J, int idx) { return stackidx(J, idx)->t.type != JS_TUNDEFINED; }
int js_isundefined(js_State *J, int idx) { return stackidx(J, idx)->t.type == JS_TUNDEFINED; }
int js_isnull(js_State *J, int idx) { return stackidx(J, idx)->t.type == JS_TNULL; }
int js_isboolean(js_State *J, int idx) { return stackidx(J, idx)->t.type == JS_TBOOLEAN; }
int js_isnumber(js_State *J, int idx) { return stackidx(J, idx)->t.type == JS_TNUMBER; }
int js_isstring(js_State *J, int idx) { enum js_Type t = stackidx(J, idx)->t.type; return t == JS_TSHRSTR || t == JS_TLITSTR || t == JS_TMEMSTR; }
int js_isprimitive(js_State *J, int idx) { return stackidx(J, idx)->t.type != JS_TOBJECT; }
int js_isobject(js_State *J, int idx) { return stackidx(J, idx)->t.type == JS_TOBJECT; }
int js_iscoercible(js_State *J, int idx) { js_Value *v = stackidx(J, idx); return v->t.type != JS_TUNDEFINED && v->t.type != JS_TNULL; }

int js_iscallable(js_State *J, int idx)
{
	js_Value *v = stackidx(J, idx);
	if (v->t.type == JS_TOBJECT)
		return v->u.object->type == JS_CFUNCTION ||
			v->u.object->type == JS_CSCRIPT ||
			v->u.object->type == JS_CCFUNCTION;
	return 0;
}

int js_isarray(js_State *J, int idx)
{
	js_Value *v = stackidx(J, idx);
	return v->t.type == JS_TOBJECT && v->u.object->type == JS_CARRAY;
}

int js_isregexp(js_State *J, int idx)
{
	js_Value *v = stackidx(J, idx);
	return v->t.type == JS_TOBJECT && v->u.object->type == JS_CREGEXP;
}

int js_isuserdata(js_State *J, int idx, const char *tag)
{
	js_Value *v = stackidx(J, idx);
	if (v->t.type == JS_TOBJECT && v->u.object->type == JS_CUSERDATA)
		return !strcmp(tag, v->u.object->u.user.tag);
	return 0;
}

int js_iserror(js_State *J, int idx)
{
	js_Value *v = stackidx(J, idx);
	return v->t.type == JS_TOBJECT && v->u.object->type == JS_CERROR;
}

const char *js_typeof(js_State *J, int idx)
{
	js_Value *v = stackidx(J, idx);
	switch (v->t.type) {
	default:
	case JS_TSHRSTR: return "string";
	case JS_TUNDEFINED: return "undefined";
	case JS_TNULL: return "object";
	case JS_TBOOLEAN: return "boolean";
	case JS_TNUMBER: return "number";
	case JS_TLITSTR: return "string";
	case JS_TMEMSTR: return "string";
	case JS_TOBJECT:
		if (v->u.object->type == JS_CFUNCTION || v->u.object->type == JS_CCFUNCTION)
			return "function";
		return "object";
	}
}

int js_type(js_State *J, int idx)
{
	js_Value *v = stackidx(J, idx);
	switch (v->t.type) {
	default:
	case JS_TSHRSTR: return JS_ISSTRING;
	case JS_TUNDEFINED: return JS_ISUNDEFINED;
	case JS_TNULL: return JS_ISNULL;
	case JS_TBOOLEAN: return JS_ISBOOLEAN;
	case JS_TNUMBER: return JS_ISNUMBER;
	case JS_TLITSTR: return JS_ISSTRING;
	case JS_TMEMSTR: return JS_ISSTRING;
	case JS_TOBJECT:
		if (v->u.object->type == JS_CFUNCTION || v->u.object->type == JS_CCFUNCTION)
			return JS_ISFUNCTION;
		return JS_ISOBJECT;
	}
}

int js_toboolean(js_State *J, int idx)
{
	return jsV_toboolean(J, stackidx(J, idx));
}

double js_tonumber(js_State *J, int idx)
{
	return jsV_tonumber(J, stackidx(J, idx));
}

int js_tointeger(js_State *J, int idx)
{
	return jsV_numbertointeger(jsV_tonumber(J, stackidx(J, idx)));
}

int js_toint32(js_State *J, int idx)
{
	return jsV_numbertoint32(jsV_tonumber(J, stackidx(J, idx)));
}

unsigned int js_touint32(js_State *J, int idx)
{
	return jsV_numbertouint32(jsV_tonumber(J, stackidx(J, idx)));
}

short js_toint16(js_State *J, int idx)
{
	return jsV_numbertoint16(jsV_tonumber(J, stackidx(J, idx)));
}

unsigned short js_touint16(js_State *J, int idx)
{
	return jsV_numbertouint16(jsV_tonumber(J, stackidx(J, idx)));
}

const char *js_tostring(js_State *J, int idx)
{
	return jsV_tostring(J, stackidx(J, idx));
}

js_Object *js_toobject(js_State *J, int idx)
{
	return jsV_toobject(J, stackidx(J, idx));
}

void js_toprimitive(js_State *J, int idx, int hint)
{
	jsV_toprimitive(J, stackidx(J, idx), hint);
}

js_Regexp *js_toregexp(js_State *J, int idx)
{
	js_Value *v = stackidx(J, idx);
	if (v->t.type == JS_TOBJECT && v->u.object->type == JS_CREGEXP)
		return &v->u.object->u.r;
	js_typeerror(J, "not a regexp");
}

void *js_touserdata(js_State *J, int idx, const char *tag)
{
	js_Value *v = stackidx(J, idx);
	if (v->t.type == JS_TOBJECT && v->u.object->type == JS_CUSERDATA)
		if (!strcmp(tag, v->u.object->u.user.tag))
			return v->u.object->u.user.data;
	js_typeerror(J, "not a %s", tag);
}

static js_Object *jsR_tofunction(js_State *J, int idx)
{
	js_Value *v = stackidx(J, idx);
	if (v->t.type == JS_TUNDEFINED || v->t.type == JS_TNULL)
		return NULL;
	if (v->t.type == JS_TOBJECT)
		if (v->u.object->type == JS_CFUNCTION || v->u.object->type == JS_CCFUNCTION)
			return v->u.object;
	js_typeerror(J, "not a function");
}

int js_gettop(js_State *J)
{
	return TOP - BOT;
}

void js_pop(js_State *J, int n)
{
	TOP -= n;
	if (TOP < BOT) {
		TOP = BOT;
		js_error(J, "stack underflow!");
	}
}

void js_remove(js_State *J, int idx)
{
	idx = idx < 0 ? TOP + idx : BOT + idx;
	if (idx < BOT || idx >= TOP)
		js_error(J, "stack error!");
	for (;idx < TOP - 1; ++idx)
		STACK[idx] = STACK[idx+1];
	--TOP;
}

void js_insert(js_State *J, int idx)
{
	js_error(J, "not implemented yet");
}

void js_replace(js_State* J, int idx)
{
	idx = idx < 0 ? TOP + idx : BOT + idx;
	if (idx < BOT || idx >= TOP)
		js_error(J, "stack error!");
	STACK[idx] = STACK[--TOP];
}

void js_copy(js_State *J, int idx)
{
	CHECKSTACK(1);
	STACK[TOP] = *stackidx(J, idx);
	++TOP;
}

void js_dup(js_State *J)
{
	CHECKSTACK(1);
	STACK[TOP] = STACK[TOP-1];
	++TOP;
}

void js_dup2(js_State *J)
{
	CHECKSTACK(2);
	STACK[TOP] = STACK[TOP-2];
	STACK[TOP+1] = STACK[TOP-1];
	TOP += 2;
}

void js_rot2(js_State *J)
{

	js_Value tmp = STACK[TOP-1];
	STACK[TOP-1] = STACK[TOP-2];
	STACK[TOP-2] = tmp;
}

void js_rot3(js_State *J)
{

	js_Value tmp = STACK[TOP-1];
	STACK[TOP-1] = STACK[TOP-2];
	STACK[TOP-2] = STACK[TOP-3];
	STACK[TOP-3] = tmp;
}

void js_rot4(js_State *J)
{

	js_Value tmp = STACK[TOP-1];
	STACK[TOP-1] = STACK[TOP-2];
	STACK[TOP-2] = STACK[TOP-3];
	STACK[TOP-3] = STACK[TOP-4];
	STACK[TOP-4] = tmp;
}

void js_rot2pop1(js_State *J)
{

	STACK[TOP-2] = STACK[TOP-1];
	--TOP;
}

void js_rot3pop2(js_State *J)
{

	STACK[TOP-3] = STACK[TOP-1];
	TOP -= 2;
}

void js_rot(js_State *J, int n)
{
	int i;
	js_Value tmp = STACK[TOP-1];
	for (i = 1; i < n; ++i)
		STACK[TOP-i] = STACK[TOP-i-1];
	STACK[TOP-i] = tmp;
}

int js_isarrayindex(js_State *J, const char *p, int *idx)
{
	int n = 0;

	if (p[0] == 0)
		return 0;

	if (p[0] == '0')
		return (p[1] == 0) ? *idx = 0, 1 : 0;

	while (*p) {
		int c = *p++;
		if (c >= '0' && c <= '9') {
			if (n >= INT_MAX / 10)
				return 0;
			n = n * 10 + (c - '0');
		} else {
			return 0;
		}
	}
	return *idx = n, 1;
}

static void js_pushrune(js_State *J, Rune rune)
{
	char buf[UTFmax + 1];
	if (rune >= 0) {
		buf[runetochar(buf, &rune)] = 0;
		js_pushstring(J, buf);
	} else {
		js_pushundefined(J);
	}
}

void jsR_unflattenarray(js_State *J, js_Object *obj) {
	if (obj->type == JS_CARRAY && obj->u.a.simple) {
		js_Property *ref;
		int i;
		char name[32];
		if (js_try(J)) {
			obj->properties = NULL;
			js_throw(J);
		}
		for (i = 0; i < obj->u.a.flat_length; ++i) {
			js_itoa(name, i);
			ref = jsV_setproperty(J, obj, name);
			ref->value = obj->u.a.array[i];
		}
		js_free(J, obj->u.a.array);
		obj->u.a.simple = 0;
		obj->u.a.flat_length = 0;
		obj->u.a.flat_capacity = 0;
		obj->u.a.array = NULL;
		js_endtry(J);
	}
}

static int jsR_hasproperty(js_State *J, js_Object *obj, const char *name)
{
	js_Property *ref;
	int k;

	if (obj->type == JS_CARRAY) {
		if (!strcmp(name, "length")) {
			js_pushnumber(J, obj->u.a.length);
			return 1;
		}
		if (obj->u.a.simple) {
			if (js_isarrayindex(J, name, &k)) {
				if (k >= 0 && k < obj->u.a.flat_length) {
					js_pushvalue(J, obj->u.a.array[k]);
					return 1;
				}
				return 0;
			}
		}
	}

	else if (obj->type == JS_CSTRING) {
		if (!strcmp(name, "length")) {
			js_pushnumber(J, obj->u.s.length);
			return 1;
		}
		if (js_isarrayindex(J, name, &k)) {
			if (k >= 0 && k < obj->u.s.length) {
				js_pushrune(J, js_runeat(J, obj->u.s.string, k));
				return 1;
			}
		}
	}

	else if (obj->type == JS_CREGEXP) {
		if (!strcmp(name, "source")) {
			js_pushstring(J, obj->u.r.source);
			return 1;
		}
		if (!strcmp(name, "global")) {
			js_pushboolean(J, obj->u.r.flags & JS_REGEXP_G);
			return 1;
		}
		if (!strcmp(name, "ignoreCase")) {
			js_pushboolean(J, obj->u.r.flags & JS_REGEXP_I);
			return 1;
		}
		if (!strcmp(name, "multiline")) {
			js_pushboolean(J, obj->u.r.flags & JS_REGEXP_M);
			return 1;
		}
		if (!strcmp(name, "lastIndex")) {
			js_pushnumber(J, obj->u.r.last);
			return 1;
		}
	}

	else if (obj->type == JS_CUSERDATA) {
		if (obj->u.user.has && obj->u.user.has(J, obj->u.user.data, name))
			return 1;
	}

	ref = jsV_getproperty(J, obj, name);
	if (ref) {
		if (ref->getter) {
			js_pushobject(J, ref->getter);
			js_pushobject(J, obj);
			js_call(J, 0);
		} else {
			js_pushvalue(J, ref->value);
		}
		return 1;
	}

	return 0;
}

static void jsR_getproperty(js_State *J, js_Object *obj, const char *name)
{
	if (!jsR_hasproperty(J, obj, name))
		js_pushundefined(J);
}

static int jsR_hasindex(js_State *J, js_Object *obj, int k)
{
	char buf[32];
	if (obj->type == JS_CARRAY && obj->u.a.simple) {
		if (k >= 0 && k < obj->u.a.flat_length) {
			js_pushvalue(J, obj->u.a.array[k]);
			return 1;
		}
		return 0;
	}
	return jsR_hasproperty(J, obj, js_itoa(buf, k));
}

static void jsR_getindex(js_State *J, js_Object *obj, int k)
{
	if (!jsR_hasindex(J, obj, k))
		js_pushundefined(J);
}

static void jsR_setarrayindex(js_State *J, js_Object *obj, int k, js_Value *value)
{
	int newlen = k + 1;
	assert(obj->u.a.simple);
	assert(k >= 0);
	if (newlen > JS_ARRAYLIMIT)
		js_rangeerror(J, "array too large");
	if (newlen > obj->u.a.flat_length) {
		assert(newlen == obj->u.a.flat_length + 1);
		if (newlen > obj->u.a.flat_capacity) {
			int newcap = obj->u.a.flat_capacity;
			if (newcap == 0)
				newcap = 8;
			while (newcap < newlen)
				newcap <<= 1;
			obj->u.a.array = js_realloc(J, obj->u.a.array, newcap * sizeof(js_Value));
			obj->u.a.flat_capacity = newcap;
		}
		obj->u.a.flat_length = newlen;
	}
	if (newlen > obj->u.a.length)
		obj->u.a.length = newlen;
	obj->u.a.array[k] = *value;
}

static void jsR_setproperty(js_State *J, js_Object *obj, const char *name, int transient)
{
	js_Value *value = stackidx(J, -1);
	js_Property *ref;
	int k;
	int own;

	if (obj->type == JS_CARRAY) {
		if (!strcmp(name, "length")) {
			double rawlen = jsV_tonumber(J, value);
			int newlen = jsV_numbertointeger(rawlen);
			if (newlen != rawlen || newlen < 0)
				js_rangeerror(J, "invalid array length");
			if (newlen > JS_ARRAYLIMIT)
				js_rangeerror(J, "array too large");
			if (obj->u.a.simple) {
				obj->u.a.length = newlen;
				if (newlen <= obj->u.a.flat_length)
					obj->u.a.flat_length = newlen;
			} else  {
				jsV_resizearray(J, obj, newlen);
			}
			return;
		}

		if (js_isarrayindex(J, name, &k)) {
			if (obj->u.a.simple) {
				if (k >= 0 && k <= obj->u.a.flat_length) {
					jsR_setarrayindex(J, obj, k, value);
				} else {
					jsR_unflattenarray(J, obj);
					if (obj->u.a.length < k + 1)
						obj->u.a.length = k + 1;
				}
			} else {
				if (obj->u.a.length < k + 1)
					obj->u.a.length = k + 1;
			}
		}
	}

	else if (obj->type == JS_CSTRING) {
		if (!strcmp(name, "length"))
			goto readonly;
		if (js_isarrayindex(J, name, &k))
			if (k >= 0 && k < obj->u.s.length)
				goto readonly;
	}

	else if (obj->type == JS_CREGEXP) {
		if (!strcmp(name, "source")) goto readonly;
		if (!strcmp(name, "global")) goto readonly;
		if (!strcmp(name, "ignoreCase")) goto readonly;
		if (!strcmp(name, "multiline")) goto readonly;
		if (!strcmp(name, "lastIndex")) {
			obj->u.r.last = jsV_tointeger(J, value);
			return;
		}
	}

	else if (obj->type == JS_CUSERDATA) {
		if (obj->u.user.put && obj->u.user.put(J, obj->u.user.data, name))
			return;
	}

	ref = jsV_getpropertyx(J, obj, name, &own);
	if (ref) {
		if (ref->setter) {
			js_pushobject(J, ref->setter);
			js_pushobject(J, obj);
			js_pushvalue(J, *value);
			js_call(J, 1);
			js_pop(J, 1);
			return;
		} else {
			if (J->strict)
				if (ref->getter)
					js_typeerror(J, "setting property '%s' that only has a getter", name);
			if (ref->atts & JS_READONLY)
				goto readonly;
		}
	}

	if (!ref || !own) {
		if (transient) {
			if (J->strict)
				js_typeerror(J, "cannot create property '%s' on transient object", name);
			return;
		}
		ref = jsV_setproperty(J, obj, name);
	}

	if (ref) {
		if (!(ref->atts & JS_READONLY))
			ref->value = *value;
		else
			goto readonly;
	}

	return;

readonly:
	if (J->strict)
		js_typeerror(J, "'%s' is read-only", name);
}

static void jsR_setindex(js_State *J, js_Object *obj, int k, int transient)
{
	char buf[32];
	if (obj->type == JS_CARRAY && obj->u.a.simple && k >= 0 && k <= obj->u.a.flat_length) {
		jsR_setarrayindex(J, obj, k, stackidx(J, -1));
	} else {
		jsR_setproperty(J, obj, js_itoa(buf, k), transient);
	}
}

static void jsR_defproperty(js_State *J, js_Object *obj, const char *name,
	int atts, js_Value *value, js_Object *getter, js_Object *setter,
	int throw)
{
	js_Property *ref;
	int k;

	if (obj->type == JS_CARRAY) {
		if (!strcmp(name, "length"))
			goto readonly;
		if (obj->u.a.simple)
			jsR_unflattenarray(J, obj);
	}

	else if (obj->type == JS_CSTRING) {
		if (!strcmp(name, "length"))
			goto readonly;
		if (js_isarrayindex(J, name, &k))
			if (k >= 0 && k < obj->u.s.length)
				goto readonly;
	}

	else if (obj->type == JS_CREGEXP) {
		if (!strcmp(name, "source")) goto readonly;
		if (!strcmp(name, "global")) goto readonly;
		if (!strcmp(name, "ignoreCase")) goto readonly;
		if (!strcmp(name, "multiline")) goto readonly;
		if (!strcmp(name, "lastIndex")) goto readonly;
	}

	else if (obj->type == JS_CUSERDATA) {
		if (obj->u.user.put && obj->u.user.put(J, obj->u.user.data, name))
			return;
	}

	ref = jsV_setproperty(J, obj, name);
	if (ref) {
		if (value) {
			if (!(ref->atts & JS_READONLY))
				ref->value = *value;
			else if (J->strict)
				js_typeerror(J, "'%s' is read-only", name);
		}
		if (getter) {
			if (!(ref->atts & JS_DONTCONF))
				ref->getter = getter;
			else if (J->strict)
				js_typeerror(J, "'%s' is non-configurable", name);
		}
		if (setter) {
			if (!(ref->atts & JS_DONTCONF))
				ref->setter = setter;
			else if (J->strict)
				js_typeerror(J, "'%s' is non-configurable", name);
		}
		ref->atts |= atts;
	}

	return;

readonly:
	if (J->strict || throw)
		js_typeerror(J, "'%s' is read-only or non-configurable", name);
}

static int jsR_delproperty(js_State *J, js_Object *obj, const char *name)
{
	js_Property *ref;
	int k;

	if (obj->type == JS_CARRAY) {
		if (!strcmp(name, "length"))
			goto dontconf;
		if (obj->u.a.simple)
			jsR_unflattenarray(J, obj);
	}

	else if (obj->type == JS_CSTRING) {
		if (!strcmp(name, "length"))
			goto dontconf;
		if (js_isarrayindex(J, name, &k))
			if (k >= 0 && k < obj->u.s.length)
				goto dontconf;
	}

	else if (obj->type == JS_CREGEXP) {
		if (!strcmp(name, "source")) goto dontconf;
		if (!strcmp(name, "global")) goto dontconf;
		if (!strcmp(name, "ignoreCase")) goto dontconf;
		if (!strcmp(name, "multiline")) goto dontconf;
		if (!strcmp(name, "lastIndex")) goto dontconf;
	}

	else if (obj->type == JS_CUSERDATA) {
		if (obj->u.user.delete && obj->u.user.delete(J, obj->u.user.data, name))
			return 1;
	}

	ref = jsV_getownproperty(J, obj, name);
	if (ref) {
		if (ref->atts & JS_DONTCONF)
			goto dontconf;
		jsV_delproperty(J, obj, name);
	}
	return 1;

dontconf:
	if (J->strict)
		js_typeerror(J, "'%s' is non-configurable", name);
	return 0;
}

static void jsR_delindex(js_State *J, js_Object *obj, int k)
{
	char buf[32];

	if (obj->type == JS_CARRAY && obj->u.a.simple && k == obj->u.a.flat_length - 1)
		obj->u.a.flat_length = k;
	else
		jsR_delproperty(J, obj, js_itoa(buf, k));
}

const char *js_ref(js_State *J)
{
	js_Value *v = stackidx(J, -1);
	const char *s;
	char buf[32];
	switch (v->t.type) {
	case JS_TUNDEFINED: s = "_Undefined"; break;
	case JS_TNULL: s = "_Null"; break;
	case JS_TBOOLEAN:
		s = v->u.boolean ? "_True" : "_False";
		break;
	case JS_TOBJECT:
		sprintf(buf, "%p", (void*)v->u.object);
		s = js_intern(J, buf);
		break;
	default:
		sprintf(buf, "%d", J->nextref++);
		s = js_intern(J, buf);
		break;
	}
	js_setregistry(J, s);
	return s;
}

void js_unref(js_State *J, const char *ref)
{
	js_delregistry(J, ref);
}

void js_getregistry(js_State *J, const char *name)
{
	jsR_getproperty(J, J->R, name);
}

void js_setregistry(js_State *J, const char *name)
{
	jsR_setproperty(J, J->R, name, 0);
	js_pop(J, 1);
}

void js_delregistry(js_State *J, const char *name)
{
	jsR_delproperty(J, J->R, name);
}

void js_getglobal(js_State *J, const char *name)
{
	jsR_getproperty(J, J->G, name);
}

void js_setglobal(js_State *J, const char *name)
{
	jsR_setproperty(J, J->G, name, 0);
	js_pop(J, 1);
}

void js_defglobal(js_State *J, const char *name, int atts)
{
	jsR_defproperty(J, J->G, name, atts, stackidx(J, -1), NULL, NULL, 0);
	js_pop(J, 1);
}

void js_delglobal(js_State *J, const char *name)
{
	jsR_delproperty(J, J->G, name);
}

void js_getproperty(js_State *J, int idx, const char *name)
{
	jsR_getproperty(J, js_toobject(J, idx), name);
}

void js_setproperty(js_State *J, int idx, const char *name)
{
	jsR_setproperty(J, js_toobject(J, idx), name, !js_isobject(J, idx));
	js_pop(J, 1);
}

void js_defproperty(js_State *J, int idx, const char *name, int atts)
{
	jsR_defproperty(J, js_toobject(J, idx), name, atts, stackidx(J, -1), NULL, NULL, 1);
	js_pop(J, 1);
}

void js_delproperty(js_State *J, int idx, const char *name)
{
	jsR_delproperty(J, js_toobject(J, idx), name);
}

void js_defaccessor(js_State *J, int idx, const char *name, int atts)
{
	jsR_defproperty(J, js_toobject(J, idx), name, atts, NULL, jsR_tofunction(J, -2), jsR_tofunction(J, -1), 1);
	js_pop(J, 2);
}

int js_hasproperty(js_State *J, int idx, const char *name)
{
	return jsR_hasproperty(J, js_toobject(J, idx), name);
}

void js_getindex(js_State *J, int idx, int i)
{
	jsR_getindex(J, js_toobject(J, idx), i);
}

int js_hasindex(js_State *J, int idx, int i)
{
	return jsR_hasindex(J, js_toobject(J, idx), i);
}

void js_setindex(js_State *J, int idx, int i)
{
	jsR_setindex(J, js_toobject(J, idx), i, !js_isobject(J, idx));
	js_pop(J, 1);
}

void js_delindex(js_State *J, int idx, int i)
{
	jsR_delindex(J, js_toobject(J, idx), i);
}

void js_pushiterator(js_State *J, int idx, int own)
{
	js_pushobject(J, jsV_newiterator(J, js_toobject(J, idx), own));
}

const char *js_nextiterator(js_State *J, int idx)
{
	return jsV_nextiterator(J, js_toobject(J, idx));
}

js_Environment *jsR_newenvironment(js_State *J, js_Object *vars, js_Environment *outer)
{
	js_Environment *E = js_malloc(J, sizeof *E);
	E->gcmark = 0;
	E->gcnext = J->gcenv;
	J->gcenv = E;
	++J->gccounter;

	E->outer = outer;
	E->variables = vars;
	return E;
}

static void js_initvar(js_State *J, const char *name, int idx)
{
	jsR_defproperty(J, J->E->variables, name, JS_DONTENUM | JS_DONTCONF, stackidx(J, idx), NULL, NULL, 0);
}

static int js_hasvar(js_State *J, const char *name)
{
	js_Environment *E = J->E;
	do {
		js_Property *ref = jsV_getproperty(J, E->variables, name);
		if (ref) {
			if (ref->getter) {
				js_pushobject(J, ref->getter);
				js_pushobject(J, E->variables);
				js_call(J, 0);
			} else {
				js_pushvalue(J, ref->value);
			}
			return 1;
		}
		E = E->outer;
	} while (E);
	return 0;
}

static void js_setvar(js_State *J, const char *name)
{
	js_Environment *E = J->E;
	do {
		js_Property *ref = jsV_getproperty(J, E->variables, name);
		if (ref) {
			if (ref->setter) {
				js_pushobject(J, ref->setter);
				js_pushobject(J, E->variables);
				js_copy(J, -3);
				js_call(J, 1);
				js_pop(J, 1);
				return;
			}
			if (!(ref->atts & JS_READONLY))
				ref->value = *stackidx(J, -1);
			else if (J->strict)
				js_typeerror(J, "'%s' is read-only", name);
			return;
		}
		E = E->outer;
	} while (E);
	if (J->strict)
		js_referenceerror(J, "assignment to undeclared variable '%s'", name);
	jsR_setproperty(J, J->G, name, 0);
}

static int js_delvar(js_State *J, const char *name)
{
	js_Environment *E = J->E;
	do {
		js_Property *ref = jsV_getownproperty(J, E->variables, name);
		if (ref) {
			if (ref->atts & JS_DONTCONF) {
				if (J->strict)
					js_typeerror(J, "'%s' is non-configurable", name);
				return 0;
			}
			jsV_delproperty(J, E->variables, name);
			return 1;
		}
		E = E->outer;
	} while (E);
	return jsR_delproperty(J, J->G, name);
}

static void jsR_savescope(js_State *J, js_Environment *newE)
{
	if (J->envtop + 1 >= JS_ENVLIMIT)
		js_stackoverflow(J);
	J->envstack[J->envtop++] = J->E;
	J->E = newE;
}

static void jsR_restorescope(js_State *J)
{
	J->E = J->envstack[--J->envtop];
}

static void jsR_calllwfunction(js_State *J, int n, js_Function *F, js_Environment *scope)
{
	js_Value v;
	int i;

	jsR_savescope(J, scope);

	if (n > F->numparams) {
		js_pop(J, n - F->numparams);
		n = F->numparams;
	}

	for (i = n; i < F->varlen; ++i)
		js_pushundefined(J);

	jsR_run(J, F);
	v = *stackidx(J, -1);
	TOP = --BOT;
	js_pushvalue(J, v);

	jsR_restorescope(J);
}

static void jsR_callfunction(js_State *J, int n, js_Function *F, js_Environment *scope)
{
	js_Value v;
	int i;

	scope = jsR_newenvironment(J, jsV_newobject(J, JS_COBJECT, NULL), scope);

	jsR_savescope(J, scope);

	if (F->arguments) {
		js_newarguments(J);
		if (!J->strict) {
			js_currentfunction(J);
			js_defproperty(J, -2, "callee", JS_DONTENUM);
		}
		js_pushnumber(J, n);
		js_defproperty(J, -2, "length", JS_DONTENUM);
		for (i = 0; i < n; ++i) {
			js_copy(J, i + 1);
			js_setindex(J, -2, i);
		}
		js_initvar(J, "arguments", -1);
		js_pop(J, 1);
	}

	for (i = 0; i < n && i < F->numparams; ++i)
		js_initvar(J, F->vartab[i], i + 1);
	js_pop(J, n);

	for (; i < F->varlen; ++i) {
		js_pushundefined(J);
		js_initvar(J, F->vartab[i], -1);
		js_pop(J, 1);
	}

	jsR_run(J, F);
	v = *stackidx(J, -1);
	TOP = --BOT;
	js_pushvalue(J, v);

	jsR_restorescope(J);
}

static void jsR_callscript(js_State *J, int n, js_Function *F, js_Environment *scope)
{
	js_Value v;
	int i;

	if (scope)
		jsR_savescope(J, scope);

	js_pop(J, n);

	for (i = 0; i < F->varlen; ++i) {

		if (!js_hasvar(J, F->vartab[i])) {
			js_pushundefined(J);
			js_initvar(J, F->vartab[i], -1);
			js_pop(J, 1);
		}
	}

	jsR_run(J, F);
	v = *stackidx(J, -1);
	TOP = --BOT;
	js_pushvalue(J, v);

	if (scope)
		jsR_restorescope(J);
}

static void jsR_callcfunction(js_State *J, int n, int min, js_CFunction F)
{
	int save_top;
	int i;
	js_Value v;

	for (i = n; i < min; ++i)
		js_pushundefined(J);

	save_top = TOP;
	F(J);
	if (TOP > save_top) {
		v = *stackidx(J, -1);
		TOP = --BOT;
		js_pushvalue(J, v);
	} else {
		TOP = --BOT;
		js_pushundefined(J);
	}
}

static void jsR_pushtrace(js_State *J, const char *name, const char *file, int line)
{
	if (J->tracetop + 1 == JS_ENVLIMIT)
		js_error(J, "call stack overflow");
	++J->tracetop;
	J->trace[J->tracetop].stack = J->bot;
	J->trace[J->tracetop].name = name;
	J->trace[J->tracetop].file = file;
	J->trace[J->tracetop].line = line;
}

void js_call(js_State *J, int n)
{
	js_Object *obj;
	int savebot;

	if (n < 0)
		js_rangeerror(J, "number of arguments cannot be negative");

	if (!js_iscallable(J, -n-2))
		js_typeerror(J, "%s is not callable", js_typeof(J, -n-2));

	obj = js_toobject(J, -n-2);

	savebot = BOT;
	BOT = TOP - n - 1;

	if (obj->type == JS_CFUNCTION) {
		jsR_pushtrace(J, obj->u.f.function->name, obj->u.f.function->filename, obj->u.f.function->line);
		if (obj->u.f.function->lightweight)
			jsR_calllwfunction(J, n, obj->u.f.function, obj->u.f.scope);
		else
			jsR_callfunction(J, n, obj->u.f.function, obj->u.f.scope);
		--J->tracetop;
	} else if (obj->type == JS_CSCRIPT) {
		jsR_pushtrace(J, obj->u.f.function->name, obj->u.f.function->filename, obj->u.f.function->line);
		jsR_callscript(J, n, obj->u.f.function, obj->u.f.scope);
		--J->tracetop;
	} else if (obj->type == JS_CCFUNCTION) {
		jsR_pushtrace(J, obj->u.c.name, "native", 0);
		jsR_callcfunction(J, n, obj->u.c.length, obj->u.c.function);
		--J->tracetop;
	}

	BOT = savebot;
}

void js_construct(js_State *J, int n)
{
	js_Object *obj;
	js_Object *prototype;
	js_Object *newobj;

	if (!js_iscallable(J, -n-1))
		js_typeerror(J, "%s is not callable", js_typeof(J, -n-1));

	obj = js_toobject(J, -n-1);

	if (obj->type == JS_CCFUNCTION && obj->u.c.constructor) {
		int savebot = BOT;
		js_pushnull(J);
		if (n > 0)
			js_rot(J, n + 1);
		BOT = TOP - n - 1;

		jsR_pushtrace(J, obj->u.c.name, "native", 0);
		jsR_callcfunction(J, n, obj->u.c.length, obj->u.c.constructor);
		--J->tracetop;

		BOT = savebot;
		return;
	}

	js_getproperty(J, -n - 1, "prototype");
	if (js_isobject(J, -1))
		prototype = js_toobject(J, -1);
	else
		prototype = J->Object_prototype;
	js_pop(J, 1);

	newobj = jsV_newobject(J, JS_COBJECT, prototype);
	js_pushobject(J, newobj);
	if (n > 0)
		js_rot(J, n + 1);

	js_pushobject(J, newobj);
	js_rot(J, n + 3);

	js_call(J, n);

	if (!js_isobject(J, -1)) {
		js_pop(J, 1);
	} else {
		js_rot2pop1(J);
	}
}

void js_eval(js_State *J)
{
	if (!js_isstring(J, -1))
		return;
	js_loadeval(J, "(eval)", js_tostring(J, -1));
	js_rot2pop1(J);
	js_copy(J, 0);
	js_call(J, 0);
}

int js_pconstruct(js_State *J, int n)
{
	int savetop = TOP - n - 2;
	if (js_try(J)) {

		STACK[savetop] = STACK[TOP-1];
		TOP = savetop + 1;
		return 1;
	}
	js_construct(J, n);
	js_endtry(J);
	return 0;
}

int js_pcall(js_State *J, int n)
{
	int savetop = TOP - n - 2;
	if (js_try(J)) {

		STACK[savetop] = STACK[TOP-1];
		TOP = savetop + 1;
		return 1;
	}
	js_call(J, n);
	js_endtry(J);
	return 0;
}

void *js_savetrypc(js_State *J, js_Instruction *pc)
{
	if (J->trytop == JS_TRYLIMIT)
		js_trystackoverflow(J);
	J->trybuf[J->trytop].E = J->E;
	J->trybuf[J->trytop].envtop = J->envtop;
	J->trybuf[J->trytop].tracetop = J->tracetop;
	J->trybuf[J->trytop].top = J->top;
	J->trybuf[J->trytop].bot = J->bot;
	J->trybuf[J->trytop].strict = J->strict;
	J->trybuf[J->trytop].pc = pc;
	return J->trybuf[J->trytop++].buf;
}

void *js_savetry(js_State *J)
{
	if (J->trytop == JS_TRYLIMIT)
		js_trystackoverflow(J);
	J->trybuf[J->trytop].E = J->E;
	J->trybuf[J->trytop].envtop = J->envtop;
	J->trybuf[J->trytop].tracetop = J->tracetop;
	J->trybuf[J->trytop].top = J->top;
	J->trybuf[J->trytop].bot = J->bot;
	J->trybuf[J->trytop].strict = J->strict;
	J->trybuf[J->trytop].pc = NULL;
	return J->trybuf[J->trytop++].buf;
}

void js_endtry(js_State *J)
{
	if (J->trytop == 0)
		js_error(J, "endtry: exception stack underflow");
	--J->trytop;
}

void js_throw(js_State *J)
{
	if (J->trytop > 0) {
		js_Value v = *stackidx(J, -1);
		--J->trytop;
		J->E = J->trybuf[J->trytop].E;
		J->envtop = J->trybuf[J->trytop].envtop;
		J->tracetop = J->trybuf[J->trytop].tracetop;
		J->top = J->trybuf[J->trytop].top;
		J->bot = J->trybuf[J->trytop].bot;
		J->strict = J->trybuf[J->trytop].strict;
		js_pushvalue(J, v);
		longjmp(J->trybuf[J->trytop].buf, 1);
	}
	if (J->panic)
		J->panic(J);
	abort();
}

static void js_dumpvalue(js_State *J, js_Value v)
{
	switch (v.t.type) {
	case JS_TUNDEFINED: printf("undefined"); break;
	case JS_TNULL: printf("null"); break;
	case JS_TBOOLEAN: printf(v.u.boolean ? "true" : "false"); break;
	case JS_TNUMBER: printf("%.9g", v.u.number); break;
	case JS_TSHRSTR: printf("'%s'", v.u.shrstr); break;
	case JS_TLITSTR: printf("'%s'", v.u.litstr); break;
	case JS_TMEMSTR: printf("'%s'", v.u.memstr->p); break;
	case JS_TOBJECT:
		if (v.u.object == J->G) {
			printf("[Global]");
			break;
		}
		switch (v.u.object->type) {
		case JS_COBJECT: printf("[Object %p]", (void*)v.u.object); break;
		case JS_CARRAY: printf("[Array %p]", (void*)v.u.object); break;
		case JS_CFUNCTION:
			printf("[Function %p, %s, %s:%d]",
				(void*)v.u.object,
				v.u.object->u.f.function->name,
				v.u.object->u.f.function->filename,
				v.u.object->u.f.function->line);
			break;
		case JS_CSCRIPT: printf("[Script %s]", v.u.object->u.f.function->filename); break;
		case JS_CCFUNCTION: printf("[CFunction %s]", v.u.object->u.c.name); break;
		case JS_CBOOLEAN: printf("[Boolean %d]", v.u.object->u.boolean); break;
		case JS_CNUMBER: printf("[Number %g]", v.u.object->u.number); break;
		case JS_CSTRING: printf("[String'%s']", v.u.object->u.s.string); break;
		case JS_CERROR: printf("[Error]"); break;
		case JS_CARGUMENTS: printf("[Arguments %p]", (void*)v.u.object); break;
		case JS_CITERATOR: printf("[Iterator %p]", (void*)v.u.object); break;
		case JS_CUSERDATA:
			printf("[Userdata %s %p]", v.u.object->u.user.tag, v.u.object->u.user.data);
			break;
		default: printf("[Object %p]", (void*)v.u.object); break;
		}
		break;
	}
}

static void js_stacktrace(js_State *J)
{
	int n;
	printf("stack trace:\n");
	for (n = J->tracetop; n >= 0; --n) {
		const char *name = J->trace[n].name;
		const char *file = J->trace[n].file;
		int line = J->trace[n].line;
		if (line > 0) {
			if (name[0])
				printf("\tat %s (%s:%d)\n", name, file, line);
			else
				printf("\tat %s:%d\n", file, line);
		} else
			printf("\tat %s (%s)\n", name, file);
	}
}

static void js_dumpstack(js_State *J)
{
	int i;
	printf("stack {\n");
	for (i = 0; i < TOP; ++i) {
		putchar(i == BOT ? '>' : ' ');
		printf("%4d: ", i);
		js_dumpvalue(J, STACK[i]);
		putchar('\n');
	}
	printf("}\n");
}

void js_trap(js_State *J, int pc)
{
	js_dumpstack(J);
	js_stacktrace(J);
}

static int jsR_isindex(js_State *J, int idx, int *k)
{
	js_Value *v = stackidx(J, idx);
	if (v->t.type == JS_TNUMBER) {
		*k = v->u.number;
		return *k == v->u.number && *k >= 0;
	}
	return 0;
}

static void jsR_run(js_State *J, js_Function *F)
{
	js_Function **FT = F->funtab;
	const char **VT = F->vartab ? F->vartab - 1 : NULL;
	int lightweight = F->lightweight;
	js_Instruction *pcstart = F->code;
	js_Instruction *pc = F->code;
	enum js_OpCode opcode;
	int offset;
	int savestrict;

	const char *str;
	js_Object *obj;
	double x, y;
	unsigned int ux, uy;
	int ix, iy, okay;
	int b;
	int transient;

	savestrict = J->strict;
	J->strict = F->strict;

#define READSTRING() \
	memcpy(&str, pc, sizeof(str)); \
	pc += sizeof(str) / sizeof(*pc)

	while (1) {
		if (J->runlimit > 0) {
			if (J->runlimit == 1)
				js_runlimit(J);
			--J->runlimit;
		}

		if (J->gccounter > J->gcthresh)
			js_gc(J, 0);

		J->trace[J->tracetop].line = *pc++;

		opcode = *pc++;

		switch (opcode) {
		case OP_POP: js_pop(J, 1); break;
		case OP_DUP: js_dup(J); break;
		case OP_DUP2: js_dup2(J); break;
		case OP_ROT2: js_rot2(J); break;
		case OP_ROT3: js_rot3(J); break;
		case OP_ROT4: js_rot4(J); break;

		case OP_INTEGER:
			js_pushnumber(J, *pc++ - 32768);
			break;

		case OP_NUMBER:
			memcpy(&x, pc, sizeof(x));
			pc += sizeof(x) / sizeof(*pc);
			js_pushnumber(J, x);
			break;

		case OP_STRING:
			READSTRING();
			js_pushliteral(J, str);
			break;

		case OP_CLOSURE: js_newfunction(J, FT[*pc++], J->E); break;
		case OP_NEWOBJECT: js_newobject(J); break;
		case OP_NEWARRAY: js_newarray(J); break;
		case OP_NEWREGEXP:
			READSTRING();
			js_newregexp(J, str, *pc++);
			break;

		case OP_UNDEF: js_pushundefined(J); break;
		case OP_NULL: js_pushnull(J); break;
		case OP_TRUE: js_pushboolean(J, 1); break;
		case OP_FALSE: js_pushboolean(J, 0); break;

		case OP_THIS:
			if (J->strict) {
				js_copy(J, 0);
			} else {
				if (js_iscoercible(J, 0))
					js_copy(J, 0);
				else
					js_pushglobal(J);
			}
			break;

		case OP_CURRENT:
			js_currentfunction(J);
			break;

		case OP_GETLOCAL:
			if (lightweight) {
				CHECKSTACK(1);
				STACK[TOP++] = STACK[BOT + *pc++];
			} else {
				str = VT[*pc++];
				if (!js_hasvar(J, str))
					js_referenceerror(J, "'%s' is not defined", str);
			}
			break;

		case OP_SETLOCAL:
			if (lightweight) {
				STACK[BOT + *pc++] = STACK[TOP-1];
			} else {
				js_setvar(J, VT[*pc++]);
			}
			break;

		case OP_DELLOCAL:
			if (lightweight) {
				++pc;
				js_pushboolean(J, 0);
			} else {
				b = js_delvar(J, VT[*pc++]);
				js_pushboolean(J, b);
			}
			break;

		case OP_GETVAR:
			READSTRING();
			if (!js_hasvar(J, str))
				js_referenceerror(J, "'%s' is not defined", str);
			break;

		case OP_HASVAR:
			READSTRING();
			if (!js_hasvar(J, str))
				js_pushundefined(J);
			break;

		case OP_SETVAR:
			READSTRING();
			js_setvar(J, str);
			break;

		case OP_DELVAR:
			READSTRING();
			b = js_delvar(J, str);
			js_pushboolean(J, b);
			break;

		case OP_IN:
			str = js_tostring(J, -2);
			if (!js_isobject(J, -1))
				js_typeerror(J, "operand to 'in' is not an object");
			b = js_hasproperty(J, -1, str);
			js_pop(J, 2 + b);
			js_pushboolean(J, b);
			break;

		case OP_SKIPARRAY:
			js_setlength(J, -1, js_getlength(J, -1) + 1);
			break;
		case OP_INITARRAY:
			js_setindex(J, -2, js_getlength(J, -2));
			break;

		case OP_INITPROP:
			obj = js_toobject(J, -3);
			str = js_tostring(J, -2);
			jsR_setproperty(J, obj, str, 0);
			js_pop(J, 2);
			break;

		case OP_INITGETTER:
			obj = js_toobject(J, -3);
			str = js_tostring(J, -2);
			jsR_defproperty(J, obj, str, 0, NULL, jsR_tofunction(J, -1), NULL, 0);
			js_pop(J, 2);
			break;

		case OP_INITSETTER:
			obj = js_toobject(J, -3);
			str = js_tostring(J, -2);
			jsR_defproperty(J, obj, str, 0, NULL, NULL, jsR_tofunction(J, -1), 0);
			js_pop(J, 2);
			break;

		case OP_GETPROP:
			if (jsR_isindex(J, -1, &ix)) {
				obj = js_toobject(J, -2);
				jsR_getindex(J, obj, ix);
			} else {
				str = js_tostring(J, -1);
				obj = js_toobject(J, -2);
				jsR_getproperty(J, obj, str);
			}
			js_rot3pop2(J);
			break;

		case OP_GETPROP_S:
			READSTRING();
			obj = js_toobject(J, -1);
			jsR_getproperty(J, obj, str);
			js_rot2pop1(J);
			break;

		case OP_SETPROP:
			if (jsR_isindex(J, -2, &ix)) {
				obj = js_toobject(J, -3);
				transient = !js_isobject(J, -3);
				jsR_setindex(J, obj, ix, transient);
			} else {
				str = js_tostring(J, -2);
				obj = js_toobject(J, -3);
				transient = !js_isobject(J, -3);
				jsR_setproperty(J, obj, str, transient);
			}
			js_rot3pop2(J);
			break;

		case OP_SETPROP_S:
			READSTRING();
			obj = js_toobject(J, -2);
			transient = !js_isobject(J, -2);
			jsR_setproperty(J, obj, str, transient);
			js_rot2pop1(J);
			break;

		case OP_DELPROP:
			str = js_tostring(J, -1);
			obj = js_toobject(J, -2);
			b = jsR_delproperty(J, obj, str);
			js_pop(J, 2);
			js_pushboolean(J, b);
			break;

		case OP_DELPROP_S:
			READSTRING();
			obj = js_toobject(J, -1);
			b = jsR_delproperty(J, obj, str);
			js_pop(J, 1);
			js_pushboolean(J, b);
			break;

		case OP_ITERATOR:
			if (js_iscoercible(J, -1)) {
				obj = jsV_newiterator(J, js_toobject(J, -1), 0);
				js_pop(J, 1);
				js_pushobject(J, obj);
			}
			break;

		case OP_NEXTITER:
			if (js_isobject(J, -1)) {
				obj = js_toobject(J, -1);
				str = jsV_nextiterator(J, obj);
				if (str) {
					js_pushstring(J, str);
					js_pushboolean(J, 1);
				} else {
					js_pop(J, 1);
					js_pushboolean(J, 0);
				}
			} else {
				js_pop(J, 1);
				js_pushboolean(J, 0);
			}
			break;

		case OP_EVAL:
			js_eval(J);
			break;

		case OP_CALL:
			js_call(J, *pc++);
			break;

		case OP_NEW:
			js_construct(J, *pc++);
			break;

		case OP_TYPEOF:
			str = js_typeof(J, -1);
			js_pop(J, 1);
			js_pushliteral(J, str);
			break;

		case OP_POS:
			x = js_tonumber(J, -1);
			js_pop(J, 1);
			js_pushnumber(J, x);
			break;

		case OP_NEG:
			x = js_tonumber(J, -1);
			js_pop(J, 1);
			js_pushnumber(J, -x);
			break;

		case OP_BITNOT:
			ix = js_toint32(J, -1);
			js_pop(J, 1);
			js_pushnumber(J, ~ix);
			break;

		case OP_LOGNOT:
			b = js_toboolean(J, -1);
			js_pop(J, 1);
			js_pushboolean(J, !b);
			break;

		case OP_INC:
			x = js_tonumber(J, -1);
			js_pop(J, 1);
			js_pushnumber(J, x + 1);
			break;

		case OP_DEC:
			x = js_tonumber(J, -1);
			js_pop(J, 1);
			js_pushnumber(J, x - 1);
			break;

		case OP_POSTINC:
			x = js_tonumber(J, -1);
			js_pop(J, 1);
			js_pushnumber(J, x + 1);
			js_pushnumber(J, x);
			break;

		case OP_POSTDEC:
			x = js_tonumber(J, -1);
			js_pop(J, 1);
			js_pushnumber(J, x - 1);
			js_pushnumber(J, x);
			break;

		case OP_MUL:
			x = js_tonumber(J, -2);
			y = js_tonumber(J, -1);
			js_pop(J, 2);
			js_pushnumber(J, x * y);
			break;

		case OP_DIV:
			x = js_tonumber(J, -2);
			y = js_tonumber(J, -1);
			js_pop(J, 2);
			js_pushnumber(J, x / y);
			break;

		case OP_MOD:
			x = js_tonumber(J, -2);
			y = js_tonumber(J, -1);
			js_pop(J, 2);
			js_pushnumber(J, fmod(x, y));
			break;

		case OP_ADD:
			js_concat(J);
			break;

		case OP_SUB:
			x = js_tonumber(J, -2);
			y = js_tonumber(J, -1);
			js_pop(J, 2);
			js_pushnumber(J, x - y);
			break;

		case OP_SHL:
			ix = js_toint32(J, -2);
			uy = js_touint32(J, -1);
			js_pop(J, 2);
			js_pushnumber(J, ix << (uy & 0x1F));
			break;

		case OP_SHR:
			ix = js_toint32(J, -2);
			uy = js_touint32(J, -1);
			js_pop(J, 2);
			js_pushnumber(J, ix >> (uy & 0x1F));
			break;

		case OP_USHR:
			ux = js_touint32(J, -2);
			uy = js_touint32(J, -1);
			js_pop(J, 2);
			js_pushnumber(J, ux >> (uy & 0x1F));
			break;

		case OP_LT: b = js_compare(J, &okay); js_pop(J, 2); js_pushboolean(J, okay && b < 0); break;
		case OP_GT: b = js_compare(J, &okay); js_pop(J, 2); js_pushboolean(J, okay && b > 0); break;
		case OP_LE: b = js_compare(J, &okay); js_pop(J, 2); js_pushboolean(J, okay && b <= 0); break;
		case OP_GE: b = js_compare(J, &okay); js_pop(J, 2); js_pushboolean(J, okay && b >= 0); break;

		case OP_INSTANCEOF:
			b = js_instanceof(J);
			js_pop(J, 2);
			js_pushboolean(J, b);
			break;

		case OP_EQ: b = js_equal(J); js_pop(J, 2); js_pushboolean(J, b); break;
		case OP_NE: b = js_equal(J); js_pop(J, 2); js_pushboolean(J, !b); break;
		case OP_STRICTEQ: b = js_strictequal(J); js_pop(J, 2); js_pushboolean(J, b); break;
		case OP_STRICTNE: b = js_strictequal(J); js_pop(J, 2); js_pushboolean(J, !b); break;

		case OP_JCASE:
			offset = *pc++;
			b = js_strictequal(J);
			if (b) {
				js_pop(J, 2);
				pc = pcstart + offset;
			} else {
				js_pop(J, 1);
			}
			break;

		case OP_BITAND:
			ix = js_toint32(J, -2);
			iy = js_toint32(J, -1);
			js_pop(J, 2);
			js_pushnumber(J, ix & iy);
			break;

		case OP_BITXOR:
			ix = js_toint32(J, -2);
			iy = js_toint32(J, -1);
			js_pop(J, 2);
			js_pushnumber(J, ix ^ iy);
			break;

		case OP_BITOR:
			ix = js_toint32(J, -2);
			iy = js_toint32(J, -1);
			js_pop(J, 2);
			js_pushnumber(J, ix | iy);
			break;

		case OP_THROW:
			js_throw(J);

		case OP_TRY:
			offset = *pc++;
			if (js_trypc(J, pc)) {
				pc = J->trybuf[J->trytop].pc;
			} else {
				pc = pcstart + offset;
			}
			break;

		case OP_ENDTRY:
			js_endtry(J);
			break;

		case OP_CATCH:
			READSTRING();
			obj = jsV_newobject(J, JS_COBJECT, NULL);
			js_pushobject(J, obj);
			js_rot2(J);
			js_setproperty(J, -2, str);
			J->E = jsR_newenvironment(J, obj, J->E);
			js_pop(J, 1);
			break;

		case OP_ENDCATCH:
			J->E = J->E->outer;
			break;

		case OP_WITH:
			obj = js_toobject(J, -1);
			J->E = jsR_newenvironment(J, obj, J->E);
			js_pop(J, 1);
			break;

		case OP_ENDWITH:
			J->E = J->E->outer;
			break;

		case OP_DEBUGGER:
			js_trap(J, (int)(pc - pcstart) - 1);
			break;

		case OP_JUMP:
			pc = pcstart + *pc;
			break;

		case OP_JTRUE:
			offset = *pc++;
			b = js_toboolean(J, -1);
			js_pop(J, 1);
			if (b)
				pc = pcstart + offset;
			break;

		case OP_JFALSE:
			offset = *pc++;
			b = js_toboolean(J, -1);
			js_pop(J, 1);
			if (!b)
				pc = pcstart + offset;
			break;

		case OP_RETURN:
			J->strict = savestrict;
			return;
		}
	}
}

#include <assert.h>
#include <errno.h>

static int js_ptry(js_State *J) {
	if (J->trytop == JS_TRYLIMIT) {
		J->stack[J->top].t.type = JS_TLITSTR;
		J->stack[J->top].u.litstr = "exception stack overflow";
		++J->top;
		return 1;
	}
	return 0;
}

static void *js_defaultalloc(void *actx, void *ptr, int size)
{
	if (size == 0) {
		free(ptr);
		return NULL;
	}
	return realloc(ptr, (size_t)size);
}

static void js_defaultreport(js_State *J, const char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
}

static void js_defaultpanic(js_State *J)
{
	js_report(J, "uncaught exception");

}

int js_ploadstring(js_State *J, const char *filename, const char *source)
{
	if (js_ptry(J))
		return 1;
	if (js_try(J))
		return 1;
	js_loadstring(J, filename, source);
	js_endtry(J);
	return 0;
}

int js_ploadfile(js_State *J, const char *filename)
{
	if (js_ptry(J))
		return 1;
	if (js_try(J))
		return 1;
	js_loadfile(J, filename);
	js_endtry(J);
	return 0;
}

const char *js_trystring(js_State *J, int idx, const char *error)
{
	const char *s;
	if (js_ptry(J)) {
		js_pop(J, 1);
		return error;
	}
	if (js_try(J)) {
		js_pop(J, 1);
		return error;
	}
	s = js_tostring(J, idx);
	js_endtry(J);
	return s;
}

double js_trynumber(js_State *J, int idx, double error)
{
	double v;
	if (js_ptry(J)) {
		js_pop(J, 1);
		return error;
	}
	if (js_try(J)) {
		js_pop(J, 1);
		return error;
	}
	v = js_tonumber(J, idx);
	js_endtry(J);
	return v;
}

int js_tryinteger(js_State *J, int idx, int error)
{
	int v;
	if (js_ptry(J)) {
		js_pop(J, 1);
		return error;
	}
	if (js_try(J)) {
		js_pop(J, 1);
		return error;
	}
	v = js_tointeger(J, idx);
	js_endtry(J);
	return v;
}

int js_tryboolean(js_State *J, int idx, int error)
{
	int v;
	if (js_ptry(J)) {
		js_pop(J, 1);
		return error;
	}
	if (js_try(J)) {
		js_pop(J, 1);
		return error;
	}
	v = js_toboolean(J, idx);
	js_endtry(J);
	return v;
}

static void js_loadstringx(js_State *J, const char *filename, const char *source, int iseval)
{
	js_Ast *P;
	js_Function *F;

	if (js_try(J)) {
		jsP_freeparse(J);
		js_throw(J);
	}

	P = jsP_parse(J, filename, source);
	F = jsC_compilescript(J, P, iseval ? J->strict : J->default_strict);
	jsP_freeparse(J);
	js_newscript(J, F, iseval ? (J->strict ? J->E : NULL) : J->GE);

	js_endtry(J);
}

void js_loadeval(js_State *J, const char *filename, const char *source)
{
	js_loadstringx(J, filename, source, 1);
}

void js_loadstring(js_State *J, const char *filename, const char *source)
{
	js_loadstringx(J, filename, source, 0);
}

void js_loadfile(js_State *J, const char *filename)
{
	FILE *f;
	char *s, *p;
	int n, t;

	f = fopen(filename, "rb");
	if (!f) {
		js_error(J, "cannot open file '%s': %s", filename, strerror(errno));
	}

	if (fseek(f, 0, SEEK_END) < 0) {
		fclose(f);
		js_error(J, "cannot seek in file '%s': %s", filename, strerror(errno));
	}

	n = ftell(f);
	if (n < 0) {
		fclose(f);
		js_error(J, "cannot tell in file '%s': %s", filename, strerror(errno));
	}

	if (fseek(f, 0, SEEK_SET) < 0) {
		fclose(f);
		js_error(J, "cannot seek in file '%s': %s", filename, strerror(errno));
	}

	if (js_try(J)) {
		fclose(f);
		js_throw(J);
	}
	s = js_malloc(J, n + 1);
	js_endtry(J);

	t = fread(s, 1, (size_t)n, f);
	if (t != n) {
		js_free(J, s);
		fclose(f);
		js_error(J, "cannot read data from file '%s': %s", filename, strerror(errno));
	}

	s[n] = 0;

	if (js_try(J)) {
		js_free(J, s);
		fclose(f);
		js_throw(J);
	}

	p = s;
	if (p[0] == '#' && p[1] == '!') {
		p += 2;
		while (*p && *p != '\n')
			++p;
	}

	js_loadstring(J, filename, p);

	js_free(J, s);
	fclose(f);
	js_endtry(J);
}

int js_dostring(js_State *J, const char *source)
{
	if (js_ptry(J)) {
		js_report(J, "exception stack overflow");
		js_pop(J, 1);
		return 1;
	}
	if (js_try(J)) {
		js_report(J, js_trystring(J, -1, "Error"));
		js_pop(J, 1);
		return 1;
	}
	js_loadstring(J, "[string]", source);
	js_pushundefined(J);
	js_call(J, 0);
	js_pop(J, 1);
	js_endtry(J);
	return 0;
}

int js_dofile(js_State *J, const char *filename)
{
	if (js_ptry(J)) {
		js_report(J, "exception stack overflow");
		js_pop(J, 1);
		return 1;
	}
	if (js_try(J)) {
		js_report(J, js_trystring(J, -1, "Error"));
		js_pop(J, 1);
		return 1;
	}
	js_loadfile(J, filename);
	js_pushundefined(J);
	js_call(J, 0);
	js_pop(J, 1);
	js_endtry(J);
	return 0;
}

js_Panic js_atpanic(js_State *J, js_Panic panic)
{
	js_Panic old = J->panic;
	J->panic = panic;
	return old;
}

void js_report(js_State *J, const char *message)
{
	if (J->report)
		J->report(J, message);
}

void js_setreport(js_State *J, js_Report report)
{
	J->report = report;
}

void js_setcontext(js_State *J, void *uctx)
{
	J->uctx = uctx;
}

void *js_getcontext(js_State *J)
{
	return J->uctx;
}

js_State *js_newstate(js_Alloc alloc, void *actx, int flags)
{
	js_State *J;

	assert(sizeof(js_Value) == 16);
	assert(soffsetof(js_Value, t.type) == 15);

	if (!alloc)
		alloc = js_defaultalloc;

	J = alloc(actx, NULL, sizeof *J);
	if (!J)
		return NULL;
	memset(J, 0, sizeof(*J));
	J->actx = actx;
	J->alloc = alloc;

	if (flags & JS_STRICT)
		J->strict = J->default_strict = 1;

	J->trace[0].name = "-top-";
	J->trace[0].file = "native";
	J->trace[0].line = 0;

	J->report = js_defaultreport;
	J->panic = js_defaultpanic;

	J->stack = alloc(actx, NULL, JS_STACKSIZE * sizeof *J->stack);
	if (!J->stack) {
		alloc(actx, J, 0);
		return NULL;
	}

	J->gcmark = 1;
	J->nextref = 0;
	J->gcthresh = 0;

	if (js_try(J)) {
		js_freestate(J);
		return NULL;
	}

	J->R = jsV_newobject(J, JS_COBJECT, NULL);
	J->G = jsV_newobject(J, JS_COBJECT, NULL);
	J->E = jsR_newenvironment(J, J->G, NULL);
	J->GE = J->E;

	jsB_init(J);

	js_endtry(J);
	return J;
}

static int js_doregexec(js_State *J, Reprog *prog, const char *string, Resub *sub, int eflags)
{
	int result = js_regexec(prog, string, sub, eflags);
	if (result < 0)
		js_error(J, "regexec failed");
	return result;
}

static const char *checkstring(js_State *J, int idx)
{
	if (!js_iscoercible(J, idx))
		js_typeerror(J, "string function called on null or undefined");
	return js_tostring(J, idx);
}

int js_runeat(js_State *J, const char *s, int i)
{
	Rune rune = EOF;
	while (i >= 0) {
		rune = *(unsigned char*)s;
		if (rune < Runeself) {
			if (rune == 0)
				return EOF;
			++s;
			--i;
		} else {
			s += chartorune(&rune, s);
			if (rune >= 0x10000)
				i -= 2;
			else
				--i;
		}
	}
	if (rune >= 0x10000) {

		if (i == -2)
			return 0xd800 + ((rune - 0x10000) >> 10);

		else
			return 0xdc00 + ((rune - 0x10000) & 0x3ff);
	}
	return rune;
}

int js_utflen(const char *s)
{
	int c;
	int n;
	Rune rune;

	n = 0;
	for(;;) {
		c = *(unsigned char *)s;
		if (c < Runeself) {
			if (c == 0)
				return n;
			s++;
			n++;
		} else {
			s += chartorune(&rune, s);
			if (rune >= 0x10000)
				n += 2;
			else
				n++;
		}
	}
}

int js_utfptrtoidx(const char *s, const char *p)
{
	Rune rune;
	int i = 0;
	while (s < p) {
		if (*(unsigned char *)s < Runeself)
		{
			++s;
			++i;
		}
		else
		{
			s += chartorune(&rune, s);
			if (rune >= 0x10000)
				i += 2;
			else
				i += 1;
		}
	}
	return i;
}

static void jsB_new_String(js_State *J)
{
	js_newstring(J, js_gettop(J) > 1 ? js_tostring(J, 1) : "");
}

static void jsB_String(js_State *J)
{
	js_pushstring(J, js_gettop(J) > 1 ? js_tostring(J, 1) : "");
}

static void Sp_toString(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	if (self->type != JS_CSTRING) js_typeerror(J, "not a string");
	js_pushstring(J, self->u.s.string);
}

static void Sp_valueOf(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	if (self->type != JS_CSTRING) js_typeerror(J, "not a string");
	js_pushstring(J, self->u.s.string);
}

static void Sp_charAt(js_State *J)
{
	char buf[UTFmax + 1];
	const char *s = checkstring(J, 0);
	int pos = js_tointeger(J, 1);
	Rune rune = js_runeat(J, s, pos);
	if (rune >= 0) {
		buf[runetochar(buf, &rune)] = 0;
		js_pushstring(J, buf);
	} else {
		js_pushliteral(J, "");
	}
}

static void Sp_charCodeAt(js_State *J)
{
	const char *s = checkstring(J, 0);
	int pos = js_tointeger(J, 1);
	Rune rune = js_runeat(J, s, pos);
	if (rune >= 0)
		js_pushnumber(J, rune);
	else
		js_pushnumber(J, NAN);
}

static void Sp_concat(js_State *J)
{
	int i, top = js_gettop(J);
	int n;
	char * volatile out = NULL;
	const char *s;

	if (top == 1)
		return;

	s = checkstring(J, 0);
	n = 1 + strlen(s);

	if (js_try(J)) {
		js_free(J, out);
		js_throw(J);
	}

	if (n > JS_STRLIMIT)
		js_rangeerror(J, "invalid string length");
	out = js_malloc(J, n);
	strcpy(out, s);

	for (i = 1; i < top; ++i) {
		s = js_tostring(J, i);
		n += strlen(s);
		if (n > JS_STRLIMIT)
			js_rangeerror(J, "invalid string length");
		out = js_realloc(J, out, n);
		strcat(out, s);
	}

	js_pushstring(J, out);
	js_endtry(J);
	js_free(J, out);
}

static void Sp_indexOf(js_State *J)
{
	const char *haystack = checkstring(J, 0);
	const char *needle = js_tostring(J, 1);
	int pos = js_tointeger(J, 2);
	int len = strlen(needle);
	int k = 0;
	Rune rune;
	while (*haystack) {
		if (k >= pos && !strncmp(haystack, needle, len)) {
			js_pushnumber(J, k);
			return;
		}
		haystack += chartorune(&rune, haystack);
		++k;
	}
	js_pushnumber(J, -1);
}

static void Sp_lastIndexOf(js_State *J)
{
	const char *haystack = checkstring(J, 0);
	const char *needle = js_tostring(J, 1);
	int pos = js_isdefined(J, 2) ? js_tointeger(J, 2) : (int)strlen(haystack);
	int len = strlen(needle);
	int k = 0, last = -1;
	Rune rune;
	while (*haystack && k <= pos) {
		if (!strncmp(haystack, needle, len))
			last = k;
		haystack += chartorune(&rune, haystack);
		++k;
	}
	js_pushnumber(J, last);
}

static void Sp_localeCompare(js_State *J)
{
	const char *a = checkstring(J, 0);
	const char *b = js_tostring(J, 1);
	js_pushnumber(J, strcmp(a, b));
}

static void Sp_substring_imp(js_State *J, const char *s, int a, int n)
{
	Rune head_rune = 0, tail_rune = 0;
	const char *head, *tail;
	char *p;
	int i, k, head_len, tail_len;

	head = s;
	for (i = 0; i < a; ++i) {
		head += chartorune(&head_rune, head);
		if (head_rune >= 0x10000)
			++i;
	}

	tail = head;
	for (k = i - a; k < n; ++k) {
		tail += chartorune(&tail_rune, tail);
		if (tail_rune >= 0x10000)
			++k;
	}

	if (i == a && k == n) {
		js_pushlstring(J, head, tail - head);
		return;
	}

	if (js_try(J)) {
		js_free(J, p);
		js_throw(J);
	}

	p = js_malloc(J, UTFmax + (tail - head));

	if (i > a) {
		head_rune = 0xdc00 + ((head_rune - 0x10000) & 0x3ff);
		head_len = runetochar(p, &head_rune);
		memcpy(p + head_len, head, tail - head);
		js_pushlstring(J, p, head_len + (tail - head));
	}

	if (k > n) {
		tail -= runelen(tail_rune);
		memcpy(p, head, tail - head);
		tail_rune = 0xd800 + ((tail_rune - 0x10000) >> 10);
		tail_len = runetochar(p + (tail - head), &tail_rune);
		js_pushlstring(J, p, (tail - head) + tail_len);
	}

	js_endtry(J);
	js_free(J, p);
}

static void Sp_slice(js_State *J)
{
	const char *str = checkstring(J, 0);
	int len = js_utflen(str);
	int s = js_tointeger(J, 1);
	int e = js_isdefined(J, 2) ? js_tointeger(J, 2) : len;

	s = s < 0 ? s + len : s;
	e = e < 0 ? e + len : e;

	s = s < 0 ? 0 : s > len ? len : s;
	e = e < 0 ? 0 : e > len ? len : e;

	if (s < e)
		Sp_substring_imp(J, str, s, e - s);
	else if (s > e)
		Sp_substring_imp(J, str, e, s - e);
	else
		js_pushliteral(J, "");
}

static void Sp_substring(js_State *J)
{
	const char *str = checkstring(J, 0);
	int len = js_utflen(str);
	int s = js_tointeger(J, 1);
	int e = js_isdefined(J, 2) ? js_tointeger(J, 2) : len;

	s = s < 0 ? 0 : s > len ? len : s;
	e = e < 0 ? 0 : e > len ? len : e;

	if (s < e)
		Sp_substring_imp(J, str, s, e - s);
	else if (s > e)
		Sp_substring_imp(J, str, e, s - e);
	else
		js_pushliteral(J, "");
}

static void Sp_toLowerCase(js_State *J)
{
	const char *s, *s0 = checkstring(J, 0);
	char * volatile dst = NULL;
	char *d;
	Rune rune;
	const Rune *full;
	int n;

	n = 1;
	for (s = s0; *s;) {
		s += chartorune(&rune, s);
		full = tolowerrune_full(rune);
		if (full) {
			while (*full) {
				n += runelen(*full);
				++full;
			}
		} else {
			rune = tolowerrune(rune);
			n += runelen(rune);
		}
	}

	if (js_try(J)) {
		js_free(J, dst);
		js_throw(J);
	}

	d = dst = js_malloc(J, n);
	for (s = s0; *s;) {
		s += chartorune(&rune, s);
		full = tolowerrune_full(rune);
		if (full) {
			while (*full) {
				d += runetochar(d, full);
				++full;
			}
		} else {
			rune = tolowerrune(rune);
			d += runetochar(d, &rune);
		}
	}
	*d = 0;

	js_pushstring(J, dst);
	js_endtry(J);
	js_free(J, dst);
}

static void Sp_toUpperCase(js_State *J)
{
	const char *s, *s0 = checkstring(J, 0);
	char * volatile dst = NULL;
	char *d;
	const Rune *full;
	Rune rune;
	int n;

	n = 1;
	for (s = s0; *s;) {
		s += chartorune(&rune, s);
		full = toupperrune_full(rune);
		if (full) {
			while (*full) {
				n += runelen(*full);
				++full;
			}
		} else {
			rune = toupperrune(rune);
			n += runelen(rune);
		}
	}

	if (js_try(J)) {
		js_free(J, dst);
		js_throw(J);
	}

	d = dst = js_malloc(J, n);
	for (s = s0; *s;) {
		s += chartorune(&rune, s);
		full = toupperrune_full(rune);
		if (full) {
			while (*full) {
				d += runetochar(d, full);
				++full;
			}
		} else {
			rune = toupperrune(rune);
			d += runetochar(d, &rune);
		}
	}
	*d = 0;

	js_pushstring(J, dst);
	js_endtry(J);
	js_free(J, dst);
}

static int isbol(js_Regexp *re, const char *text, const char *a)
{
	return a == text || ((re->flags & JS_REGEXP_M) && a[-1] == '\n');
}

static int istrim(int c)
{
	return c == 0x9 || c == 0xB || c == 0xC || c == 0x20 || c == 0xA0 || c == 0xFEFF ||
		c == 0xA || c == 0xD || c == 0x2028 || c == 0x2029;
}

static void Sp_trim(js_State *J)
{
	const char *s, *e;
	s = checkstring(J, 0);
	while (istrim(*s))
		++s;
	e = s + strlen(s);
	while (e > s && istrim(e[-1]))
		--e;
	js_pushlstring(J, s, e - s);
}

static void S_fromCharCode(js_State *J)
{
	int i, top = js_gettop(J);
	char * volatile s = NULL;
	char *p;
	Rune c;

	if (js_try(J)) {
		js_free(J, s);
		js_throw(J);
	}

	s = p = js_malloc(J, (top-1) * UTFmax + 1);

	for (i = 1; i < top; ++i) {
		c = js_touint32(J, i);
		p += runetochar(p, &c);
	}
	*p = 0;

	js_pushstring(J, s);
	js_endtry(J);
	js_free(J, s);
}

static void Sp_match(js_State *J)
{
	js_Regexp *re;
	const char *text;
	int len;
	const char *a, *b, *c, *e;
	Resub m;
	Rune rune;

	text = checkstring(J, 0);

	if (js_isregexp(J, 1))
		js_copy(J, 1);
	else if (js_isundefined(J, 1))
		js_newregexp(J, "", 0);
	else
		js_newregexp(J, js_tostring(J, 1), 0);

	re = js_toregexp(J, -1);
	if (!(re->flags & JS_REGEXP_G)) {
		js_RegExp_prototype_exec(J, re, text);
		return;
	}

	re->last = 0;

	js_newarray(J);

	len = 0;
	a = text;
	e = text + strlen(text);
	while (a <= e) {
		if (js_doregexec(J, re->prog, a, &m, isbol(re, text, a) ? 0 : REG_NOTBOL))
			break;

		b = m.sub[0].sp;
		c = m.sub[0].ep;

		js_pushlstring(J, b, c - b);
		js_setindex(J, -2, len++);

		a = c;
		if (c - b == 0)
			a += chartorune(&rune, a);
	}

	if (len == 0) {
		js_pop(J, 1);
		js_pushnull(J);
	}
}

static void Sp_search(js_State *J)
{
	js_Regexp *re;
	const char *text;
	Resub m;

	text = checkstring(J, 0);

	if (js_isregexp(J, 1))
		js_copy(J, 1);
	else if (js_isundefined(J, 1))
		js_newregexp(J, "", 0);
	else
		js_newregexp(J, js_tostring(J, 1), 0);

	re = js_toregexp(J, -1);

	if (!js_doregexec(J, re->prog, text, &m, 0))
		js_pushnumber(J, js_utfptrtoidx(text, m.sub[0].sp));
	else
		js_pushnumber(J, -1);
}

static void Sp_replace_regexp(js_State *J)
{
	js_Regexp *re;
	const char *source, *source0, *s, *r;
	js_Buffer *sb = NULL;
	int n, x;
	Resub m;

	source = source0 = checkstring(J, 0);
	re = js_toregexp(J, 1);

	if (js_doregexec(J, re->prog, source, &m, 0)) {
		js_copy(J, 0);
		return;
	}

	re->last = 0;

	if (js_try(J)) {
		js_free(J, sb);
		js_throw(J);
	}

loop:
	s = m.sub[0].sp;
	n = m.sub[0].ep - m.sub[0].sp;

	if (js_iscallable(J, 2)) {
		js_copy(J, 2);
		js_pushundefined(J);
		for (x = 0; m.sub[x].sp; ++x)
			js_pushlstring(J, m.sub[x].sp, m.sub[x].ep - m.sub[x].sp);
		js_pushnumber(J, s - source);
		js_copy(J, 0);
		js_call(J, 2 + x);
		r = js_tostring(J, -1);
		js_putm(J, &sb, source, s);
		js_puts(J, &sb, r);
		js_pop(J, 1);
	} else {
		r = js_tostring(J, 2);
		js_putm(J, &sb, source, s);
		while (*r) {
			if (*r == '$') {
				switch (*(++r)) {
				case 0: --r;

				case '$': js_putc(J, &sb, '$'); break;
				case '`': js_putm(J, &sb, source0, s); break;
				case '\'': js_puts(J, &sb, s + n); break;
				case '&':
					js_putm(J, &sb, s, s + n);
					break;
				case '0': case '1': case '2': case '3': case '4':
				case '5': case '6': case '7': case '8': case '9':
					x = *r - '0';
					if (r[1] >= '0' && r[1] <= '9')
						x = x * 10 + *(++r) - '0';
					if (x > 0 && x < m.nsub) {
						js_putm(J, &sb, m.sub[x].sp, m.sub[x].ep);
					} else {
						js_putc(J, &sb, '$');
						if (x > 10) {
							js_putc(J, &sb, '0' + x / 10);
							js_putc(J, &sb, '0' + x % 10);
						} else {
							js_putc(J, &sb, '0' + x);
						}
					}
					break;
				default:
					js_putc(J, &sb, '$');
					js_putc(J, &sb, *r);
					break;
				}
				++r;
			} else {
				js_putc(J, &sb, *r++);
			}
		}
	}

	if (re->flags & JS_REGEXP_G) {
		source = m.sub[0].ep;
		if (n == 0) {
			if (*source)
				js_putc(J, &sb, *source++);
			else
				goto end;
		}
		if (!js_doregexec(J, re->prog, source, &m, isbol(re, source0, source) ? 0 : REG_NOTBOL))
			goto loop;
	}

end:
	js_puts(J, &sb, s + n);
	js_putc(J, &sb, 0);

	js_pushstring(J, sb ? sb->s : "");
	js_endtry(J);
	js_free(J, sb);
}

static void Sp_replace_string(js_State *J)
{
	const char *source, *needle, *s, *r;
	js_Buffer *sb = NULL;
	int n;

	source = checkstring(J, 0);
	needle = js_tostring(J, 1);

	s = strstr(source, needle);
	if (!s) {
		js_copy(J, 0);
		return;
	}
	n = strlen(needle);

	if (js_try(J)) {
		js_free(J, sb);
		js_throw(J);
	}

	if (js_iscallable(J, 2)) {
		js_copy(J, 2);
		js_pushundefined(J);
		js_pushlstring(J, s, n);
		js_pushnumber(J, s - source);
		js_copy(J, 0);
		js_call(J, 3);
		r = js_tostring(J, -1);
		js_putm(J, &sb, source, s);
		js_puts(J, &sb, r);
		js_puts(J, &sb, s + n);
		js_putc(J, &sb, 0);
		js_pop(J, 1);
	} else {
		r = js_tostring(J, 2);
		js_putm(J, &sb, source, s);
		while (*r) {
			if (*r == '$') {
				switch (*(++r)) {
				case 0: --r;

				case '$': js_putc(J, &sb, '$'); break;
				case '&': js_putm(J, &sb, s, s + n); break;
				case '`': js_putm(J, &sb, source, s); break;
				case '\'': js_puts(J, &sb, s + n); break;
				default: js_putc(J, &sb, '$'); js_putc(J, &sb, *r); break;
				}
				++r;
			} else {
				js_putc(J, &sb, *r++);
			}
		}
		js_puts(J, &sb, s + n);
		js_putc(J, &sb, 0);
	}

	js_pushstring(J, sb ? sb->s : "");
	js_endtry(J);
	js_free(J, sb);
}

static void Sp_replace(js_State *J)
{
	if (js_isregexp(J, 1))
		Sp_replace_regexp(J);
	else
		Sp_replace_string(J);
}

static void Sp_split_regexp(js_State *J)
{
	js_Regexp *re;
	const char *text;
	int limit, len, k;
	const char *p, *a, *b, *c, *e;
	Resub m;
	Rune rune;

	text = checkstring(J, 0);
	re = js_toregexp(J, 1);
	limit = js_isdefined(J, 2) ? js_tointeger(J, 2) : 1 << 30;

	js_newarray(J);
	len = 0;

	if (limit == 0)
		return;

	e = text + strlen(text);

	if (e == text) {
		if (js_doregexec(J, re->prog, text, &m, 0)) {
			js_pushliteral(J, "");
			js_setindex(J, -2, 0);
		}
		return;
	}

	p = a = text;
	while (a < e) {
		if (js_doregexec(J, re->prog, a, &m, isbol(re, text, a) ? 0 : REG_NOTBOL))
			break;

		b = m.sub[0].sp;
		c = m.sub[0].ep;

		if (b == c && b == p) {
			a += chartorune(&rune, a);
			continue;
		}

		if (len == limit) return;
		js_pushlstring(J, p, b - p);
		js_setindex(J, -2, len++);

		for (k = 1; k < m.nsub; ++k) {
			if (len == limit) return;
			js_pushlstring(J, m.sub[k].sp, m.sub[k].ep - m.sub[k].sp);
			js_setindex(J, -2, len++);
		}

		a = p = c;
	}

	if (len == limit) return;
	js_pushstring(J, p);
	js_setindex(J, -2, len);
}

static void Sp_split_string(js_State *J)
{
	const char *str = checkstring(J, 0);
	const char *sep = js_tostring(J, 1);
	int limit = js_isdefined(J, 2) ? js_tointeger(J, 2) : 1 << 30;
	int i, n;

	js_newarray(J);

	if (limit == 0)
		return;

	n = strlen(sep);

	if (n == 0) {
		Rune rune;
		for (i = 0; *str && i < limit; ++i) {
			n = chartorune(&rune, str);
			js_pushlstring(J, str, n);
			js_setindex(J, -2, i);
			str += n;
		}
		return;
	}

	for (i = 0; str && i < limit; ++i) {
		const char *s = strstr(str, sep);
		if (s) {
			js_pushlstring(J, str, s-str);
			js_setindex(J, -2, i);
			str = s + n;
		} else {
			js_pushstring(J, str);
			js_setindex(J, -2, i);
			str = NULL;
		}
	}
}

static void Sp_split(js_State *J)
{
	if (js_isundefined(J, 1)) {
		js_newarray(J);
		js_pushstring(J, js_tostring(J, 0));
		js_setindex(J, -2, 0);
	} else if (js_isregexp(J, 1)) {
		Sp_split_regexp(J);
	} else {
		Sp_split_string(J);
	}
}

void jsB_initstring(js_State *J)
{
	J->String_prototype->u.s.shrstr[0] = 0;
	J->String_prototype->u.s.string = J->String_prototype->u.s.shrstr;
	J->String_prototype->u.s.length = 0;

	js_pushobject(J, J->String_prototype);
	{
		jsB_propf(J, "String.prototype.toString", Sp_toString, 0);
		jsB_propf(J, "String.prototype.valueOf", Sp_valueOf, 0);
		jsB_propf(J, "String.prototype.charAt", Sp_charAt, 1);
		jsB_propf(J, "String.prototype.charCodeAt", Sp_charCodeAt, 1);
		jsB_propf(J, "String.prototype.concat", Sp_concat, 0);
		jsB_propf(J, "String.prototype.indexOf", Sp_indexOf, 1);
		jsB_propf(J, "String.prototype.lastIndexOf", Sp_lastIndexOf, 1);
		jsB_propf(J, "String.prototype.localeCompare", Sp_localeCompare, 1);
		jsB_propf(J, "String.prototype.match", Sp_match, 1);
		jsB_propf(J, "String.prototype.replace", Sp_replace, 2);
		jsB_propf(J, "String.prototype.search", Sp_search, 1);
		jsB_propf(J, "String.prototype.slice", Sp_slice, 2);
		jsB_propf(J, "String.prototype.split", Sp_split, 2);
		jsB_propf(J, "String.prototype.substring", Sp_substring, 2);
		jsB_propf(J, "String.prototype.toLowerCase", Sp_toLowerCase, 0);
		jsB_propf(J, "String.prototype.toLocaleLowerCase", Sp_toLowerCase, 0);
		jsB_propf(J, "String.prototype.toUpperCase", Sp_toUpperCase, 0);
		jsB_propf(J, "String.prototype.toLocaleUpperCase", Sp_toUpperCase, 0);

		jsB_propf(J, "String.prototype.trim", Sp_trim, 0);
	}
	js_newcconstructor(J, jsB_String, jsB_new_String, "String", 0);
	{
		jsB_propf(J, "String.fromCharCode", S_fromCharCode, 0);
	}
	js_defglobal(J, "String", JS_DONTENUM);
}

#define JSV_ISSTRING(v) (v->t.type==JS_TSHRSTR || v->t.type==JS_TMEMSTR || v->t.type==JS_TLITSTR)
#define JSV_TOSTRING(v) (v->t.type==JS_TSHRSTR ? v->u.shrstr : v->t.type==JS_TLITSTR ? v->u.litstr : v->t.type==JS_TMEMSTR ? v->u.memstr->p : "")

double js_strtol(const char *s, char **p, int base)
{

	static const unsigned char table[256] = {
		80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
		80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
		80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 80, 80, 80, 80, 80, 80,
		80, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
		25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 80, 80, 80, 80, 80,
		80, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
		25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 80, 80, 80, 80, 80,
		80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
		80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
		80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
		80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
		80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
		80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
		80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
		80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80
	};
	double x;
	unsigned char c;
	if (base == 10)
		for (x = 0, c = *s++; (0 <= c - '0') && (c - '0' < 10); c = *s++)
			x = x * 10 + (c - '0');
	else
		for (x = 0, c = *s++; table[c] < base; c = *s++)
			x = x * base + table[c];
	if (p)
		*p = (char*)s-1;
	return x;
}

int jsV_numbertointeger(double n)
{
	if (n == 0) return 0;
	if (isnan(n)) return 0;
	n = (n < 0) ? -floor(-n) : floor(n);
	if (n < INT_MIN) return INT_MIN;
	if (n > INT_MAX) return INT_MAX;
	return (int)n;
}

int jsV_numbertoint32(double n)
{
	double two32 = 4294967296.0;
	double two31 = 2147483648.0;

	if (!isfinite(n) || n == 0)
		return 0;

	n = fmod(n, two32);
	n = n >= 0 ? floor(n) : ceil(n) + two32;
	if (n >= two31)
		return n - two32;
	else
		return n;
}

unsigned int jsV_numbertouint32(double n)
{
	return (unsigned int)jsV_numbertoint32(n);
}

short jsV_numbertoint16(double n)
{
	return jsV_numbertoint32(n);
}

unsigned short jsV_numbertouint16(double n)
{
	return jsV_numbertoint32(n);
}

static int jsV_toString(js_State *J, js_Object *obj)
{
	js_pushobject(J, obj);
	js_getproperty(J, -1, "toString");
	if (js_iscallable(J, -1)) {
		js_rot2(J);
		js_call(J, 0);
		if (js_isprimitive(J, -1))
			return 1;
		js_pop(J, 1);
		return 0;
	}
	js_pop(J, 2);
	return 0;
}

static int jsV_valueOf(js_State *J, js_Object *obj)
{
	js_pushobject(J, obj);
	js_getproperty(J, -1, "valueOf");
	if (js_iscallable(J, -1)) {
		js_rot2(J);
		js_call(J, 0);
		if (js_isprimitive(J, -1))
			return 1;
		js_pop(J, 1);
		return 0;
	}
	js_pop(J, 2);
	return 0;
}

void jsV_toprimitive(js_State *J, js_Value *v, int preferred)
{
	js_Object *obj;

	if (v->t.type != JS_TOBJECT)
		return;

	obj = v->u.object;

	if (preferred == JS_HNONE)
		preferred = obj->type == JS_CDATE ? JS_HSTRING : JS_HNUMBER;

	if (preferred == JS_HSTRING) {
		if (jsV_toString(J, obj) || jsV_valueOf(J, obj)) {
			*v = *js_tovalue(J, -1);
			js_pop(J, 1);
			return;
		}
	} else {
		if (jsV_valueOf(J, obj) || jsV_toString(J, obj)) {
			*v = *js_tovalue(J, -1);
			js_pop(J, 1);
			return;
		}
	}

	if (J->strict)
		js_typeerror(J, "cannot convert object to primitive");

	v->t.type = JS_TLITSTR;
	v->u.litstr = "[object]";
	return;
}

int jsV_toboolean(js_State *J, js_Value *v)
{
	switch (v->t.type) {
	default:
	case JS_TSHRSTR: return v->u.shrstr[0] != 0;
	case JS_TUNDEFINED: return 0;
	case JS_TNULL: return 0;
	case JS_TBOOLEAN: return v->u.boolean;
	case JS_TNUMBER: return v->u.number != 0 && !isnan(v->u.number);
	case JS_TLITSTR: return v->u.litstr[0] != 0;
	case JS_TMEMSTR: return v->u.memstr->p[0] != 0;
	case JS_TOBJECT: return 1;
	}
}

const char *js_itoa(char *out, int v)
{
	char buf[32], *s = out;
	unsigned int a;
	int i = 0;
	if (v < 0) {
		a = -(unsigned)v;
		*s++ = '-';
	} else {
		a = v;
	}
	while (a) {
		buf[i++] = (a % 10) + '0';
		a /= 10;
	}
	if (i == 0)
		buf[i++] = '0';
	while (i > 0)
		*s++ = buf[--i];
	*s = 0;
	return out;
}

double js_stringtofloat(const char *s, char **ep)
{
	char *end;
	double n;
	const char *e = s;
	int isflt = 0;
	if (*e == '+' || *e == '-') ++e;
	while (*e >= '0' && *e <= '9') ++e;
	if (*e == '.') { ++e; isflt = 1; }
	while (*e >= '0' && *e <= '9') ++e;
	if (*e == 'e' || *e == 'E') {
		++e;
		if (*e == '+' || *e == '-') ++e;
		while (*e >= '0' && *e <= '9') ++e;
		isflt = 1;
	}
	if (isflt)
		n = js_strtod(s, &end);
	else {

		if (*s == '-')
			n = -js_strtol(s+1, &end, 10);
		else if (*s == '+')
			n = js_strtol(s+1, &end, 10);
		else
			n = js_strtol(s, &end, 10);
	}
	if (end == e) {
		*ep = (char*)e;
		return n;
	}
	*ep = (char*)s;
	return 0;
}

double jsV_stringtonumber(js_State *J, const char *s)
{
	char *e;
	double n;
	while (jsY_iswhite(*s) || jsY_isnewline(*s)) ++s;
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && s[2] != 0)
		n = js_strtol(s + 2, &e, 16);
	else if (!strncmp(s, "Infinity", 8))
		n = INFINITY, e = (char*)s + 8;
	else if (!strncmp(s, "+Infinity", 9))
		n = INFINITY, e = (char*)s + 9;
	else if (!strncmp(s, "-Infinity", 9))
		n = -INFINITY, e = (char*)s + 9;
	else
		n = js_stringtofloat(s, &e);
	while (jsY_iswhite(*e) || jsY_isnewline(*e)) ++e;
	if (*e) return NAN;
	return n;
}

double jsV_tonumber(js_State *J, js_Value *v)
{
	switch (v->t.type) {
	default:
	case JS_TSHRSTR: return jsV_stringtonumber(J, v->u.shrstr);
	case JS_TUNDEFINED: return NAN;
	case JS_TNULL: return 0;
	case JS_TBOOLEAN: return v->u.boolean;
	case JS_TNUMBER: return v->u.number;
	case JS_TLITSTR: return jsV_stringtonumber(J, v->u.litstr);
	case JS_TMEMSTR: return jsV_stringtonumber(J, v->u.memstr->p);
	case JS_TOBJECT:
		jsV_toprimitive(J, v, JS_HNUMBER);
		return jsV_tonumber(J, v);
	}
}

double jsV_tointeger(js_State *J, js_Value *v)
{
	return jsV_numbertointeger(jsV_tonumber(J, v));
}

const char *jsV_numbertostring(js_State *J, char buf[32], double f)
{
	char digits[32], *p = buf, *s = digits;
	int exp, ndigits, point;

	if (f == 0) return "0";
	if (isnan(f)) return "NaN";
	if (isinf(f)) return f < 0 ? "-Infinity" : "Infinity";

	if (f >= INT_MIN && f <= INT_MAX) {
		int i = (int)f;
		if ((double)i == f)
			return js_itoa(buf, i);
	}

	ndigits = js_grisu2(f, digits, &exp);
	point = ndigits + exp;

	if (signbit(f))
		*p++ = '-';

	if (point < -5 || point > 21) {
		*p++ = *s++;
		if (ndigits > 1) {
			int n = ndigits - 1;
			*p++ = '.';
			while (n--)
				*p++ = *s++;
		}
		js_fmtexp(p, point - 1);
	}

	else if (point <= 0) {
		*p++ = '0';
		*p++ = '.';
		while (point++ < 0)
			*p++ = '0';
		while (ndigits-- > 0)
			*p++ = *s++;
		*p = 0;
	}

	else {
		while (ndigits-- > 0) {
			*p++ = *s++;
			if (--point == 0 && ndigits > 0)
				*p++ = '.';
		}
		while (point-- > 0)
			*p++ = '0';
		*p = 0;
	}

	return buf;
}

const char *jsV_tostring(js_State *J, js_Value *v)
{
	char buf[32];
	const char *p;
	switch (v->t.type) {
	default:
	case JS_TSHRSTR: return v->u.shrstr;
	case JS_TUNDEFINED: return "undefined";
	case JS_TNULL: return "null";
	case JS_TBOOLEAN: return v->u.boolean ? "true" : "false";
	case JS_TLITSTR: return v->u.litstr;
	case JS_TMEMSTR: return v->u.memstr->p;
	case JS_TNUMBER:
		p = jsV_numbertostring(J, buf, v->u.number);
		if (p == buf) {
			int n = strlen(p);
			if (n <= soffsetof(js_Value, t.type)) {
				char *s = v->u.shrstr;
				while (n--) *s++ = *p++;
				*s = 0;
				v->t.type = JS_TSHRSTR;
				return v->u.shrstr;
			} else {
				v->u.memstr = jsV_newmemstring(J, p, n);
				v->t.type = JS_TMEMSTR;
				return v->u.memstr->p;
			}
		}
		return p;
	case JS_TOBJECT:
		jsV_toprimitive(J, v, JS_HSTRING);
		return jsV_tostring(J, v);
	}
}

static js_Object *jsV_newboolean(js_State *J, int v)
{
	js_Object *obj = jsV_newobject(J, JS_CBOOLEAN, J->Boolean_prototype);
	obj->u.boolean = v;
	return obj;
}

static js_Object *jsV_newnumber(js_State *J, double v)
{
	js_Object *obj = jsV_newobject(J, JS_CNUMBER, J->Number_prototype);
	obj->u.number = v;
	return obj;
}

static js_Object *jsV_newstring(js_State *J, const char *v)
{
	js_Object *obj = jsV_newobject(J, JS_CSTRING, J->String_prototype);
	size_t n = strlen(v);
	if (n < sizeof(obj->u.s.shrstr)) {
		obj->u.s.string = obj->u.s.shrstr;
		memcpy(obj->u.s.shrstr, v, n + 1);
	} else {
		obj->u.s.string = js_strdup(J, v);
	}
	obj->u.s.length = js_utflen(v);
	return obj;
}

js_Object *jsV_toobject(js_State *J, js_Value *v)
{
	js_Object *o;
	switch (v->t.type) {
	default:
	case JS_TUNDEFINED: js_typeerror(J, "cannot convert undefined to object");
	case JS_TNULL: js_typeerror(J, "cannot convert null to object");
	case JS_TOBJECT: return v->u.object;
	case JS_TSHRSTR: o = jsV_newstring(J, v->u.shrstr); break;
	case JS_TLITSTR: o = jsV_newstring(J, v->u.litstr); break;
	case JS_TMEMSTR: o = jsV_newstring(J, v->u.memstr->p); break;
	case JS_TBOOLEAN: o = jsV_newboolean(J, v->u.boolean); break;
	case JS_TNUMBER: o = jsV_newnumber(J, v->u.number); break;
	}
	v->t.type = JS_TOBJECT;
	v->u.object = o;
	return o;
}

void js_newobjectx(js_State *J)
{
	js_Object *prototype = NULL;
	if (js_isobject(J, -1))
		prototype = js_toobject(J, -1);
	js_pop(J, 1);
	js_pushobject(J, jsV_newobject(J, JS_COBJECT, prototype));
}

void js_newobject(js_State *J)
{
	js_pushobject(J, jsV_newobject(J, JS_COBJECT, J->Object_prototype));
}

void js_newarguments(js_State *J)
{
	js_pushobject(J, jsV_newobject(J, JS_CARGUMENTS, J->Object_prototype));
}

void js_newarray(js_State *J)
{
	js_Object *obj = jsV_newobject(J, JS_CARRAY, J->Array_prototype);
	obj->u.a.simple = 1;
	js_pushobject(J, obj);
}

void js_newboolean(js_State *J, int v)
{
	js_pushobject(J, jsV_newboolean(J, v));
}

void js_newnumber(js_State *J, double v)
{
	js_pushobject(J, jsV_newnumber(J, v));
}

void js_newstring(js_State *J, const char *v)
{
	js_pushobject(J, jsV_newstring(J, v));
}

void js_newfunction(js_State *J, js_Function *fun, js_Environment *scope)
{
	js_Object *obj = jsV_newobject(J, JS_CFUNCTION, J->Function_prototype);
	obj->u.f.function = fun;
	obj->u.f.scope = scope;
	js_pushobject(J, obj);
	{
		js_pushnumber(J, fun->numparams);
		js_defproperty(J, -2, "length", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
		js_newobject(J);
		{
			js_copy(J, -2);
			js_defproperty(J, -2, "constructor", JS_DONTENUM);
		}
		js_defproperty(J, -2, "prototype", JS_DONTENUM | JS_DONTCONF);
	}
}

void js_newscript(js_State *J, js_Function *fun, js_Environment *scope)
{
	js_Object *obj = jsV_newobject(J, JS_CSCRIPT, NULL);
	obj->u.f.function = fun;
	obj->u.f.scope = scope;
	js_pushobject(J, obj);
}

void js_newcfunctionx(js_State *J, js_CFunction cfun, const char *name, int length, void *data, js_Finalize finalize)
{
	js_Object *obj;

	if (js_try(J)) {
		if (finalize)
			finalize(J, data);
		js_throw(J);
	}

	obj = jsV_newobject(J, JS_CCFUNCTION, J->Function_prototype);
	obj->u.c.name = name;
	obj->u.c.function = cfun;
	obj->u.c.constructor = NULL;
	obj->u.c.length = length;
	obj->u.c.data = data;
	obj->u.c.finalize = finalize;

	js_endtry(J);

	js_pushobject(J, obj);
	{
		js_pushnumber(J, length);
		js_defproperty(J, -2, "length", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
		js_newobject(J);
		{
			js_copy(J, -2);
			js_defproperty(J, -2, "constructor", JS_DONTENUM);
		}
		js_defproperty(J, -2, "prototype", JS_DONTENUM | JS_DONTCONF);
	}
}

void js_newcfunction(js_State *J, js_CFunction cfun, const char *name, int length)
{
	js_newcfunctionx(J, cfun, name, length, NULL, NULL);
}

void js_newcconstructor(js_State *J, js_CFunction cfun, js_CFunction ccon, const char *name, int length)
{
	js_Object *obj = jsV_newobject(J, JS_CCFUNCTION, J->Function_prototype);
	obj->u.c.name = name;
	obj->u.c.function = cfun;
	obj->u.c.constructor = ccon;
	obj->u.c.length = length;
	js_pushobject(J, obj);
	{
		js_pushnumber(J, length);
		js_defproperty(J, -2, "length", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
		js_rot2(J);
		js_copy(J, -2);
		js_defproperty(J, -2, "constructor", JS_DONTENUM);
		js_defproperty(J, -2, "prototype", JS_DONTENUM | JS_DONTCONF);
	}
}

void js_newuserdatax(js_State *J, const char *tag, void *data, js_HasProperty has, js_Put put, js_Delete delete, js_Finalize finalize)
{
	js_Object *prototype = NULL;
	js_Object *obj;

	if (js_isobject(J, -1))
		prototype = js_toobject(J, -1);
	js_pop(J, 1);

	if (js_try(J)) {
		if (finalize)
			finalize(J, data);
		js_throw(J);
	}

	obj = jsV_newobject(J, JS_CUSERDATA, prototype);
	obj->u.user.tag = tag;
	obj->u.user.data = data;
	obj->u.user.has = has;
	obj->u.user.put = put;
	obj->u.user.delete = delete;
	obj->u.user.finalize = finalize;

	js_endtry(J);

	js_pushobject(J, obj);
}

void js_newuserdata(js_State *J, const char *tag, void *data, js_Finalize finalize)
{
	js_newuserdatax(J, tag, data, NULL, NULL, NULL, finalize);
}

int js_instanceof(js_State *J)
{
	js_Object *O, *V;

	if (!js_iscallable(J, -1))
		js_typeerror(J, "instanceof: invalid operand");

	if (!js_isobject(J, -2))
		return 0;

	js_getproperty(J, -1, "prototype");
	if (!js_isobject(J, -1))
		js_typeerror(J, "instanceof: 'prototype' property is not an object");
	O = js_toobject(J, -1);
	js_pop(J, 1);

	V = js_toobject(J, -2);
	while (V) {
		V = V->prototype;
		if (O == V)
			return 1;
	}

	return 0;
}

void js_concat(js_State *J)
{
	js_toprimitive(J, -2, JS_HNONE);
	js_toprimitive(J, -1, JS_HNONE);

	if (js_isstring(J, -2) || js_isstring(J, -1)) {
		const char *sa = js_tostring(J, -2);
		const char *sb = js_tostring(J, -1);
		char * volatile sab = NULL;

		if (js_try(J)) {
			js_free(J, sab);
			js_throw(J);
		}
		sab = js_malloc(J, strlen(sa) + strlen(sb) + 1);
		strcpy(sab, sa);
		strcat(sab, sb);
		js_pop(J, 2);
		js_pushstring(J, sab);
		js_endtry(J);
		js_free(J, sab);
	} else {
		double x = js_tonumber(J, -2);
		double y = js_tonumber(J, -1);
		js_pop(J, 2);
		js_pushnumber(J, x + y);
	}
}

int js_compare(js_State *J, int *okay)
{
	js_toprimitive(J, -2, JS_HNUMBER);
	js_toprimitive(J, -1, JS_HNUMBER);

	*okay = 1;
	if (js_isstring(J, -2) && js_isstring(J, -1)) {
		return strcmp(js_tostring(J, -2), js_tostring(J, -1));
	} else {
		double x = js_tonumber(J, -2);
		double y = js_tonumber(J, -1);
		if (isnan(x) || isnan(y))
			*okay = 0;
		return x < y ? -1 : x > y ? 1 : 0;
	}
}

int js_equal(js_State *J)
{
	js_Value *x = js_tovalue(J, -2);
	js_Value *y = js_tovalue(J, -1);

retry:
	if (JSV_ISSTRING(x) && JSV_ISSTRING(y))
		return !strcmp(JSV_TOSTRING(x), JSV_TOSTRING(y));
	if (x->t.type == y->t.type) {
		if (x->t.type == JS_TUNDEFINED) return 1;
		if (x->t.type == JS_TNULL) return 1;
		if (x->t.type == JS_TNUMBER) return x->u.number == y->u.number;
		if (x->t.type == JS_TBOOLEAN) return x->u.boolean == y->u.boolean;
		if (x->t.type == JS_TOBJECT) return x->u.object == y->u.object;
		return 0;
	}

	if (x->t.type == JS_TNULL && y->t.type == JS_TUNDEFINED) return 1;
	if (x->t.type == JS_TUNDEFINED && y->t.type == JS_TNULL) return 1;

	if (x->t.type == JS_TNUMBER && JSV_ISSTRING(y))
		return x->u.number == jsV_tonumber(J, y);
	if (JSV_ISSTRING(x) && y->t.type == JS_TNUMBER)
		return jsV_tonumber(J, x) == y->u.number;

	if (x->t.type == JS_TBOOLEAN) {
		x->t.type = JS_TNUMBER;
		x->u.number = x->u.boolean ? 1 : 0;
		goto retry;
	}
	if (y->t.type == JS_TBOOLEAN) {
		y->t.type = JS_TNUMBER;
		y->u.number = y->u.boolean ? 1 : 0;
		goto retry;
	}
	if ((JSV_ISSTRING(x) || x->t.type == JS_TNUMBER) && y->t.type == JS_TOBJECT) {
		jsV_toprimitive(J, y, JS_HNONE);
		goto retry;
	}
	if (x->t.type == JS_TOBJECT && (JSV_ISSTRING(y) || y->t.type == JS_TNUMBER)) {
		jsV_toprimitive(J, x, JS_HNONE);
		goto retry;
	}

	return 0;
}

int js_strictequal(js_State *J)
{
	js_Value *x = js_tovalue(J, -2);
	js_Value *y = js_tovalue(J, -1);

	if (JSV_ISSTRING(x) && JSV_ISSTRING(y))
		return !strcmp(JSV_TOSTRING(x), JSV_TOSTRING(y));

	if (x->t.type != y->t.type) return 0;
	if (x->t.type == JS_TUNDEFINED) return 1;
	if (x->t.type == JS_TNULL) return 1;
	if (x->t.type == JS_TNUMBER) return x->u.number == y->u.number;
	if (x->t.type == JS_TBOOLEAN) return x->u.boolean == y->u.boolean;
	if (x->t.type == JS_TOBJECT) return x->u.object == y->u.object;
	return 0;
}

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <limits.h>

#define emit regemit
#define next regnext
#define accept regaccept

#define nelem(a) (int)(sizeof (a) / sizeof (a)[0])

#define REPINF 255
#ifndef REG_MAXPROG
#define REG_MAXPROG (32 << 10)
#endif
#ifndef REG_MAXREC
#define REG_MAXREC 4096
#endif
#ifndef REG_MAXSPAN
#define REG_MAXSPAN 64
#endif
#ifndef REG_MAXCLASS
#define REG_MAXCLASS 128
#endif

typedef struct Reclass Reclass;
typedef struct Renode Renode;
typedef struct Reinst Reinst;
typedef struct Rethread Rethread;

struct Reclass {
	Rune *end;
	Rune spans[REG_MAXSPAN];
};

struct Reprog {
	Reinst *start, *end;
	Reclass *cclass;
	int flags;
	int nsub;
};

struct cstate {
	Reprog *prog;
	Renode *pstart, *pend;

	const char *source;
	int ncclass;
	int nsub;
	Renode *sub[REG_MAXSUB];

	int lookahead;
	Rune yychar;
	Reclass *yycc;
	int yymin, yymax;

	const char *error;
	jmp_buf kaboom;

	Reclass cclass[REG_MAXCLASS];
};

static void die(struct cstate *g, const char *message)
{
	g->error = message;
	longjmp(g->kaboom, 1);
}

static int canon(Rune c)
{
	Rune u = toupperrune(c);
	if (c >= 128 && u < 128)
		return c;
	return u;
}

enum {
	L_CHAR = 256,
	L_CCLASS,
	L_NCCLASS,
	L_NC,
	L_PLA,
	L_NLA,
	L_WORD,
	L_NWORD,
	L_REF,
	L_COUNT,
};

static int hex(struct cstate *g, int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 0xA;
	if (c >= 'A' && c <= 'F') return c - 'A' + 0xA;
	die(g, "invalid escape sequence");
	return 0;
}

static int dec(struct cstate *g, int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	die(g, "invalid quantifier");
	return 0;
}

#define ESCAPES "BbDdSsWw^$\\.*+?()[]{}|-0123456789"

static int isunicodeletter(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || isalpharune(c);
}

static int nextrune(struct cstate *g)
{
	if (!*g->source) {
		g->yychar = EOF;
		return 0;
	}
	g->source += chartorune(&g->yychar, g->source);
	if (g->yychar == '\\') {
		if (!*g->source)
			die(g, "unterminated escape sequence");
		g->source += chartorune(&g->yychar, g->source);
		switch (g->yychar) {
		case 'f': g->yychar = '\f'; return 0;
		case 'n': g->yychar = '\n'; return 0;
		case 'r': g->yychar = '\r'; return 0;
		case 't': g->yychar = '\t'; return 0;
		case 'v': g->yychar = '\v'; return 0;
		case 'c':
			if (!g->source[0])
				die(g, "unterminated escape sequence");
			g->yychar = (*g->source++) & 31;
			return 0;
		case 'x':
			if (!g->source[0] || !g->source[1])
				die(g, "unterminated escape sequence");
			g->yychar = hex(g, *g->source++) << 4;
			g->yychar += hex(g, *g->source++);
			if (g->yychar == 0) {
				g->yychar = '0';
				return 1;
			}
			return 1;
		case 'u':
			if (!g->source[0] || !g->source[1] || !g->source[2] || !g->source[3])
				die(g, "unterminated escape sequence");
			g->yychar = hex(g, *g->source++) << 12;
			g->yychar += hex(g, *g->source++) << 8;
			g->yychar += hex(g, *g->source++) << 4;
			g->yychar += hex(g, *g->source++);
			if (g->yychar == 0) {
				g->yychar = '0';
				return 1;
			}
			return 1;
		case 0:
			g->yychar = '0';
			return 1;
		}
		if (strchr(ESCAPES, g->yychar))
			return 1;
		if (isunicodeletter(g->yychar) || g->yychar == '_')
			die(g, "invalid escape character");
		return 0;
	}
	return 0;
}

static int lexcount(struct cstate *g)
{
	g->yychar = *g->source++;

	g->yymin = dec(g, g->yychar);
	g->yychar = *g->source++;
	while (g->yychar != ',' && g->yychar != '}') {
		g->yymin = g->yymin * 10 + dec(g, g->yychar);
		g->yychar = *g->source++;
		if (g->yymin >= REPINF)
			die(g, "numeric overflow");
	}

	if (g->yychar == ',') {
		g->yychar = *g->source++;
		if (g->yychar == '}') {
			g->yymax = REPINF;
		} else {
			g->yymax = dec(g, g->yychar);
			g->yychar = *g->source++;
			while (g->yychar != '}') {
				g->yymax = g->yymax * 10 + dec(g, g->yychar);
				g->yychar = *g->source++;
				if (g->yymax >= REPINF)
					die(g, "numeric overflow");
			}
		}
	} else {
		g->yymax = g->yymin;
	}

	return L_COUNT;
}

static void newcclass(struct cstate *g)
{
	if (g->ncclass >= REG_MAXCLASS)
		die(g, "too many character classes");
	g->yycc = g->cclass + g->ncclass++;
	g->yycc->end = g->yycc->spans;
}

static void addrange(struct cstate *g, Rune a, Rune b)
{
	Reclass *cc = g->yycc;
	Rune *p;

	if (a > b)
		die(g, "invalid character class range");

	for (p = cc->spans; p < cc->end; p += 2) {

		if (a >= p[0] && b <= p[1])
			return;

		if (a < p[0] && b >= p[1]) {
			p[0] = a;
			p[1] = b;
			return;
		}

		if (b >= p[0] - 1 && b <= p[1] && a < p[0]) {
			p[0] = a;
			return;
		}

		if (a >= p[0] && a <= p[1] + 1 && b > p[1]) {
			p[1] = b;
			return;
		}
	}

	if (cc->end + 2 >= cc->spans + nelem(cc->spans))
		die(g, "too many character class ranges");
	*cc->end++ = a;
	*cc->end++ = b;
}

static void addranges_d(struct cstate *g)
{
	addrange(g, '0', '9');
}

static void addranges_D(struct cstate *g)
{
	addrange(g, 0, '0'-1);
	addrange(g, '9'+1, 0xFFFF);
}

static void addranges_s(struct cstate *g)
{
	addrange(g, 0x9, 0xD);
	addrange(g, 0x20, 0x20);
	addrange(g, 0xA0, 0xA0);
	addrange(g, 0x2028, 0x2029);
	addrange(g, 0xFEFF, 0xFEFF);
}

static void addranges_S(struct cstate *g)
{
	addrange(g, 0, 0x9-1);
	addrange(g, 0xD+1, 0x20-1);
	addrange(g, 0x20+1, 0xA0-1);
	addrange(g, 0xA0+1, 0x2028-1);
	addrange(g, 0x2029+1, 0xFEFF-1);
	addrange(g, 0xFEFF+1, 0xFFFF);
}

static void addranges_w(struct cstate *g)
{
	addrange(g, '0', '9');
	addrange(g, 'A', 'Z');
	addrange(g, '_', '_');
	addrange(g, 'a', 'z');
}

static void addranges_W(struct cstate *g)
{
	addrange(g, 0, '0'-1);
	addrange(g, '9'+1, 'A'-1);
	addrange(g, 'Z'+1, '_'-1);
	addrange(g, '_'+1, 'a'-1);
	addrange(g, 'z'+1, 0xFFFF);
}

static int lexclass(struct cstate *g)
{
	int type = L_CCLASS;
	int quoted, havesave, havedash;
	Rune save = 0;

	newcclass(g);

	quoted = nextrune(g);
	if (!quoted && g->yychar == '^') {
		type = L_NCCLASS;
		quoted = nextrune(g);
	}

	havesave = havedash = 0;
	for (;;) {
		if (g->yychar == EOF)
			die(g, "unterminated character class");
		if (!quoted && g->yychar == ']')
			break;

		if (!quoted && g->yychar == '-') {
			if (havesave) {
				if (havedash) {
					addrange(g, save, '-');
					havesave = havedash = 0;
				} else {
					havedash = 1;
				}
			} else {
				save = '-';
				havesave = 1;
			}
		} else if (quoted && strchr("DSWdsw", g->yychar)) {
			if (havesave) {
				addrange(g, save, save);
				if (havedash)
					addrange(g, '-', '-');
			}
			switch (g->yychar) {
			case 'd': addranges_d(g); break;
			case 's': addranges_s(g); break;
			case 'w': addranges_w(g); break;
			case 'D': addranges_D(g); break;
			case 'S': addranges_S(g); break;
			case 'W': addranges_W(g); break;
			}
			havesave = havedash = 0;
		} else {
			if (quoted) {
				if (g->yychar == 'b')
					g->yychar = '\b';
				else if (g->yychar == '0')
					g->yychar = 0;

			}
			if (havesave) {
				if (havedash) {
					addrange(g, save, g->yychar);
					havesave = havedash = 0;
				} else {
					addrange(g, save, save);
					save = g->yychar;
				}
			} else {
				save = g->yychar;
				havesave = 1;
			}
		}

		quoted = nextrune(g);
	}

	if (havesave) {
		addrange(g, save, save);
		if (havedash)
			addrange(g, '-', '-');
	}

	return type;
}

static int lex(struct cstate *g)
{
	int quoted = nextrune(g);
	if (quoted) {
		switch (g->yychar) {
		case 'b': return L_WORD;
		case 'B': return L_NWORD;
		case 'd': newcclass(g); addranges_d(g); return L_CCLASS;
		case 's': newcclass(g); addranges_s(g); return L_CCLASS;
		case 'w': newcclass(g); addranges_w(g); return L_CCLASS;
		case 'D': newcclass(g); addranges_d(g); return L_NCCLASS;
		case 'S': newcclass(g); addranges_s(g); return L_NCCLASS;
		case 'W': newcclass(g); addranges_w(g); return L_NCCLASS;
		case '0': g->yychar = 0; return L_CHAR;
		}
		if (g->yychar >= '0' && g->yychar <= '9') {
			g->yychar -= '0';
			if (*g->source >= '0' && *g->source <= '9')
				g->yychar = g->yychar * 10 + *g->source++ - '0';
			return L_REF;
		}
		return L_CHAR;
	}

	switch (g->yychar) {
	case EOF:
	case '$': case ')': case '*': case '+':
	case '.': case '?': case '^': case '|':
		return g->yychar;
	}

	if (g->yychar == '{')
		return lexcount(g);
	if (g->yychar == '[')
		return lexclass(g);
	if (g->yychar == '(') {
		if (g->source[0] == '?') {
			if (g->source[1] == ':') {
				g->source += 2;
				return L_NC;
			}
			if (g->source[1] == '=') {
				g->source += 2;
				return L_PLA;
			}
			if (g->source[1] == '!') {
				g->source += 2;
				return L_NLA;
			}
		}
		return '(';
	}

	return L_CHAR;
}

enum {
	P_CAT, P_ALT, P_REP,
	P_BOL, P_EOL, P_WORD, P_NWORD,
	P_PAR, P_PLA, P_NLA,
	P_ANY, P_CHAR, P_CCLASS, P_NCCLASS,
	P_REF,
};

struct Renode {
	unsigned char type;
	unsigned char ng, m, n;
	Rune c;
	int cc;
	Renode *x;
	Renode *y;
};

static Renode *newnode(struct cstate *g, int type)
{
	Renode *node = g->pend++;
	node->type = type;
	node->cc = -1;
	node->c = 0;
	node->ng = 0;
	node->m = 0;
	node->n = 0;
	node->x = node->y = NULL;
	return node;
}

static int empty(Renode *node)
{
	if (!node) return 1;
	switch (node->type) {
	default: return 1;
	case P_CAT: return empty(node->x) && empty(node->y);
	case P_ALT: return empty(node->x) || empty(node->y);
	case P_REP: return empty(node->x) || node->m == 0;
	case P_PAR: return empty(node->x);
	case P_REF: return empty(node->x);
	case P_ANY: case P_CHAR: case P_CCLASS: case P_NCCLASS: return 0;
	}
}

static Renode *newrep(struct cstate *g, Renode *atom, int ng, int min, int max)
{
	Renode *rep = newnode(g, P_REP);
	if (max == REPINF && empty(atom))
		die(g, "infinite loop matching the empty string");
	rep->ng = ng;
	rep->m = min;
	rep->n = max;
	rep->x = atom;
	return rep;
}

static void next(struct cstate *g)
{
	g->lookahead = lex(g);
}

static int accept(struct cstate *g, int t)
{
	if (g->lookahead == t) {
		next(g);
		return 1;
	}
	return 0;
}

static Renode *parsealt(struct cstate *g);

static Renode *parseatom(struct cstate *g)
{
	Renode *atom;
	if (g->lookahead == L_CHAR) {
		atom = newnode(g, P_CHAR);
		atom->c = g->yychar;
		next(g);
		return atom;
	}
	if (g->lookahead == L_CCLASS) {
		atom = newnode(g, P_CCLASS);
		atom->cc = g->yycc - g->cclass;
		next(g);
		return atom;
	}
	if (g->lookahead == L_NCCLASS) {
		atom = newnode(g, P_NCCLASS);
		atom->cc = g->yycc - g->cclass;
		next(g);
		return atom;
	}
	if (g->lookahead == L_REF) {
		atom = newnode(g, P_REF);
		if (g->yychar == 0 || g->yychar >= g->nsub || !g->sub[g->yychar])
			die(g, "invalid back-reference");
		atom->n = g->yychar;
		atom->x = g->sub[g->yychar];
		next(g);
		return atom;
	}
	if (accept(g, '.'))
		return newnode(g, P_ANY);
	if (accept(g, '(')) {
		atom = newnode(g, P_PAR);
		if (g->nsub == REG_MAXSUB)
			die(g, "too many captures");
		atom->n = g->nsub++;
		atom->x = parsealt(g);
		g->sub[atom->n] = atom;
		if (!accept(g, ')'))
			die(g, "unmatched '('");
		return atom;
	}
	if (accept(g, L_NC)) {
		atom = parsealt(g);
		if (!accept(g, ')'))
			die(g, "unmatched '('");
		return atom;
	}
	if (accept(g, L_PLA)) {
		atom = newnode(g, P_PLA);
		atom->x = parsealt(g);
		if (!accept(g, ')'))
			die(g, "unmatched '('");
		return atom;
	}
	if (accept(g, L_NLA)) {
		atom = newnode(g, P_NLA);
		atom->x = parsealt(g);
		if (!accept(g, ')'))
			die(g, "unmatched '('");
		return atom;
	}
	die(g, "syntax error");
	return NULL;
}

static Renode *parserep(struct cstate *g)
{
	Renode *atom;

	if (accept(g, '^')) return newnode(g, P_BOL);
	if (accept(g, '$')) return newnode(g, P_EOL);
	if (accept(g, L_WORD)) return newnode(g, P_WORD);
	if (accept(g, L_NWORD)) return newnode(g, P_NWORD);

	atom = parseatom(g);
	if (g->lookahead == L_COUNT) {
		int min = g->yymin, max = g->yymax;
		next(g);
		if (max < min)
			die(g, "invalid quantifier");
		return newrep(g, atom, accept(g, '?'), min, max);
	}
	if (accept(g, '*')) return newrep(g, atom, accept(g, '?'), 0, REPINF);
	if (accept(g, '+')) return newrep(g, atom, accept(g, '?'), 1, REPINF);
	if (accept(g, '?')) return newrep(g, atom, accept(g, '?'), 0, 1);
	return atom;
}

static Renode *parsecat(struct cstate *g)
{
	Renode *cat, *head, **tail;
	if (g->lookahead != EOF && g->lookahead != '|' && g->lookahead != ')') {

		head = parserep(g);
		tail = &head;
		while (g->lookahead != EOF && g->lookahead != '|' && g->lookahead != ')') {
			cat = newnode(g, P_CAT);
			cat->x = *tail;
			cat->y = parserep(g);
			*tail = cat;
			tail = &cat->y;
		}
		return head;
	}
	return NULL;
}

static Renode *parsealt(struct cstate *g)
{
	Renode *alt, *x;
	alt = parsecat(g);
	while (accept(g, '|')) {
		x = alt;
		alt = newnode(g, P_ALT);
		alt->x = x;
		alt->y = parsecat(g);
	}
	return alt;
}

enum {
	I_END, I_JUMP, I_SPLIT, I_PLA, I_NLA,
	I_ANYNL, I_ANY, I_CHAR, I_CCLASS, I_NCCLASS, I_REF,
	I_BOL, I_EOL, I_WORD, I_NWORD,
	I_LPAR, I_RPAR
};

struct Reinst {
	unsigned char opcode;
	unsigned char n;
	Rune c;
	Reclass *cc;
	Reinst *x;
	Reinst *y;
};

static int count(struct cstate *g, Renode *node, int depth)
{
	int min, max, n;
	if (!node) return 0;
	if (++depth > REG_MAXREC) die(g, "stack overflow");
	switch (node->type) {
	default: return 1;
	case P_CAT: return count(g, node->x, depth) + count(g, node->y, depth);
	case P_ALT: return count(g, node->x, depth) + count(g, node->y, depth) + 2;
	case P_REP:
		min = node->m;
		max = node->n;
		if (min == max) n = count(g, node->x, depth) * min;
		else if (max < REPINF) n = count(g, node->x, depth) * max + (max - min);
		else n = count(g, node->x, depth) * (min + 1) + 2;
		if (n < 0 || n > REG_MAXPROG) die(g, "program too large");
		return n;
	case P_PAR: return count(g, node->x, depth) + 2;
	case P_PLA: return count(g, node->x, depth) + 2;
	case P_NLA: return count(g, node->x, depth) + 2;
	}
}

static Reinst *emit(Reprog *prog, int opcode)
{
	Reinst *inst = prog->end++;
	inst->opcode = opcode;
	inst->n = 0;
	inst->c = 0;
	inst->cc = NULL;
	inst->x = inst->y = NULL;
	return inst;
}

static void compile(Reprog *prog, Renode *node)
{
	Reinst *inst, *split, *jump;
	int i;

loop:
	if (!node)
		return;

	switch (node->type) {
	case P_CAT:
		compile(prog, node->x);
		node = node->y;
		goto loop;

	case P_ALT:
		split = emit(prog, I_SPLIT);
		compile(prog, node->x);
		jump = emit(prog, I_JUMP);
		compile(prog, node->y);
		split->x = split + 1;
		split->y = jump + 1;
		jump->x = prog->end;
		break;

	case P_REP:
		inst = NULL;
		for (i = 0; i < node->m; ++i) {
			inst = prog->end;
			compile(prog, node->x);
		}
		if (node->m == node->n)
			break;
		if (node->n < REPINF) {
			for (i = node->m; i < node->n; ++i) {
				split = emit(prog, I_SPLIT);
				compile(prog, node->x);
				if (node->ng) {
					split->y = split + 1;
					split->x = prog->end;
				} else {
					split->x = split + 1;
					split->y = prog->end;
				}
			}
		} else if (node->m == 0) {
			split = emit(prog, I_SPLIT);
			compile(prog, node->x);
			jump = emit(prog, I_JUMP);
			if (node->ng) {
				split->y = split + 1;
				split->x = prog->end;
			} else {
				split->x = split + 1;
				split->y = prog->end;
			}
			jump->x = split;
		} else {
			split = emit(prog, I_SPLIT);
			if (node->ng) {
				split->y = inst;
				split->x = prog->end;
			} else {
				split->x = inst;
				split->y = prog->end;
			}
		}
		break;

	case P_BOL: emit(prog, I_BOL); break;
	case P_EOL: emit(prog, I_EOL); break;
	case P_WORD: emit(prog, I_WORD); break;
	case P_NWORD: emit(prog, I_NWORD); break;

	case P_PAR:
		inst = emit(prog, I_LPAR);
		inst->n = node->n;
		compile(prog, node->x);
		inst = emit(prog, I_RPAR);
		inst->n = node->n;
		break;
	case P_PLA:
		split = emit(prog, I_PLA);
		compile(prog, node->x);
		emit(prog, I_END);
		split->x = split + 1;
		split->y = prog->end;
		break;
	case P_NLA:
		split = emit(prog, I_NLA);
		compile(prog, node->x);
		emit(prog, I_END);
		split->x = split + 1;
		split->y = prog->end;
		break;

	case P_ANY:
		emit(prog, I_ANY);
		break;
	case P_CHAR:
		inst = emit(prog, I_CHAR);
		inst->c = (prog->flags & REG_ICASE) ? canon(node->c) : node->c;
		break;
	case P_CCLASS:
		inst = emit(prog, I_CCLASS);
		inst->cc = prog->cclass + node->cc;
		break;
	case P_NCCLASS:
		inst = emit(prog, I_NCCLASS);
		inst->cc = prog->cclass + node->cc;
		break;
	case P_REF:
		inst = emit(prog, I_REF);
		inst->n = node->n;
		break;
	}
}

#ifdef TEST
static void dumpnode(struct cstate *g, Renode *node)
{
	Rune *p;
	Reclass *cc;
	if (!node) { printf("Empty"); return; }
	switch (node->type) {
	case P_CAT: printf("Cat("); dumpnode(g, node->x); printf(", "); dumpnode(g, node->y); printf(")"); break;
	case P_ALT: printf("Alt("); dumpnode(g, node->x); printf(", "); dumpnode(g, node->y); printf(")"); break;
	case P_REP:
		printf(node->ng ? "NgRep(%d,%d," : "Rep(%d,%d,", node->m, node->n);
		dumpnode(g, node->x);
		printf(")");
		break;
	case P_BOL: printf("Bol"); break;
	case P_EOL: printf("Eol"); break;
	case P_WORD: printf("Word"); break;
	case P_NWORD: printf("NotWord"); break;
	case P_PAR: printf("Par(%d,", node->n); dumpnode(g, node->x); printf(")"); break;
	case P_PLA: printf("PLA("); dumpnode(g, node->x); printf(")"); break;
	case P_NLA: printf("NLA("); dumpnode(g, node->x); printf(")"); break;
	case P_ANY: printf("Any"); break;
	case P_CHAR: printf("Char(%c)", node->c); break;
	case P_CCLASS:
		printf("Class(");
		cc = g->cclass + node->cc;
		for (p = cc->spans; p < cc->end; p += 2) printf("%02X-%02X,", p[0], p[1]);
		printf(")");
		break;
	case P_NCCLASS:
		printf("NotClass(");
		cc = g->cclass + node->cc;
		for (p = cc->spans; p < cc->end; p += 2) printf("%02X-%02X,", p[0], p[1]);
		printf(")");
		break;
	case P_REF: printf("Ref(%d)", node->n); break;
	}
}

static void dumpcclass(Reclass *cc) {
	Rune *p;
	for (p = cc->spans; p < cc->end; p += 2) {
		if (p[0] > 32 && p[0] < 127)
			printf(" %c", p[0]);
		else
			printf(" \\x%02x", p[0]);
		if (p[1] > 32 && p[1] < 127)
			printf("-%c", p[1]);
		else
			printf("-\\x%02x", p[1]);
	}
	putchar('\n');
}

static void dumpprog(Reprog *prog)
{
	Reinst *inst;
	int i;
	for (i = 0, inst = prog->start; inst < prog->end; ++i, ++inst) {
		printf("% 5d: ", i);
		switch (inst->opcode) {
		case I_END: puts("end"); break;
		case I_JUMP: printf("jump %d\n", (int)(inst->x - prog->start)); break;
		case I_SPLIT: printf("split %d %d\n", (int)(inst->x - prog->start), (int)(inst->y - prog->start)); break;
		case I_PLA: printf("pla %d %d\n", (int)(inst->x - prog->start), (int)(inst->y - prog->start)); break;
		case I_NLA: printf("nla %d %d\n", (int)(inst->x - prog->start), (int)(inst->y - prog->start)); break;
		case I_ANY: puts("any"); break;
		case I_ANYNL: puts("anynl"); break;
		case I_CHAR: printf(inst->c >= 32 && inst->c < 127 ? "char '%c'\n" : "char U+%04X\n", inst->c); break;
		case I_CCLASS: printf("cclass"); dumpcclass(inst->cc); break;
		case I_NCCLASS: printf("ncclass"); dumpcclass(inst->cc); break;
		case I_REF: printf("ref %d\n", inst->n); break;
		case I_BOL: puts("bol"); break;
		case I_EOL: puts("eol"); break;
		case I_WORD: puts("word"); break;
		case I_NWORD: puts("nword"); break;
		case I_LPAR: printf("lpar %d\n", inst->n); break;
		case I_RPAR: printf("rpar %d\n", inst->n); break;
		}
	}
}
#endif

Reprog *regcompx(void *(*alloc)(void *ctx, void *p, int n), void *ctx,
	const char *pattern, int cflags, const char **errorp)
{
	struct cstate g;
	Renode *node;
	Reinst *split, *jump;
	int i, n;

	g.pstart = NULL;
	g.prog = NULL;

	if (setjmp(g.kaboom)) {
		if (errorp) *errorp = g.error;
		alloc(ctx, g.pstart, 0);
		if (g.prog) {
			alloc(ctx, g.prog->cclass, 0);
			alloc(ctx, g.prog->start, 0);
			alloc(ctx, g.prog, 0);
		}
		return NULL;
	}

	g.prog = alloc(ctx, NULL, sizeof (Reprog));
	if (!g.prog)
		die(&g, "cannot allocate regular expression");
	g.prog->start = NULL;
	g.prog->cclass = NULL;

	n = strlen(pattern) * 2;
	if (n > REG_MAXPROG)
		die(&g, "program too large");
	if (n > 0) {
		g.pstart = g.pend = alloc(ctx, NULL, sizeof (Renode) * n);
		if (!g.pstart)
			die(&g, "cannot allocate regular expression parse list");
	}

	g.source = pattern;
	g.ncclass = 0;
	g.nsub = 1;
	for (i = 0; i < REG_MAXSUB; ++i)
		g.sub[i] = 0;

	g.prog->flags = cflags;

	next(&g);
	node = parsealt(&g);
	if (g.lookahead == ')')
		die(&g, "unmatched ')'");
	if (g.lookahead != EOF)
		die(&g, "syntax error");

#ifdef TEST
	dumpnode(&g, node);
	putchar('\n');
#endif

	n = 6 + count(&g, node, 0);
	if (n < 0 || n > REG_MAXPROG)
		die(&g, "program too large");

	g.prog->nsub = g.nsub;
	g.prog->start = g.prog->end = alloc(ctx, NULL, n * sizeof (Reinst));
	if (!g.prog->start)
		die(&g, "cannot allocate regular expression instruction list");

	if (g.ncclass > 0) {
		g.prog->cclass = alloc(ctx, NULL, g.ncclass * sizeof (Reclass));
		if (!g.prog->cclass)
			die(&g, "cannot allocate regular expression character class list");
		memcpy(g.prog->cclass, g.cclass, g.ncclass * sizeof (Reclass));
		for (i = 0; i < g.ncclass; ++i)
			g.prog->cclass[i].end = g.prog->cclass[i].spans + (g.cclass[i].end - g.cclass[i].spans);
	}

	split = emit(g.prog, I_SPLIT);
	split->x = split + 3;
	split->y = split + 1;
	emit(g.prog, I_ANYNL);
	jump = emit(g.prog, I_JUMP);
	jump->x = split;
	emit(g.prog, I_LPAR);
	compile(g.prog, node);
	emit(g.prog, I_RPAR);
	emit(g.prog, I_END);

#ifdef TEST
	dumpprog(g.prog);
#endif

	alloc(ctx, g.pstart, 0);

	if (errorp) *errorp = NULL;
	return g.prog;
}

void regfreex(void *(*alloc)(void *ctx, void *p, int n), void *ctx, Reprog *prog)
{
	if (prog) {
		if (prog->cclass)
			alloc(ctx, prog->cclass, 0);
		alloc(ctx, prog->start, 0);
		alloc(ctx, prog, 0);
	}
}

static void *default_alloc(void *ctx, void *p, int n)
{
	if (n == 0) {
		free(p);
		return NULL;
	}
	return realloc(p, (size_t)n);
}

Reprog *regcomp(const char *pattern, int cflags, const char **errorp)
{
	return regcompx(default_alloc, NULL, pattern, cflags, errorp);
}

void regfree(Reprog *prog)
{
	regfreex(default_alloc, NULL, prog);
}

static int isnewline(int c)
{
	return c == 0xA || c == 0xD || c == 0x2028 || c == 0x2029;
}

static int iswordchar(int c)
{
	return c == '_' ||
		(c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9');
}

static int incclass(Reclass *cc, Rune c)
{
	Rune *p;
	for (p = cc->spans; p < cc->end; p += 2)
		if (p[0] <= c && c <= p[1])
			return 1;
	return 0;
}

static int incclasscanon(Reclass *cc, Rune c)
{
	Rune *p, r;
	for (p = cc->spans; p < cc->end; p += 2)
		for (r = p[0]; r <= p[1]; ++r)
			if (c == canon(r))
				return 1;
	return 0;
}

static int strncmpcanon(const char *a, const char *b, int n)
{
	Rune ra, rb;
	int c;
	while (n--) {
		if (!*a) return -1;
		if (!*b) return 1;
		a += chartorune(&ra, a);
		b += chartorune(&rb, b);
		c = canon(ra) - canon(rb);
		if (c)
			return c;
	}
	return 0;
}

static int match(Reinst *pc, const char *sp, const char *bol, int flags, Resub *out, int depth)
{
	Resub scratch;
	int result;
	int i;
	Rune c;

	if (depth > REG_MAXREC)
		return -1;

	for (;;) {
		switch (pc->opcode) {
		case I_END:
			return 0;
		case I_JUMP:
			pc = pc->x;
			break;
		case I_SPLIT:
			scratch = *out;
			result = match(pc->x, sp, bol, flags, &scratch, depth+1);
			if (result == -1)
				return -1;
			if (result == 0) {
				*out = scratch;
				return 0;
			}
			pc = pc->y;
			break;

		case I_PLA:
			result = match(pc->x, sp, bol, flags, out, depth+1);
			if (result == -1)
				return -1;
			if (result == 1)
				return 1;
			pc = pc->y;
			break;
		case I_NLA:
			scratch = *out;
			result = match(pc->x, sp, bol, flags, &scratch, depth+1);
			if (result == -1)
				return -1;
			if (result == 0)
				return 1;
			pc = pc->y;
			break;

		case I_ANYNL:
			if (!*sp) return 1;
			sp += chartorune(&c, sp);
			pc = pc + 1;
			break;
		case I_ANY:
			if (!*sp) return 1;
			sp += chartorune(&c, sp);
			if (isnewline(c))
				return 1;
			pc = pc + 1;
			break;
		case I_CHAR:
			if (!*sp) return 1;
			sp += chartorune(&c, sp);
			if (flags & REG_ICASE)
				c = canon(c);
			if (c != pc->c)
				return 1;
			pc = pc + 1;
			break;
		case I_CCLASS:
			if (!*sp) return 1;
			sp += chartorune(&c, sp);
			if (flags & REG_ICASE) {
				if (!incclasscanon(pc->cc, canon(c)))
					return 1;
			} else {
				if (!incclass(pc->cc, c))
					return 1;
			}
			pc = pc + 1;
			break;
		case I_NCCLASS:
			if (!*sp) return 1;
			sp += chartorune(&c, sp);
			if (flags & REG_ICASE) {
				if (incclasscanon(pc->cc, canon(c)))
					return 1;
			} else {
				if (incclass(pc->cc, c))
					return 1;
			}
			pc = pc + 1;
			break;
		case I_REF:
			i = out->sub[pc->n].ep - out->sub[pc->n].sp;
			if (flags & REG_ICASE) {
				if (strncmpcanon(sp, out->sub[pc->n].sp, i))
					return 1;
			} else {
				if (strncmp(sp, out->sub[pc->n].sp, i))
					return 1;
			}
			if (i > 0)
				sp += i;
			pc = pc + 1;
			break;

		case I_BOL:
			if (sp == bol && !(flags & REG_NOTBOL)) {
				pc = pc + 1;
				break;
			}
			if (flags & REG_NEWLINE) {
				if (sp > bol && isnewline(sp[-1])) {
					pc = pc + 1;
					break;
				}
			}
			return 1;
		case I_EOL:
			if (*sp == 0) {
				pc = pc + 1;
				break;
			}
			if (flags & REG_NEWLINE) {
				if (isnewline(*sp)) {
					pc = pc + 1;
					break;
				}
			}
			return 1;
		case I_WORD:
			i = sp > bol && iswordchar(sp[-1]);
			i ^= iswordchar(sp[0]);
			if (!i)
				return 1;
			pc = pc + 1;
			break;
		case I_NWORD:
			i = sp > bol && iswordchar(sp[-1]);
			i ^= iswordchar(sp[0]);
			if (i)
				return 1;
			pc = pc + 1;
			break;

		case I_LPAR:
			out->sub[pc->n].sp = sp;
			pc = pc + 1;
			break;
		case I_RPAR:
			out->sub[pc->n].ep = sp;
			pc = pc + 1;
			break;
		default:
			return 1;
		}
	}
}

int regexec(Reprog *prog, const char *sp, Resub *sub, int eflags)
{
	Resub scratch;
	int i;

	if (!sub)
		sub = &scratch;

	sub->nsub = prog->nsub;
	for (i = 0; i < REG_MAXSUB; ++i)
		sub->sub[i].sp = sub->sub[i].ep = NULL;

	return match(prog->start, sp, sp, prog->flags | eflags, sub, 0);
}

#ifdef TEST
int main(int argc, char **argv)
{
	const char *error;
	const char *s;
	Reprog *p;
	Resub m;
	int i;

	if (argc > 1) {
		p = regcomp(argv[1], 0, &error);
		if (!p) {
			fprintf(stderr, "regcomp: %s\n", error);
			return 1;
		}

		if (argc > 2) {
			s = argv[2];
			printf("nsub = %d\n", p->nsub);
			if (!regexec(p, s, &m, 0)) {
				for (i = 0; i < m.nsub; ++i) {
					int n = m.sub[i].ep - m.sub[i].sp;
					if (n > 0)
						printf("match %d: s=%d e=%d n=%d '%.*s'\n", i, (int)(m.sub[i].sp - s), (int)(m.sub[i].ep - s), n, n, m.sub[i].sp);
					else
						printf("match %d: n=0 ''\n", i);
				}
			} else {
				printf("no match\n");
			}
		}
	}

	return 0;
}
#endif

#include <stdlib.h>
#include <string.h>

static const Rune ucd_alpha2[] = {
0x41,0x5a,
0x61,0x7a,
0xc0,0xd6,
0xd8,0xf6,
0xf8,0x2c1,
0x2c6,0x2d1,
0x2e0,0x2e4,
0x370,0x374,
0x376,0x377,
0x37a,0x37d,
0x388,0x38a,
0x38e,0x3a1,
0x3a3,0x3f5,
0x3f7,0x481,
0x48a,0x52f,
0x531,0x556,
0x560,0x588,
0x5d0,0x5ea,
0x5ef,0x5f2,
0x620,0x64a,
0x66e,0x66f,
0x671,0x6d3,
0x6e5,0x6e6,
0x6ee,0x6ef,
0x6fa,0x6fc,
0x712,0x72f,
0x74d,0x7a5,
0x7ca,0x7ea,
0x7f4,0x7f5,
0x800,0x815,
0x840,0x858,
0x860,0x86a,
0x870,0x887,
0x889,0x88e,
0x8a0,0x8c9,
0x904,0x939,
0x958,0x961,
0x971,0x980,
0x985,0x98c,
0x98f,0x990,
0x993,0x9a8,
0x9aa,0x9b0,
0x9b6,0x9b9,
0x9dc,0x9dd,
0x9df,0x9e1,
0x9f0,0x9f1,
0xa05,0xa0a,
0xa0f,0xa10,
0xa13,0xa28,
0xa2a,0xa30,
0xa32,0xa33,
0xa35,0xa36,
0xa38,0xa39,
0xa59,0xa5c,
0xa72,0xa74,
0xa85,0xa8d,
0xa8f,0xa91,
0xa93,0xaa8,
0xaaa,0xab0,
0xab2,0xab3,
0xab5,0xab9,
0xae0,0xae1,
0xb05,0xb0c,
0xb0f,0xb10,
0xb13,0xb28,
0xb2a,0xb30,
0xb32,0xb33,
0xb35,0xb39,
0xb5c,0xb5d,
0xb5f,0xb61,
0xb85,0xb8a,
0xb8e,0xb90,
0xb92,0xb95,
0xb99,0xb9a,
0xb9e,0xb9f,
0xba3,0xba4,
0xba8,0xbaa,
0xbae,0xbb9,
0xc05,0xc0c,
0xc0e,0xc10,
0xc12,0xc28,
0xc2a,0xc39,
0xc58,0xc5a,
0xc60,0xc61,
0xc85,0xc8c,
0xc8e,0xc90,
0xc92,0xca8,
0xcaa,0xcb3,
0xcb5,0xcb9,
0xcdd,0xcde,
0xce0,0xce1,
0xcf1,0xcf2,
0xd04,0xd0c,
0xd0e,0xd10,
0xd12,0xd3a,
0xd54,0xd56,
0xd5f,0xd61,
0xd7a,0xd7f,
0xd85,0xd96,
0xd9a,0xdb1,
0xdb3,0xdbb,
0xdc0,0xdc6,
0xe01,0xe30,
0xe32,0xe33,
0xe40,0xe46,
0xe81,0xe82,
0xe86,0xe8a,
0xe8c,0xea3,
0xea7,0xeb0,
0xeb2,0xeb3,
0xec0,0xec4,
0xedc,0xedf,
0xf40,0xf47,
0xf49,0xf6c,
0xf88,0xf8c,
0x1000,0x102a,
0x1050,0x1055,
0x105a,0x105d,
0x1065,0x1066,
0x106e,0x1070,
0x1075,0x1081,
0x10a0,0x10c5,
0x10d0,0x10fa,
0x10fc,0x1248,
0x124a,0x124d,
0x1250,0x1256,
0x125a,0x125d,
0x1260,0x1288,
0x128a,0x128d,
0x1290,0x12b0,
0x12b2,0x12b5,
0x12b8,0x12be,
0x12c2,0x12c5,
0x12c8,0x12d6,
0x12d8,0x1310,
0x1312,0x1315,
0x1318,0x135a,
0x1380,0x138f,
0x13a0,0x13f5,
0x13f8,0x13fd,
0x1401,0x166c,
0x166f,0x167f,
0x1681,0x169a,
0x16a0,0x16ea,
0x16f1,0x16f8,
0x1700,0x1711,
0x171f,0x1731,
0x1740,0x1751,
0x1760,0x176c,
0x176e,0x1770,
0x1780,0x17b3,
0x1820,0x1878,
0x1880,0x1884,
0x1887,0x18a8,
0x18b0,0x18f5,
0x1900,0x191e,
0x1950,0x196d,
0x1970,0x1974,
0x1980,0x19ab,
0x19b0,0x19c9,
0x1a00,0x1a16,
0x1a20,0x1a54,
0x1b05,0x1b33,
0x1b45,0x1b4c,
0x1b83,0x1ba0,
0x1bae,0x1baf,
0x1bba,0x1be5,
0x1c00,0x1c23,
0x1c4d,0x1c4f,
0x1c5a,0x1c7d,
0x1c80,0x1c8a,
0x1c90,0x1cba,
0x1cbd,0x1cbf,
0x1ce9,0x1cec,
0x1cee,0x1cf3,
0x1cf5,0x1cf6,
0x1d00,0x1dbf,
0x1e00,0x1f15,
0x1f18,0x1f1d,
0x1f20,0x1f45,
0x1f48,0x1f4d,
0x1f50,0x1f57,
0x1f5f,0x1f7d,
0x1f80,0x1fb4,
0x1fb6,0x1fbc,
0x1fc2,0x1fc4,
0x1fc6,0x1fcc,
0x1fd0,0x1fd3,
0x1fd6,0x1fdb,
0x1fe0,0x1fec,
0x1ff2,0x1ff4,
0x1ff6,0x1ffc,
0x2090,0x209c,
0x210a,0x2113,
0x2119,0x211d,
0x212a,0x212d,
0x212f,0x2139,
0x213c,0x213f,
0x2145,0x2149,
0x2183,0x2184,
0x2c00,0x2ce4,
0x2ceb,0x2cee,
0x2cf2,0x2cf3,
0x2d00,0x2d25,
0x2d30,0x2d67,
0x2d80,0x2d96,
0x2da0,0x2da6,
0x2da8,0x2dae,
0x2db0,0x2db6,
0x2db8,0x2dbe,
0x2dc0,0x2dc6,
0x2dc8,0x2dce,
0x2dd0,0x2dd6,
0x2dd8,0x2dde,
0x3005,0x3006,
0x3031,0x3035,
0x303b,0x303c,
0x3041,0x3096,
0x309d,0x309f,
0x30a1,0x30fa,
0x30fc,0x30ff,
0x3105,0x312f,
0x3131,0x318e,
0x31a0,0x31bf,
0x31f0,0x31ff,
0x9fff,0xa48c,
0xa4d0,0xa4fd,
0xa500,0xa60c,
0xa610,0xa61f,
0xa62a,0xa62b,
0xa640,0xa66e,
0xa67f,0xa69d,
0xa6a0,0xa6e5,
0xa717,0xa71f,
0xa722,0xa788,
0xa78b,0xa7cd,
0xa7d0,0xa7d1,
0xa7d5,0xa7dc,
0xa7f2,0xa801,
0xa803,0xa805,
0xa807,0xa80a,
0xa80c,0xa822,
0xa840,0xa873,
0xa882,0xa8b3,
0xa8f2,0xa8f7,
0xa8fd,0xa8fe,
0xa90a,0xa925,
0xa930,0xa946,
0xa960,0xa97c,
0xa984,0xa9b2,
0xa9e0,0xa9e4,
0xa9e6,0xa9ef,
0xa9fa,0xa9fe,
0xaa00,0xaa28,
0xaa40,0xaa42,
0xaa44,0xaa4b,
0xaa60,0xaa76,
0xaa7e,0xaaaf,
0xaab5,0xaab6,
0xaab9,0xaabd,
0xaadb,0xaadd,
0xaae0,0xaaea,
0xaaf2,0xaaf4,
0xab01,0xab06,
0xab09,0xab0e,
0xab11,0xab16,
0xab20,0xab26,
0xab28,0xab2e,
0xab30,0xab5a,
0xab5c,0xab69,
0xab70,0xabe2,
0xd7b0,0xd7c6,
0xd7cb,0xd7fb,
0xf900,0xfa6d,
0xfa70,0xfad9,
0xfb00,0xfb06,
0xfb13,0xfb17,
0xfb1f,0xfb28,
0xfb2a,0xfb36,
0xfb38,0xfb3c,
0xfb40,0xfb41,
0xfb43,0xfb44,
0xfb46,0xfbb1,
0xfbd3,0xfd3d,
0xfd50,0xfd8f,
0xfd92,0xfdc7,
0xfdf0,0xfdfb,
0xfe70,0xfe74,
0xfe76,0xfefc,
0xff21,0xff3a,
0xff41,0xff5a,
0xff66,0xffbe,
0xffc2,0xffc7,
0xffca,0xffcf,
0xffd2,0xffd7,
0xffda,0xffdc,
0x10000,0x1000b,
0x1000d,0x10026,
0x10028,0x1003a,
0x1003c,0x1003d,
0x1003f,0x1004d,
0x10050,0x1005d,
0x10080,0x100fa,
0x10280,0x1029c,
0x102a0,0x102d0,
0x10300,0x1031f,
0x1032d,0x10340,
0x10342,0x10349,
0x10350,0x10375,
0x10380,0x1039d,
0x103a0,0x103c3,
0x103c8,0x103cf,
0x10400,0x1049d,
0x104b0,0x104d3,
0x104d8,0x104fb,
0x10500,0x10527,
0x10530,0x10563,
0x10570,0x1057a,
0x1057c,0x1058a,
0x1058c,0x10592,
0x10594,0x10595,
0x10597,0x105a1,
0x105a3,0x105b1,
0x105b3,0x105b9,
0x105bb,0x105bc,
0x105c0,0x105f3,
0x10600,0x10736,
0x10740,0x10755,
0x10760,0x10767,
0x10780,0x10785,
0x10787,0x107b0,
0x107b2,0x107ba,
0x10800,0x10805,
0x1080a,0x10835,
0x10837,0x10838,
0x1083f,0x10855,
0x10860,0x10876,
0x10880,0x1089e,
0x108e0,0x108f2,
0x108f4,0x108f5,
0x10900,0x10915,
0x10920,0x10939,
0x10980,0x109b7,
0x109be,0x109bf,
0x10a10,0x10a13,
0x10a15,0x10a17,
0x10a19,0x10a35,
0x10a60,0x10a7c,
0x10a80,0x10a9c,
0x10ac0,0x10ac7,
0x10ac9,0x10ae4,
0x10b00,0x10b35,
0x10b40,0x10b55,
0x10b60,0x10b72,
0x10b80,0x10b91,
0x10c00,0x10c48,
0x10c80,0x10cb2,
0x10cc0,0x10cf2,
0x10d00,0x10d23,
0x10d4a,0x10d65,
0x10d6f,0x10d85,
0x10e80,0x10ea9,
0x10eb0,0x10eb1,
0x10ec2,0x10ec4,
0x10f00,0x10f1c,
0x10f30,0x10f45,
0x10f70,0x10f81,
0x10fb0,0x10fc4,
0x10fe0,0x10ff6,
0x11003,0x11037,
0x11071,0x11072,
0x11083,0x110af,
0x110d0,0x110e8,
0x11103,0x11126,
0x11150,0x11172,
0x11183,0x111b2,
0x111c1,0x111c4,
0x11200,0x11211,
0x11213,0x1122b,
0x1123f,0x11240,
0x11280,0x11286,
0x1128a,0x1128d,
0x1128f,0x1129d,
0x1129f,0x112a8,
0x112b0,0x112de,
0x11305,0x1130c,
0x1130f,0x11310,
0x11313,0x11328,
0x1132a,0x11330,
0x11332,0x11333,
0x11335,0x11339,
0x1135d,0x11361,
0x11380,0x11389,
0x11390,0x113b5,
0x11400,0x11434,
0x11447,0x1144a,
0x1145f,0x11461,
0x11480,0x114af,
0x114c4,0x114c5,
0x11580,0x115ae,
0x115d8,0x115db,
0x11600,0x1162f,
0x11680,0x116aa,
0x11700,0x1171a,
0x11740,0x11746,
0x11800,0x1182b,
0x118a0,0x118df,
0x118ff,0x11906,
0x1190c,0x11913,
0x11915,0x11916,
0x11918,0x1192f,
0x119a0,0x119a7,
0x119aa,0x119d0,
0x11a0b,0x11a32,
0x11a5c,0x11a89,
0x11ab0,0x11af8,
0x11bc0,0x11be0,
0x11c00,0x11c08,
0x11c0a,0x11c2e,
0x11c72,0x11c8f,
0x11d00,0x11d06,
0x11d08,0x11d09,
0x11d0b,0x11d30,
0x11d60,0x11d65,
0x11d67,0x11d68,
0x11d6a,0x11d89,
0x11ee0,0x11ef2,
0x11f04,0x11f10,
0x11f12,0x11f33,
0x12000,0x12399,
0x12480,0x12543,
0x12f90,0x12ff0,
0x13000,0x1342f,
0x13441,0x13446,
0x13460,0x143fa,
0x14400,0x14646,
0x16100,0x1611d,
0x16800,0x16a38,
0x16a40,0x16a5e,
0x16a70,0x16abe,
0x16ad0,0x16aed,
0x16b00,0x16b2f,
0x16b40,0x16b43,
0x16b63,0x16b77,
0x16b7d,0x16b8f,
0x16d40,0x16d6c,
0x16e40,0x16e7f,
0x16f00,0x16f4a,
0x16f93,0x16f9f,
0x16fe0,0x16fe1,
0x18800,0x18cd5,
0x18cff,0x18d00,
0x1aff0,0x1aff3,
0x1aff5,0x1affb,
0x1affd,0x1affe,
0x1b000,0x1b122,
0x1b150,0x1b152,
0x1b164,0x1b167,
0x1b170,0x1b2fb,
0x1bc00,0x1bc6a,
0x1bc70,0x1bc7c,
0x1bc80,0x1bc88,
0x1bc90,0x1bc99,
0x1d400,0x1d454,
0x1d456,0x1d49c,
0x1d49e,0x1d49f,
0x1d4a5,0x1d4a6,
0x1d4a9,0x1d4ac,
0x1d4ae,0x1d4b9,
0x1d4bd,0x1d4c3,
0x1d4c5,0x1d505,
0x1d507,0x1d50a,
0x1d50d,0x1d514,
0x1d516,0x1d51c,
0x1d51e,0x1d539,
0x1d53b,0x1d53e,
0x1d540,0x1d544,
0x1d54a,0x1d550,
0x1d552,0x1d6a5,
0x1d6a8,0x1d6c0,
0x1d6c2,0x1d6da,
0x1d6dc,0x1d6fa,
0x1d6fc,0x1d714,
0x1d716,0x1d734,
0x1d736,0x1d74e,
0x1d750,0x1d76e,
0x1d770,0x1d788,
0x1d78a,0x1d7a8,
0x1d7aa,0x1d7c2,
0x1d7c4,0x1d7cb,
0x1df00,0x1df1e,
0x1df25,0x1df2a,
0x1e030,0x1e06d,
0x1e100,0x1e12c,
0x1e137,0x1e13d,
0x1e290,0x1e2ad,
0x1e2c0,0x1e2eb,
0x1e4d0,0x1e4eb,
0x1e5d0,0x1e5ed,
0x1e7e0,0x1e7e6,
0x1e7e8,0x1e7eb,
0x1e7ed,0x1e7ee,
0x1e7f0,0x1e7fe,
0x1e800,0x1e8c4,
0x1e900,0x1e943,
0x1ee00,0x1ee03,
0x1ee05,0x1ee1f,
0x1ee21,0x1ee22,
0x1ee29,0x1ee32,
0x1ee34,0x1ee37,
0x1ee4d,0x1ee4f,
0x1ee51,0x1ee52,
0x1ee61,0x1ee62,
0x1ee67,0x1ee6a,
0x1ee6c,0x1ee72,
0x1ee74,0x1ee77,
0x1ee79,0x1ee7c,
0x1ee80,0x1ee89,
0x1ee8b,0x1ee9b,
0x1eea1,0x1eea3,
0x1eea5,0x1eea9,
0x1eeab,0x1eebb,
0x2f800,0x2fa1d,
};

static const Rune ucd_alpha1[] = {
0xaa,
0xb5,
0xba,
0x2ec,
0x2ee,
0x37f,
0x386,
0x38c,
0x559,
0x6d5,
0x6ff,
0x710,
0x7b1,
0x7fa,
0x81a,
0x824,
0x828,
0x93d,
0x950,
0x9b2,
0x9bd,
0x9ce,
0x9fc,
0xa5e,
0xabd,
0xad0,
0xaf9,
0xb3d,
0xb71,
0xb83,
0xb9c,
0xbd0,
0xc3d,
0xc5d,
0xc80,
0xcbd,
0xd3d,
0xd4e,
0xdbd,
0xe84,
0xea5,
0xebd,
0xec6,
0xf00,
0x103f,
0x1061,
0x108e,
0x10c7,
0x10cd,
0x1258,
0x12c0,
0x17d7,
0x17dc,
0x18aa,
0x1aa7,
0x1cfa,
0x1f59,
0x1f5b,
0x1f5d,
0x1fbe,
0x2071,
0x207f,
0x2102,
0x2107,
0x2115,
0x2124,
0x2126,
0x2128,
0x214e,
0x2d27,
0x2d2d,
0x2d6f,
0x2e2f,
0x3400,
0x4dbf,
0x4e00,
0xa7d3,
0xa8fb,
0xa9cf,
0xaa7a,
0xaab1,
0xaac0,
0xaac2,
0xac00,
0xd7a3,
0xfb1d,
0xfb3e,
0x10808,
0x1083c,
0x10a00,
0x10f27,
0x11075,
0x11144,
0x11147,
0x11176,
0x111da,
0x111dc,
0x11288,
0x1133d,
0x11350,
0x1138b,
0x1138e,
0x113b7,
0x113d1,
0x113d3,
0x114c7,
0x11644,
0x116b8,
0x11909,
0x1193f,
0x11941,
0x119e1,
0x119e3,
0x11a00,
0x11a3a,
0x11a50,
0x11a9d,
0x11c40,
0x11d46,
0x11d98,
0x11f02,
0x11fb0,
0x16f50,
0x16fe3,
0x17000,
0x187f7,
0x18d08,
0x1b132,
0x1b155,
0x1d4a2,
0x1d4bb,
0x1d546,
0x1e14e,
0x1e5f0,
0x1e94b,
0x1ee24,
0x1ee27,
0x1ee39,
0x1ee3b,
0x1ee42,
0x1ee47,
0x1ee49,
0x1ee4b,
0x1ee54,
0x1ee57,
0x1ee59,
0x1ee5b,
0x1ee5d,
0x1ee5f,
0x1ee64,
0x1ee7e,
0x20000,
0x2a6df,
0x2a700,
0x2b739,
0x2b740,
0x2b81d,
0x2b820,
0x2cea1,
0x2ceb0,
0x2ebe0,
0x2ebf0,
0x2ee5d,
0x30000,
0x3134a,
0x31350,
0x323af,
};

static const Rune ucd_tolower2[] = {
0x41,0x5a,32,
0xc0,0xd6,32,
0xd8,0xde,32,
0x189,0x18a,205,
0x1b1,0x1b2,217,
0x388,0x38a,37,
0x38e,0x38f,63,
0x391,0x3a1,32,
0x3a3,0x3ab,32,
0x3fd,0x3ff,-130,
0x400,0x40f,80,
0x410,0x42f,32,
0x531,0x556,48,
0x10a0,0x10c5,7264,
0x13a0,0x13ef,38864,
0x13f0,0x13f5,8,
0x1c90,0x1cba,-3008,
0x1cbd,0x1cbf,-3008,
0x1f08,0x1f0f,-8,
0x1f18,0x1f1d,-8,
0x1f28,0x1f2f,-8,
0x1f38,0x1f3f,-8,
0x1f48,0x1f4d,-8,
0x1f68,0x1f6f,-8,
0x1f88,0x1f8f,-8,
0x1f98,0x1f9f,-8,
0x1fa8,0x1faf,-8,
0x1fb8,0x1fb9,-8,
0x1fba,0x1fbb,-74,
0x1fc8,0x1fcb,-86,
0x1fd8,0x1fd9,-8,
0x1fda,0x1fdb,-100,
0x1fe8,0x1fe9,-8,
0x1fea,0x1feb,-112,
0x1ff8,0x1ff9,-128,
0x1ffa,0x1ffb,-126,
0x2160,0x216f,16,
0x24b6,0x24cf,26,
0x2c00,0x2c2f,48,
0x2c7e,0x2c7f,-10815,
0xff21,0xff3a,32,
0x10400,0x10427,40,
0x104b0,0x104d3,40,
0x10570,0x1057a,39,
0x1057c,0x1058a,39,
0x1058c,0x10592,39,
0x10594,0x10595,39,
0x10c80,0x10cb2,64,
0x10d50,0x10d65,32,
0x118a0,0x118bf,32,
0x16e40,0x16e5f,32,
0x1e900,0x1e921,34,
};

static const Rune ucd_tolower1[] = {
0x100,1,
0x102,1,
0x104,1,
0x106,1,
0x108,1,
0x10a,1,
0x10c,1,
0x10e,1,
0x110,1,
0x112,1,
0x114,1,
0x116,1,
0x118,1,
0x11a,1,
0x11c,1,
0x11e,1,
0x120,1,
0x122,1,
0x124,1,
0x126,1,
0x128,1,
0x12a,1,
0x12c,1,
0x12e,1,
0x130,-199,
0x132,1,
0x134,1,
0x136,1,
0x139,1,
0x13b,1,
0x13d,1,
0x13f,1,
0x141,1,
0x143,1,
0x145,1,
0x147,1,
0x14a,1,
0x14c,1,
0x14e,1,
0x150,1,
0x152,1,
0x154,1,
0x156,1,
0x158,1,
0x15a,1,
0x15c,1,
0x15e,1,
0x160,1,
0x162,1,
0x164,1,
0x166,1,
0x168,1,
0x16a,1,
0x16c,1,
0x16e,1,
0x170,1,
0x172,1,
0x174,1,
0x176,1,
0x178,-121,
0x179,1,
0x17b,1,
0x17d,1,
0x181,210,
0x182,1,
0x184,1,
0x186,206,
0x187,1,
0x18b,1,
0x18e,79,
0x18f,202,
0x190,203,
0x191,1,
0x193,205,
0x194,207,
0x196,211,
0x197,209,
0x198,1,
0x19c,211,
0x19d,213,
0x19f,214,
0x1a0,1,
0x1a2,1,
0x1a4,1,
0x1a6,218,
0x1a7,1,
0x1a9,218,
0x1ac,1,
0x1ae,218,
0x1af,1,
0x1b3,1,
0x1b5,1,
0x1b7,219,
0x1b8,1,
0x1bc,1,
0x1c4,2,
0x1c5,1,
0x1c7,2,
0x1c8,1,
0x1ca,2,
0x1cb,1,
0x1cd,1,
0x1cf,1,
0x1d1,1,
0x1d3,1,
0x1d5,1,
0x1d7,1,
0x1d9,1,
0x1db,1,
0x1de,1,
0x1e0,1,
0x1e2,1,
0x1e4,1,
0x1e6,1,
0x1e8,1,
0x1ea,1,
0x1ec,1,
0x1ee,1,
0x1f1,2,
0x1f2,1,
0x1f4,1,
0x1f6,-97,
0x1f7,-56,
0x1f8,1,
0x1fa,1,
0x1fc,1,
0x1fe,1,
0x200,1,
0x202,1,
0x204,1,
0x206,1,
0x208,1,
0x20a,1,
0x20c,1,
0x20e,1,
0x210,1,
0x212,1,
0x214,1,
0x216,1,
0x218,1,
0x21a,1,
0x21c,1,
0x21e,1,
0x220,-130,
0x222,1,
0x224,1,
0x226,1,
0x228,1,
0x22a,1,
0x22c,1,
0x22e,1,
0x230,1,
0x232,1,
0x23a,10795,
0x23b,1,
0x23d,-163,
0x23e,10792,
0x241,1,
0x243,-195,
0x244,69,
0x245,71,
0x246,1,
0x248,1,
0x24a,1,
0x24c,1,
0x24e,1,
0x370,1,
0x372,1,
0x376,1,
0x37f,116,
0x386,38,
0x38c,64,
0x3cf,8,
0x3d8,1,
0x3da,1,
0x3dc,1,
0x3de,1,
0x3e0,1,
0x3e2,1,
0x3e4,1,
0x3e6,1,
0x3e8,1,
0x3ea,1,
0x3ec,1,
0x3ee,1,
0x3f4,-60,
0x3f7,1,
0x3f9,-7,
0x3fa,1,
0x460,1,
0x462,1,
0x464,1,
0x466,1,
0x468,1,
0x46a,1,
0x46c,1,
0x46e,1,
0x470,1,
0x472,1,
0x474,1,
0x476,1,
0x478,1,
0x47a,1,
0x47c,1,
0x47e,1,
0x480,1,
0x48a,1,
0x48c,1,
0x48e,1,
0x490,1,
0x492,1,
0x494,1,
0x496,1,
0x498,1,
0x49a,1,
0x49c,1,
0x49e,1,
0x4a0,1,
0x4a2,1,
0x4a4,1,
0x4a6,1,
0x4a8,1,
0x4aa,1,
0x4ac,1,
0x4ae,1,
0x4b0,1,
0x4b2,1,
0x4b4,1,
0x4b6,1,
0x4b8,1,
0x4ba,1,
0x4bc,1,
0x4be,1,
0x4c0,15,
0x4c1,1,
0x4c3,1,
0x4c5,1,
0x4c7,1,
0x4c9,1,
0x4cb,1,
0x4cd,1,
0x4d0,1,
0x4d2,1,
0x4d4,1,
0x4d6,1,
0x4d8,1,
0x4da,1,
0x4dc,1,
0x4de,1,
0x4e0,1,
0x4e2,1,
0x4e4,1,
0x4e6,1,
0x4e8,1,
0x4ea,1,
0x4ec,1,
0x4ee,1,
0x4f0,1,
0x4f2,1,
0x4f4,1,
0x4f6,1,
0x4f8,1,
0x4fa,1,
0x4fc,1,
0x4fe,1,
0x500,1,
0x502,1,
0x504,1,
0x506,1,
0x508,1,
0x50a,1,
0x50c,1,
0x50e,1,
0x510,1,
0x512,1,
0x514,1,
0x516,1,
0x518,1,
0x51a,1,
0x51c,1,
0x51e,1,
0x520,1,
0x522,1,
0x524,1,
0x526,1,
0x528,1,
0x52a,1,
0x52c,1,
0x52e,1,
0x10c7,7264,
0x10cd,7264,
0x1c89,1,
0x1e00,1,
0x1e02,1,
0x1e04,1,
0x1e06,1,
0x1e08,1,
0x1e0a,1,
0x1e0c,1,
0x1e0e,1,
0x1e10,1,
0x1e12,1,
0x1e14,1,
0x1e16,1,
0x1e18,1,
0x1e1a,1,
0x1e1c,1,
0x1e1e,1,
0x1e20,1,
0x1e22,1,
0x1e24,1,
0x1e26,1,
0x1e28,1,
0x1e2a,1,
0x1e2c,1,
0x1e2e,1,
0x1e30,1,
0x1e32,1,
0x1e34,1,
0x1e36,1,
0x1e38,1,
0x1e3a,1,
0x1e3c,1,
0x1e3e,1,
0x1e40,1,
0x1e42,1,
0x1e44,1,
0x1e46,1,
0x1e48,1,
0x1e4a,1,
0x1e4c,1,
0x1e4e,1,
0x1e50,1,
0x1e52,1,
0x1e54,1,
0x1e56,1,
0x1e58,1,
0x1e5a,1,
0x1e5c,1,
0x1e5e,1,
0x1e60,1,
0x1e62,1,
0x1e64,1,
0x1e66,1,
0x1e68,1,
0x1e6a,1,
0x1e6c,1,
0x1e6e,1,
0x1e70,1,
0x1e72,1,
0x1e74,1,
0x1e76,1,
0x1e78,1,
0x1e7a,1,
0x1e7c,1,
0x1e7e,1,
0x1e80,1,
0x1e82,1,
0x1e84,1,
0x1e86,1,
0x1e88,1,
0x1e8a,1,
0x1e8c,1,
0x1e8e,1,
0x1e90,1,
0x1e92,1,
0x1e94,1,
0x1e9e,-7615,
0x1ea0,1,
0x1ea2,1,
0x1ea4,1,
0x1ea6,1,
0x1ea8,1,
0x1eaa,1,
0x1eac,1,
0x1eae,1,
0x1eb0,1,
0x1eb2,1,
0x1eb4,1,
0x1eb6,1,
0x1eb8,1,
0x1eba,1,
0x1ebc,1,
0x1ebe,1,
0x1ec0,1,
0x1ec2,1,
0x1ec4,1,
0x1ec6,1,
0x1ec8,1,
0x1eca,1,
0x1ecc,1,
0x1ece,1,
0x1ed0,1,
0x1ed2,1,
0x1ed4,1,
0x1ed6,1,
0x1ed8,1,
0x1eda,1,
0x1edc,1,
0x1ede,1,
0x1ee0,1,
0x1ee2,1,
0x1ee4,1,
0x1ee6,1,
0x1ee8,1,
0x1eea,1,
0x1eec,1,
0x1eee,1,
0x1ef0,1,
0x1ef2,1,
0x1ef4,1,
0x1ef6,1,
0x1ef8,1,
0x1efa,1,
0x1efc,1,
0x1efe,1,
0x1f59,-8,
0x1f5b,-8,
0x1f5d,-8,
0x1f5f,-8,
0x1fbc,-9,
0x1fcc,-9,
0x1fec,-7,
0x1ffc,-9,
0x2126,-7517,
0x212a,-8383,
0x212b,-8262,
0x2132,28,
0x2183,1,
0x2c60,1,
0x2c62,-10743,
0x2c63,-3814,
0x2c64,-10727,
0x2c67,1,
0x2c69,1,
0x2c6b,1,
0x2c6d,-10780,
0x2c6e,-10749,
0x2c6f,-10783,
0x2c70,-10782,
0x2c72,1,
0x2c75,1,
0x2c80,1,
0x2c82,1,
0x2c84,1,
0x2c86,1,
0x2c88,1,
0x2c8a,1,
0x2c8c,1,
0x2c8e,1,
0x2c90,1,
0x2c92,1,
0x2c94,1,
0x2c96,1,
0x2c98,1,
0x2c9a,1,
0x2c9c,1,
0x2c9e,1,
0x2ca0,1,
0x2ca2,1,
0x2ca4,1,
0x2ca6,1,
0x2ca8,1,
0x2caa,1,
0x2cac,1,
0x2cae,1,
0x2cb0,1,
0x2cb2,1,
0x2cb4,1,
0x2cb6,1,
0x2cb8,1,
0x2cba,1,
0x2cbc,1,
0x2cbe,1,
0x2cc0,1,
0x2cc2,1,
0x2cc4,1,
0x2cc6,1,
0x2cc8,1,
0x2cca,1,
0x2ccc,1,
0x2cce,1,
0x2cd0,1,
0x2cd2,1,
0x2cd4,1,
0x2cd6,1,
0x2cd8,1,
0x2cda,1,
0x2cdc,1,
0x2cde,1,
0x2ce0,1,
0x2ce2,1,
0x2ceb,1,
0x2ced,1,
0x2cf2,1,
0xa640,1,
0xa642,1,
0xa644,1,
0xa646,1,
0xa648,1,
0xa64a,1,
0xa64c,1,
0xa64e,1,
0xa650,1,
0xa652,1,
0xa654,1,
0xa656,1,
0xa658,1,
0xa65a,1,
0xa65c,1,
0xa65e,1,
0xa660,1,
0xa662,1,
0xa664,1,
0xa666,1,
0xa668,1,
0xa66a,1,
0xa66c,1,
0xa680,1,
0xa682,1,
0xa684,1,
0xa686,1,
0xa688,1,
0xa68a,1,
0xa68c,1,
0xa68e,1,
0xa690,1,
0xa692,1,
0xa694,1,
0xa696,1,
0xa698,1,
0xa69a,1,
0xa722,1,
0xa724,1,
0xa726,1,
0xa728,1,
0xa72a,1,
0xa72c,1,
0xa72e,1,
0xa732,1,
0xa734,1,
0xa736,1,
0xa738,1,
0xa73a,1,
0xa73c,1,
0xa73e,1,
0xa740,1,
0xa742,1,
0xa744,1,
0xa746,1,
0xa748,1,
0xa74a,1,
0xa74c,1,
0xa74e,1,
0xa750,1,
0xa752,1,
0xa754,1,
0xa756,1,
0xa758,1,
0xa75a,1,
0xa75c,1,
0xa75e,1,
0xa760,1,
0xa762,1,
0xa764,1,
0xa766,1,
0xa768,1,
0xa76a,1,
0xa76c,1,
0xa76e,1,
0xa779,1,
0xa77b,1,
0xa77d,-35332,
0xa77e,1,
0xa780,1,
0xa782,1,
0xa784,1,
0xa786,1,
0xa78b,1,
0xa78d,-42280,
0xa790,1,
0xa792,1,
0xa796,1,
0xa798,1,
0xa79a,1,
0xa79c,1,
0xa79e,1,
0xa7a0,1,
0xa7a2,1,
0xa7a4,1,
0xa7a6,1,
0xa7a8,1,
0xa7aa,-42308,
0xa7ab,-42319,
0xa7ac,-42315,
0xa7ad,-42305,
0xa7ae,-42308,
0xa7b0,-42258,
0xa7b1,-42282,
0xa7b2,-42261,
0xa7b3,928,
0xa7b4,1,
0xa7b6,1,
0xa7b8,1,
0xa7ba,1,
0xa7bc,1,
0xa7be,1,
0xa7c0,1,
0xa7c2,1,
0xa7c4,-48,
0xa7c5,-42307,
0xa7c6,-35384,
0xa7c7,1,
0xa7c9,1,
0xa7cb,-42343,
0xa7cc,1,
0xa7d0,1,
0xa7d6,1,
0xa7d8,1,
0xa7da,1,
0xa7dc,-42561,
0xa7f5,1,
};

static const Rune ucd_toupper2[] = {
0x61,0x7a,-32,
0xe0,0xf6,-32,
0xf8,0xfe,-32,
0x23f,0x240,10815,
0x256,0x257,-205,
0x28a,0x28b,-217,
0x37b,0x37d,130,
0x3ad,0x3af,-37,
0x3b1,0x3c1,-32,
0x3c3,0x3cb,-32,
0x3cd,0x3ce,-63,
0x430,0x44f,-32,
0x450,0x45f,-80,
0x561,0x586,-48,
0x10d0,0x10fa,3008,
0x10fd,0x10ff,3008,
0x13f8,0x13fd,-8,
0x1c83,0x1c84,-6242,
0x1f00,0x1f07,8,
0x1f10,0x1f15,8,
0x1f20,0x1f27,8,
0x1f30,0x1f37,8,
0x1f40,0x1f45,8,
0x1f60,0x1f67,8,
0x1f70,0x1f71,74,
0x1f72,0x1f75,86,
0x1f76,0x1f77,100,
0x1f78,0x1f79,128,
0x1f7a,0x1f7b,112,
0x1f7c,0x1f7d,126,
0x1f80,0x1f87,8,
0x1f90,0x1f97,8,
0x1fa0,0x1fa7,8,
0x1fb0,0x1fb1,8,
0x1fd0,0x1fd1,8,
0x1fe0,0x1fe1,8,
0x2170,0x217f,-16,
0x24d0,0x24e9,-26,
0x2c30,0x2c5f,-48,
0x2d00,0x2d25,-7264,
0xab70,0xabbf,-38864,
0xff41,0xff5a,-32,
0x10428,0x1044f,-40,
0x104d8,0x104fb,-40,
0x10597,0x105a1,-39,
0x105a3,0x105b1,-39,
0x105b3,0x105b9,-39,
0x105bb,0x105bc,-39,
0x10cc0,0x10cf2,-64,
0x10d70,0x10d85,-32,
0x118c0,0x118df,-32,
0x16e60,0x16e7f,-32,
0x1e922,0x1e943,-34,
};

static const Rune ucd_toupper1[] = {
0xb5,743,
0xff,121,
0x101,-1,
0x103,-1,
0x105,-1,
0x107,-1,
0x109,-1,
0x10b,-1,
0x10d,-1,
0x10f,-1,
0x111,-1,
0x113,-1,
0x115,-1,
0x117,-1,
0x119,-1,
0x11b,-1,
0x11d,-1,
0x11f,-1,
0x121,-1,
0x123,-1,
0x125,-1,
0x127,-1,
0x129,-1,
0x12b,-1,
0x12d,-1,
0x12f,-1,
0x131,-232,
0x133,-1,
0x135,-1,
0x137,-1,
0x13a,-1,
0x13c,-1,
0x13e,-1,
0x140,-1,
0x142,-1,
0x144,-1,
0x146,-1,
0x148,-1,
0x14b,-1,
0x14d,-1,
0x14f,-1,
0x151,-1,
0x153,-1,
0x155,-1,
0x157,-1,
0x159,-1,
0x15b,-1,
0x15d,-1,
0x15f,-1,
0x161,-1,
0x163,-1,
0x165,-1,
0x167,-1,
0x169,-1,
0x16b,-1,
0x16d,-1,
0x16f,-1,
0x171,-1,
0x173,-1,
0x175,-1,
0x177,-1,
0x17a,-1,
0x17c,-1,
0x17e,-1,
0x17f,-300,
0x180,195,
0x183,-1,
0x185,-1,
0x188,-1,
0x18c,-1,
0x192,-1,
0x195,97,
0x199,-1,
0x19a,163,
0x19b,42561,
0x19e,130,
0x1a1,-1,
0x1a3,-1,
0x1a5,-1,
0x1a8,-1,
0x1ad,-1,
0x1b0,-1,
0x1b4,-1,
0x1b6,-1,
0x1b9,-1,
0x1bd,-1,
0x1bf,56,
0x1c5,-1,
0x1c6,-2,
0x1c8,-1,
0x1c9,-2,
0x1cb,-1,
0x1cc,-2,
0x1ce,-1,
0x1d0,-1,
0x1d2,-1,
0x1d4,-1,
0x1d6,-1,
0x1d8,-1,
0x1da,-1,
0x1dc,-1,
0x1dd,-79,
0x1df,-1,
0x1e1,-1,
0x1e3,-1,
0x1e5,-1,
0x1e7,-1,
0x1e9,-1,
0x1eb,-1,
0x1ed,-1,
0x1ef,-1,
0x1f2,-1,
0x1f3,-2,
0x1f5,-1,
0x1f9,-1,
0x1fb,-1,
0x1fd,-1,
0x1ff,-1,
0x201,-1,
0x203,-1,
0x205,-1,
0x207,-1,
0x209,-1,
0x20b,-1,
0x20d,-1,
0x20f,-1,
0x211,-1,
0x213,-1,
0x215,-1,
0x217,-1,
0x219,-1,
0x21b,-1,
0x21d,-1,
0x21f,-1,
0x223,-1,
0x225,-1,
0x227,-1,
0x229,-1,
0x22b,-1,
0x22d,-1,
0x22f,-1,
0x231,-1,
0x233,-1,
0x23c,-1,
0x242,-1,
0x247,-1,
0x249,-1,
0x24b,-1,
0x24d,-1,
0x24f,-1,
0x250,10783,
0x251,10780,
0x252,10782,
0x253,-210,
0x254,-206,
0x259,-202,
0x25b,-203,
0x25c,42319,
0x260,-205,
0x261,42315,
0x263,-207,
0x264,42343,
0x265,42280,
0x266,42308,
0x268,-209,
0x269,-211,
0x26a,42308,
0x26b,10743,
0x26c,42305,
0x26f,-211,
0x271,10749,
0x272,-213,
0x275,-214,
0x27d,10727,
0x280,-218,
0x282,42307,
0x283,-218,
0x287,42282,
0x288,-218,
0x289,-69,
0x28c,-71,
0x292,-219,
0x29d,42261,
0x29e,42258,
0x345,84,
0x371,-1,
0x373,-1,
0x377,-1,
0x3ac,-38,
0x3c2,-31,
0x3cc,-64,
0x3d0,-62,
0x3d1,-57,
0x3d5,-47,
0x3d6,-54,
0x3d7,-8,
0x3d9,-1,
0x3db,-1,
0x3dd,-1,
0x3df,-1,
0x3e1,-1,
0x3e3,-1,
0x3e5,-1,
0x3e7,-1,
0x3e9,-1,
0x3eb,-1,
0x3ed,-1,
0x3ef,-1,
0x3f0,-86,
0x3f1,-80,
0x3f2,7,
0x3f3,-116,
0x3f5,-96,
0x3f8,-1,
0x3fb,-1,
0x461,-1,
0x463,-1,
0x465,-1,
0x467,-1,
0x469,-1,
0x46b,-1,
0x46d,-1,
0x46f,-1,
0x471,-1,
0x473,-1,
0x475,-1,
0x477,-1,
0x479,-1,
0x47b,-1,
0x47d,-1,
0x47f,-1,
0x481,-1,
0x48b,-1,
0x48d,-1,
0x48f,-1,
0x491,-1,
0x493,-1,
0x495,-1,
0x497,-1,
0x499,-1,
0x49b,-1,
0x49d,-1,
0x49f,-1,
0x4a1,-1,
0x4a3,-1,
0x4a5,-1,
0x4a7,-1,
0x4a9,-1,
0x4ab,-1,
0x4ad,-1,
0x4af,-1,
0x4b1,-1,
0x4b3,-1,
0x4b5,-1,
0x4b7,-1,
0x4b9,-1,
0x4bb,-1,
0x4bd,-1,
0x4bf,-1,
0x4c2,-1,
0x4c4,-1,
0x4c6,-1,
0x4c8,-1,
0x4ca,-1,
0x4cc,-1,
0x4ce,-1,
0x4cf,-15,
0x4d1,-1,
0x4d3,-1,
0x4d5,-1,
0x4d7,-1,
0x4d9,-1,
0x4db,-1,
0x4dd,-1,
0x4df,-1,
0x4e1,-1,
0x4e3,-1,
0x4e5,-1,
0x4e7,-1,
0x4e9,-1,
0x4eb,-1,
0x4ed,-1,
0x4ef,-1,
0x4f1,-1,
0x4f3,-1,
0x4f5,-1,
0x4f7,-1,
0x4f9,-1,
0x4fb,-1,
0x4fd,-1,
0x4ff,-1,
0x501,-1,
0x503,-1,
0x505,-1,
0x507,-1,
0x509,-1,
0x50b,-1,
0x50d,-1,
0x50f,-1,
0x511,-1,
0x513,-1,
0x515,-1,
0x517,-1,
0x519,-1,
0x51b,-1,
0x51d,-1,
0x51f,-1,
0x521,-1,
0x523,-1,
0x525,-1,
0x527,-1,
0x529,-1,
0x52b,-1,
0x52d,-1,
0x52f,-1,
0x1c80,-6254,
0x1c81,-6253,
0x1c82,-6244,
0x1c85,-6243,
0x1c86,-6236,
0x1c87,-6181,
0x1c88,35266,
0x1c8a,-1,
0x1d79,35332,
0x1d7d,3814,
0x1d8e,35384,
0x1e01,-1,
0x1e03,-1,
0x1e05,-1,
0x1e07,-1,
0x1e09,-1,
0x1e0b,-1,
0x1e0d,-1,
0x1e0f,-1,
0x1e11,-1,
0x1e13,-1,
0x1e15,-1,
0x1e17,-1,
0x1e19,-1,
0x1e1b,-1,
0x1e1d,-1,
0x1e1f,-1,
0x1e21,-1,
0x1e23,-1,
0x1e25,-1,
0x1e27,-1,
0x1e29,-1,
0x1e2b,-1,
0x1e2d,-1,
0x1e2f,-1,
0x1e31,-1,
0x1e33,-1,
0x1e35,-1,
0x1e37,-1,
0x1e39,-1,
0x1e3b,-1,
0x1e3d,-1,
0x1e3f,-1,
0x1e41,-1,
0x1e43,-1,
0x1e45,-1,
0x1e47,-1,
0x1e49,-1,
0x1e4b,-1,
0x1e4d,-1,
0x1e4f,-1,
0x1e51,-1,
0x1e53,-1,
0x1e55,-1,
0x1e57,-1,
0x1e59,-1,
0x1e5b,-1,
0x1e5d,-1,
0x1e5f,-1,
0x1e61,-1,
0x1e63,-1,
0x1e65,-1,
0x1e67,-1,
0x1e69,-1,
0x1e6b,-1,
0x1e6d,-1,
0x1e6f,-1,
0x1e71,-1,
0x1e73,-1,
0x1e75,-1,
0x1e77,-1,
0x1e79,-1,
0x1e7b,-1,
0x1e7d,-1,
0x1e7f,-1,
0x1e81,-1,
0x1e83,-1,
0x1e85,-1,
0x1e87,-1,
0x1e89,-1,
0x1e8b,-1,
0x1e8d,-1,
0x1e8f,-1,
0x1e91,-1,
0x1e93,-1,
0x1e95,-1,
0x1e9b,-59,
0x1ea1,-1,
0x1ea3,-1,
0x1ea5,-1,
0x1ea7,-1,
0x1ea9,-1,
0x1eab,-1,
0x1ead,-1,
0x1eaf,-1,
0x1eb1,-1,
0x1eb3,-1,
0x1eb5,-1,
0x1eb7,-1,
0x1eb9,-1,
0x1ebb,-1,
0x1ebd,-1,
0x1ebf,-1,
0x1ec1,-1,
0x1ec3,-1,
0x1ec5,-1,
0x1ec7,-1,
0x1ec9,-1,
0x1ecb,-1,
0x1ecd,-1,
0x1ecf,-1,
0x1ed1,-1,
0x1ed3,-1,
0x1ed5,-1,
0x1ed7,-1,
0x1ed9,-1,
0x1edb,-1,
0x1edd,-1,
0x1edf,-1,
0x1ee1,-1,
0x1ee3,-1,
0x1ee5,-1,
0x1ee7,-1,
0x1ee9,-1,
0x1eeb,-1,
0x1eed,-1,
0x1eef,-1,
0x1ef1,-1,
0x1ef3,-1,
0x1ef5,-1,
0x1ef7,-1,
0x1ef9,-1,
0x1efb,-1,
0x1efd,-1,
0x1eff,-1,
0x1f51,8,
0x1f53,8,
0x1f55,8,
0x1f57,8,
0x1fb3,9,
0x1fbe,-7205,
0x1fc3,9,
0x1fe5,7,
0x1ff3,9,
0x214e,-28,
0x2184,-1,
0x2c61,-1,
0x2c65,-10795,
0x2c66,-10792,
0x2c68,-1,
0x2c6a,-1,
0x2c6c,-1,
0x2c73,-1,
0x2c76,-1,
0x2c81,-1,
0x2c83,-1,
0x2c85,-1,
0x2c87,-1,
0x2c89,-1,
0x2c8b,-1,
0x2c8d,-1,
0x2c8f,-1,
0x2c91,-1,
0x2c93,-1,
0x2c95,-1,
0x2c97,-1,
0x2c99,-1,
0x2c9b,-1,
0x2c9d,-1,
0x2c9f,-1,
0x2ca1,-1,
0x2ca3,-1,
0x2ca5,-1,
0x2ca7,-1,
0x2ca9,-1,
0x2cab,-1,
0x2cad,-1,
0x2caf,-1,
0x2cb1,-1,
0x2cb3,-1,
0x2cb5,-1,
0x2cb7,-1,
0x2cb9,-1,
0x2cbb,-1,
0x2cbd,-1,
0x2cbf,-1,
0x2cc1,-1,
0x2cc3,-1,
0x2cc5,-1,
0x2cc7,-1,
0x2cc9,-1,
0x2ccb,-1,
0x2ccd,-1,
0x2ccf,-1,
0x2cd1,-1,
0x2cd3,-1,
0x2cd5,-1,
0x2cd7,-1,
0x2cd9,-1,
0x2cdb,-1,
0x2cdd,-1,
0x2cdf,-1,
0x2ce1,-1,
0x2ce3,-1,
0x2cec,-1,
0x2cee,-1,
0x2cf3,-1,
0x2d27,-7264,
0x2d2d,-7264,
0xa641,-1,
0xa643,-1,
0xa645,-1,
0xa647,-1,
0xa649,-1,
0xa64b,-1,
0xa64d,-1,
0xa64f,-1,
0xa651,-1,
0xa653,-1,
0xa655,-1,
0xa657,-1,
0xa659,-1,
0xa65b,-1,
0xa65d,-1,
0xa65f,-1,
0xa661,-1,
0xa663,-1,
0xa665,-1,
0xa667,-1,
0xa669,-1,
0xa66b,-1,
0xa66d,-1,
0xa681,-1,
0xa683,-1,
0xa685,-1,
0xa687,-1,
0xa689,-1,
0xa68b,-1,
0xa68d,-1,
0xa68f,-1,
0xa691,-1,
0xa693,-1,
0xa695,-1,
0xa697,-1,
0xa699,-1,
0xa69b,-1,
0xa723,-1,
0xa725,-1,
0xa727,-1,
0xa729,-1,
0xa72b,-1,
0xa72d,-1,
0xa72f,-1,
0xa733,-1,
0xa735,-1,
0xa737,-1,
0xa739,-1,
0xa73b,-1,
0xa73d,-1,
0xa73f,-1,
0xa741,-1,
0xa743,-1,
0xa745,-1,
0xa747,-1,
0xa749,-1,
0xa74b,-1,
0xa74d,-1,
0xa74f,-1,
0xa751,-1,
0xa753,-1,
0xa755,-1,
0xa757,-1,
0xa759,-1,
0xa75b,-1,
0xa75d,-1,
0xa75f,-1,
0xa761,-1,
0xa763,-1,
0xa765,-1,
0xa767,-1,
0xa769,-1,
0xa76b,-1,
0xa76d,-1,
0xa76f,-1,
0xa77a,-1,
0xa77c,-1,
0xa77f,-1,
0xa781,-1,
0xa783,-1,
0xa785,-1,
0xa787,-1,
0xa78c,-1,
0xa791,-1,
0xa793,-1,
0xa794,48,
0xa797,-1,
0xa799,-1,
0xa79b,-1,
0xa79d,-1,
0xa79f,-1,
0xa7a1,-1,
0xa7a3,-1,
0xa7a5,-1,
0xa7a7,-1,
0xa7a9,-1,
0xa7b5,-1,
0xa7b7,-1,
0xa7b9,-1,
0xa7bb,-1,
0xa7bd,-1,
0xa7bf,-1,
0xa7c1,-1,
0xa7c3,-1,
0xa7c8,-1,
0xa7ca,-1,
0xa7cd,-1,
0xa7d1,-1,
0xa7d7,-1,
0xa7d9,-1,
0xa7db,-1,
0xa7f6,-1,
0xab53,-928,
};

static const Rune ucd_tolower_full[] = {
0x130,0x69,0x307,0x0,
0x1f88,0x1f80,0x0,0x0,
0x1f89,0x1f81,0x0,0x0,
0x1f8a,0x1f82,0x0,0x0,
0x1f8b,0x1f83,0x0,0x0,
0x1f8c,0x1f84,0x0,0x0,
0x1f8d,0x1f85,0x0,0x0,
0x1f8e,0x1f86,0x0,0x0,
0x1f8f,0x1f87,0x0,0x0,
0x1f98,0x1f90,0x0,0x0,
0x1f99,0x1f91,0x0,0x0,
0x1f9a,0x1f92,0x0,0x0,
0x1f9b,0x1f93,0x0,0x0,
0x1f9c,0x1f94,0x0,0x0,
0x1f9d,0x1f95,0x0,0x0,
0x1f9e,0x1f96,0x0,0x0,
0x1f9f,0x1f97,0x0,0x0,
0x1fa8,0x1fa0,0x0,0x0,
0x1fa9,0x1fa1,0x0,0x0,
0x1faa,0x1fa2,0x0,0x0,
0x1fab,0x1fa3,0x0,0x0,
0x1fac,0x1fa4,0x0,0x0,
0x1fad,0x1fa5,0x0,0x0,
0x1fae,0x1fa6,0x0,0x0,
0x1faf,0x1fa7,0x0,0x0,
0x1fbc,0x1fb3,0x0,0x0,
0x1fcc,0x1fc3,0x0,0x0,
0x1ffc,0x1ff3,0x0,0x0,
};

static const Rune ucd_toupper_full[] = {
0xdf,0x53,0x53,0x0,0x0,
0x149,0x2bc,0x4e,0x0,0x0,
0x1f0,0x4a,0x30c,0x0,0x0,
0x390,0x399,0x308,0x301,0x0,
0x3b0,0x3a5,0x308,0x301,0x0,
0x587,0x535,0x552,0x0,0x0,
0x1e96,0x48,0x331,0x0,0x0,
0x1e97,0x54,0x308,0x0,0x0,
0x1e98,0x57,0x30a,0x0,0x0,
0x1e99,0x59,0x30a,0x0,0x0,
0x1e9a,0x41,0x2be,0x0,0x0,
0x1f50,0x3a5,0x313,0x0,0x0,
0x1f52,0x3a5,0x313,0x300,0x0,
0x1f54,0x3a5,0x313,0x301,0x0,
0x1f56,0x3a5,0x313,0x342,0x0,
0x1f80,0x1f08,0x399,0x0,0x0,
0x1f81,0x1f09,0x399,0x0,0x0,
0x1f82,0x1f0a,0x399,0x0,0x0,
0x1f83,0x1f0b,0x399,0x0,0x0,
0x1f84,0x1f0c,0x399,0x0,0x0,
0x1f85,0x1f0d,0x399,0x0,0x0,
0x1f86,0x1f0e,0x399,0x0,0x0,
0x1f87,0x1f0f,0x399,0x0,0x0,
0x1f88,0x1f08,0x399,0x0,0x0,
0x1f89,0x1f09,0x399,0x0,0x0,
0x1f8a,0x1f0a,0x399,0x0,0x0,
0x1f8b,0x1f0b,0x399,0x0,0x0,
0x1f8c,0x1f0c,0x399,0x0,0x0,
0x1f8d,0x1f0d,0x399,0x0,0x0,
0x1f8e,0x1f0e,0x399,0x0,0x0,
0x1f8f,0x1f0f,0x399,0x0,0x0,
0x1f90,0x1f28,0x399,0x0,0x0,
0x1f91,0x1f29,0x399,0x0,0x0,
0x1f92,0x1f2a,0x399,0x0,0x0,
0x1f93,0x1f2b,0x399,0x0,0x0,
0x1f94,0x1f2c,0x399,0x0,0x0,
0x1f95,0x1f2d,0x399,0x0,0x0,
0x1f96,0x1f2e,0x399,0x0,0x0,
0x1f97,0x1f2f,0x399,0x0,0x0,
0x1f98,0x1f28,0x399,0x0,0x0,
0x1f99,0x1f29,0x399,0x0,0x0,
0x1f9a,0x1f2a,0x399,0x0,0x0,
0x1f9b,0x1f2b,0x399,0x0,0x0,
0x1f9c,0x1f2c,0x399,0x0,0x0,
0x1f9d,0x1f2d,0x399,0x0,0x0,
0x1f9e,0x1f2e,0x399,0x0,0x0,
0x1f9f,0x1f2f,0x399,0x0,0x0,
0x1fa0,0x1f68,0x399,0x0,0x0,
0x1fa1,0x1f69,0x399,0x0,0x0,
0x1fa2,0x1f6a,0x399,0x0,0x0,
0x1fa3,0x1f6b,0x399,0x0,0x0,
0x1fa4,0x1f6c,0x399,0x0,0x0,
0x1fa5,0x1f6d,0x399,0x0,0x0,
0x1fa6,0x1f6e,0x399,0x0,0x0,
0x1fa7,0x1f6f,0x399,0x0,0x0,
0x1fa8,0x1f68,0x399,0x0,0x0,
0x1fa9,0x1f69,0x399,0x0,0x0,
0x1faa,0x1f6a,0x399,0x0,0x0,
0x1fab,0x1f6b,0x399,0x0,0x0,
0x1fac,0x1f6c,0x399,0x0,0x0,
0x1fad,0x1f6d,0x399,0x0,0x0,
0x1fae,0x1f6e,0x399,0x0,0x0,
0x1faf,0x1f6f,0x399,0x0,0x0,
0x1fb2,0x1fba,0x399,0x0,0x0,
0x1fb3,0x391,0x399,0x0,0x0,
0x1fb4,0x386,0x399,0x0,0x0,
0x1fb6,0x391,0x342,0x0,0x0,
0x1fb7,0x391,0x342,0x399,0x0,
0x1fbc,0x391,0x399,0x0,0x0,
0x1fc2,0x1fca,0x399,0x0,0x0,
0x1fc3,0x397,0x399,0x0,0x0,
0x1fc4,0x389,0x399,0x0,0x0,
0x1fc6,0x397,0x342,0x0,0x0,
0x1fc7,0x397,0x342,0x399,0x0,
0x1fcc,0x397,0x399,0x0,0x0,
0x1fd2,0x399,0x308,0x300,0x0,
0x1fd3,0x399,0x308,0x301,0x0,
0x1fd6,0x399,0x342,0x0,0x0,
0x1fd7,0x399,0x308,0x342,0x0,
0x1fe2,0x3a5,0x308,0x300,0x0,
0x1fe3,0x3a5,0x308,0x301,0x0,
0x1fe4,0x3a1,0x313,0x0,0x0,
0x1fe6,0x3a5,0x342,0x0,0x0,
0x1fe7,0x3a5,0x308,0x342,0x0,
0x1ff2,0x1ffa,0x399,0x0,0x0,
0x1ff3,0x3a9,0x399,0x0,0x0,
0x1ff4,0x38f,0x399,0x0,0x0,
0x1ff6,0x3a9,0x342,0x0,0x0,
0x1ff7,0x3a9,0x342,0x399,0x0,
0x1ffc,0x3a9,0x399,0x0,0x0,
0xfb00,0x46,0x46,0x0,0x0,
0xfb01,0x46,0x49,0x0,0x0,
0xfb02,0x46,0x4c,0x0,0x0,
0xfb03,0x46,0x46,0x49,0x0,
0xfb04,0x46,0x46,0x4c,0x0,
0xfb05,0x53,0x54,0x0,0x0,
0xfb06,0x53,0x54,0x0,0x0,
0xfb13,0x544,0x546,0x0,0x0,
0xfb14,0x544,0x535,0x0,0x0,
0xfb15,0x544,0x53b,0x0,0x0,
0xfb16,0x54e,0x546,0x0,0x0,
0xfb17,0x544,0x53d,0x0,0x0,
};

#define nelem(a) (int)(sizeof (a) / sizeof (a)[0])

typedef unsigned char uchar;

enum
{
	Bit1	= 7,
	Bitx	= 6,
	Bit2	= 5,
	Bit3	= 4,
	Bit4	= 3,
	Bit5	= 2,

	T1	= ((1<<(Bit1+1))-1) ^ 0xFF,
	Tx	= ((1<<(Bitx+1))-1) ^ 0xFF,
	T2	= ((1<<(Bit2+1))-1) ^ 0xFF,
	T3	= ((1<<(Bit3+1))-1) ^ 0xFF,
	T4	= ((1<<(Bit4+1))-1) ^ 0xFF,
	T5	= ((1<<(Bit5+1))-1) ^ 0xFF,

	Rune1	= (1<<(Bit1+0*Bitx))-1,
	Rune2	= (1<<(Bit2+1*Bitx))-1,
	Rune3	= (1<<(Bit3+2*Bitx))-1,
	Rune4	= (1<<(Bit4+3*Bitx))-1,

	Maskx	= (1<<Bitx)-1,
	Testx	= Maskx ^ 0xFF,

	Bad	= Runeerror
};

int
chartorune(Rune *rune, const char *str)
{
	int c, c1, c2, c3;
	int l;

	if((uchar)str[0] == 0xc0 && (uchar)str[1] == 0x80) {
		*rune = 0;
		return 2;
	}

	c = *(uchar*)str;
	if(c < Tx) {
		*rune = c;
		return 1;
	}

	c1 = *(uchar*)(str+1) ^ Tx;
	if(c1 & Testx)
		goto bad;
	if(c < T3) {
		if(c < T2)
			goto bad;
		l = ((c << Bitx) | c1) & Rune2;
		if(l <= Rune1)
			goto bad;
		*rune = l;
		return 2;
	}

	c2 = *(uchar*)(str+2) ^ Tx;
	if(c2 & Testx)
		goto bad;
	if(c < T4) {
		l = ((((c << Bitx) | c1) << Bitx) | c2) & Rune3;
		if(l <= Rune2)
			goto bad;
		*rune = l;
		return 3;
	}

	if(UTFmax >= 4) {
		c3 = *(uchar*)(str+3) ^ Tx;
		if(c3 & Testx)
			goto bad;
		if(c < T5) {
			l = ((((((c << Bitx) | c1) << Bitx) | c2) << Bitx) | c3) & Rune4;
			if(l <= Rune3)
				goto bad;
			if(l > Runemax)
				goto bad;
			*rune = l;
			return 4;
		}
	}

bad:
	*rune = Bad;
	return 1;
}

int
runetochar(char *str, const Rune *rune)
{
	int c = *rune;

	if (c == 0) {
		str[0] = (char)0xc0;
		str[1] = (char)0x80;
		return 2;
	}

	if(c <= Rune1) {
		str[0] = c;
		return 1;
	}

	if(c <= Rune2) {
		str[0] = T2 | (c >> 1*Bitx);
		str[1] = Tx | (c & Maskx);
		return 2;
	}

	if(c > Runemax)
		c = Runeerror;
	if(c <= Rune3) {
		str[0] = T3 |  (c >> 2*Bitx);
		str[1] = Tx | ((c >> 1*Bitx) & Maskx);
		str[2] = Tx |  (c & Maskx);
		return 3;
	}

	str[0] = T4 |  (c >> 3*Bitx);
	str[1] = Tx | ((c >> 2*Bitx) & Maskx);
	str[2] = Tx | ((c >> 1*Bitx) & Maskx);
	str[3] = Tx |  (c & Maskx);
	return 4;
}

int
runelen(int c)
{
	Rune rune;
	char str[10];

	rune = c;
	return runetochar(str, &rune);
}

static const Rune *
ucd_bsearch(Rune c, const Rune *t, int n, int ne)
{
	const Rune *p;
	int m;

	while(n > 1) {
		m = n/2;
		p = t + m*ne;
		if(c >= p[0]) {
			t = p;
			n = n-m;
		} else
			n = m;
	}
	if(n && c >= t[0])
		return t;
	return 0;
}

Rune
tolowerrune(Rune c)
{
	const Rune *p;

	p = ucd_bsearch(c, ucd_tolower2, nelem(ucd_tolower2)/3, 3);
	if(p && c >= p[0] && c <= p[1])
		return c + p[2];
	p = ucd_bsearch(c, ucd_tolower1, nelem(ucd_tolower1)/2, 2);
	if(p && c == p[0])
		return c + p[1];
	return c;
}

Rune
toupperrune(Rune c)
{
	const Rune *p;

	p = ucd_bsearch(c, ucd_toupper2, nelem(ucd_toupper2)/3, 3);
	if(p && c >= p[0] && c <= p[1])
		return c + p[2];
	p = ucd_bsearch(c, ucd_toupper1, nelem(ucd_toupper1)/2, 2);
	if(p && c == p[0])
		return c + p[1];
	return c;
}

int
islowerrune(Rune c)
{
	const Rune *p;

	p = ucd_bsearch(c, ucd_toupper2, nelem(ucd_toupper2)/3, 3);
	if(p && c >= p[0] && c <= p[1])
		return 1;
	p = ucd_bsearch(c, ucd_toupper1, nelem(ucd_toupper1)/2, 2);
	if(p && c == p[0])
		return 1;
	return 0;
}

int
isupperrune(Rune c)
{
	const Rune *p;

	p = ucd_bsearch(c, ucd_tolower2, nelem(ucd_tolower2)/3, 3);
	if(p && c >= p[0] && c <= p[1])
		return 1;
	p = ucd_bsearch(c, ucd_tolower1, nelem(ucd_tolower1)/2, 2);
	if(p && c == p[0])
		return 1;
	return 0;
}

int
isalpharune(Rune c)
{
	const Rune *p;

	p = ucd_bsearch(c, ucd_alpha2, nelem(ucd_alpha2)/2, 2);
	if(p && c >= p[0] && c <= p[1])
		return 1;
	p = ucd_bsearch(c, ucd_alpha1, nelem(ucd_alpha1), 1);
	if(p && c == p[0])
		return 1;
	return 0;
}

const Rune *
tolowerrune_full(Rune c)
{
	const Rune *p;
	p = ucd_bsearch(c, ucd_tolower_full, nelem(ucd_tolower_full)/4, 4);
	if(p && c == p[0])
		return p + 1;
	return NULL;
}

const Rune *
toupperrune_full(Rune c)
{
	const Rune *p;
	p = ucd_bsearch(c, ucd_toupper_full, nelem(ucd_toupper_full)/5, 5);
	if(p && c == p[0])
		return p + 1;
	return NULL;
}
