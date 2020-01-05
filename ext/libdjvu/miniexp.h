/* -*- C -*-
// -------------------------------------------------------------------
// MiniExp - Library for handling lisp expressions
// Copyright (c) 2005  Leon Bottou
//
// This software is subject to, and may be distributed under, the GNU
// Lesser General Public License, either Version 2.1 of the license,
// or (at your option) any later version. The license should have
// accompanied the software or you may obtain a copy of the license
// from the Free Software Foundation at http://www.fsf.org .
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// -------------------------------------------------------------------
*/

#ifndef MINIEXP_H
#define MINIEXP_H

#ifdef __cplusplus
extern "C" { 
# ifndef __cplusplus
}
# endif
#endif

#ifndef MINILISPAPI
# ifdef _WIN32
#  ifdef MINILISPAPI_EXPORT
#   define MINILISPAPI __declspec(dllexport)
#  else
#   define MINILISPAPI __declspec(dllimport)
#  endif
# endif
#endif
#ifndef MINILISPAPI
# define MINILISPAPI /**/
#endif

#ifndef __cplusplus
# ifndef inline
#  if defined(__GNUC__)
#   define inline __inline__
#  elif defined(_MSC_VER)
#   define inline __inline
#  else
#   define inline /**/
#  endif
# endif
#endif

#include <stddef.h>  

/* -------------------------------------------------- */
/* LISP EXPRESSIONS                                   */
/* -------------------------------------------------- */

/* miniexp_t -- 
   Opaque pointer type representing a lisp expression,
   also known as s-expression. 
   S-expressions can be viewed as a simple and powerful 
   alternative to XML.  DjVu uses s-expressions to handle
   annotations. Both the decoding api <ddjvuapi.h> and 
   program <djvused> use s-expressions to describe the 
   hidden text information and the navigation 
   information */


typedef struct miniexp_s* miniexp_t;


/* There are four basic types of lisp expressions,
   numbers, symbols, pairs, and objects.
   The latter category can represent any c++ object
   that inherits class <miniobj_t> defined later in this file.
   Strings and floating point numbers are implemented this way.*/


/* -------- NUMBERS -------- */

/* Minilisp numbers represent integers 
   covering at least range [-2^29...2^29-1] */


/* miniexp_numberp --
   Tests if an expression is a number. */

static inline int miniexp_numberp(miniexp_t p) {
  return (((size_t)(p)&3)==3);
}

/* miniexp_to_int --
   Returns the integer corresponding to a lisp expression.
   Assume that the expression is indeed a number. */

static inline int miniexp_to_int(miniexp_t p) {
  return (((int)(size_t)(p))>>2);
}

/* miniexp_number --
   Constructs the expression corresponding to an integer. */

static inline miniexp_t miniexp_number(int x) {
  return (miniexp_t) (size_t) ((x<<2)|3);
}
   


/* -------- SYMBOLS -------- */

/* The textual representation of a minilisp symbol is a 
   sequence of printable characters forming an identifier. 
   Each symbol has a unique representation and remain 
   permanently allocated. To compare two symbols, 
   simply compare the <miniexp_t> pointers. */


/* miniexp_symbolp --
   Tests if an expression is a symbol. */

static inline int miniexp_symbolp(miniexp_t p) {
  return ((((size_t)p)&3)==2);
}

/* miniexp_to_name --
   Returns the symbol name as a string.
   Returns NULL if the expression is not a symbol. */
   
MINILISPAPI const char* miniexp_to_name(miniexp_t p);

/* miniexp_symbol --
   Returns the unique symbol expression with the specified name. */

MINILISPAPI miniexp_t miniexp_symbol(const char *name);



/* -------- PAIRS -------- */

/* Pairs (also named "cons") are the basic building blocks for 
   minilisp lists. Each pair contains two expression:
   - the <car> represents the first element of a list.
   - the <cdr> usually is a pair representing the rest of the list.
   The empty list is represented by a null pointer. */


/* miniexp_nil --
   The empty list. */

#define miniexp_nil ((miniexp_t)(size_t)0)

/* miniexp_dummy --
   An invalid expression used to represent
   various exceptional conditions. */

#define miniexp_dummy ((miniexp_t)(size_t)2)

/* miniexp_listp --
   Tests if an expression is either a pair or the empty list. */   

static inline int miniexp_listp(miniexp_t p) {
  return ((((size_t)p)&3)==0);
}

/* miniexp_consp --
   Tests if an expression is a pair. */

static inline int miniexp_consp(miniexp_t p) {
  return p && miniexp_listp(p);
}

/* miniexp_length --
   Returns the length of a list.
   Returns 0 for non lists, -1 for circular lists. */

MINILISPAPI int miniexp_length(miniexp_t p);

/* miniexp_car --
   miniexp_cdr --
   Returns the car or cdr of a pair. */

static inline miniexp_t miniexp_car(miniexp_t p) {
  if (miniexp_consp(p))
    return ((miniexp_t*)p)[0];
  return miniexp_nil;
}

static inline miniexp_t miniexp_cdr(miniexp_t p) {
  if (miniexp_consp(p))
    return ((miniexp_t*)p)[1];
  return miniexp_nil;
}

/* miniexp_cXXr --
   Represent common combinations of car and cdr. */

MINILISPAPI miniexp_t miniexp_caar (miniexp_t p);
MINILISPAPI miniexp_t miniexp_cadr (miniexp_t p);
MINILISPAPI miniexp_t miniexp_cdar (miniexp_t p);
MINILISPAPI miniexp_t miniexp_cddr (miniexp_t p);
MINILISPAPI miniexp_t miniexp_caddr(miniexp_t p);
MINILISPAPI miniexp_t miniexp_cdddr(miniexp_t p);

/* miniexp_nth --
   Returns the n-th element of a list. */

MINILISPAPI miniexp_t miniexp_nth(int n, miniexp_t l);

/* miniexp_cons --
   Constructs a pair. */

MINILISPAPI miniexp_t miniexp_cons(miniexp_t car, miniexp_t cdr);

/* miniexp_rplaca --
   miniexp_rplacd --
   Changes the car or the cdr of a pair. */

MINILISPAPI miniexp_t miniexp_rplaca(miniexp_t pair, miniexp_t newcar);
MINILISPAPI miniexp_t miniexp_rplacd(miniexp_t pair, miniexp_t newcdr);

/* miniexp_reverse --
   Reverses a list in place. */

MINILISPAPI miniexp_t miniexp_reverse(miniexp_t p);


/* -------- OBJECTS (GENERIC) -------- */

/* Object expressions represent a c++ object
   that inherits class <miniobj_t> defined later.
   Each object expression has a symbolic class name
   and a pointer to the c++ object. */

/* miniexp_objectp --
   Tests if an expression is an object. */

static inline int miniexp_objectp(miniexp_t p) {
  return ((((size_t)p)&3)==1);
}

/* miniexp_classof --
   Returns the symbolic class of an expression.
   Returns nil if the expression is not an object. */

MINILISPAPI miniexp_t miniexp_classof(miniexp_t p);

/* miniexp_isa --
   If <p> is an instance of class named <c> or one of
   its subclasses, returns the actual class name.
   Otherwise returns miniexp_nil. */

MINILISPAPI miniexp_t miniexp_isa(miniexp_t p, miniexp_t c);


/* -------- OBJECTS (STRINGS) -------- */

/* miniexp_stringp --
   Tests if an expression is a string. */

MINILISPAPI int miniexp_stringp(miniexp_t p);

/* miniexp_to_str --
   Returns the c string represented by the expression.
   Returns NULL if the expression is not a string.
   The c string remains valid as long as the
   corresponding lisp object exists. */

MINILISPAPI const char *miniexp_to_str(miniexp_t p);

/* miniexp_to_lstr ---- 
   Returns the length of the string represented by the expression.
   Optionally returns the c string into *sp.  
   Return 0 and makes *sp null if the expression is not a string. */

MINILISPAPI size_t miniexp_to_lstr(miniexp_t p, const char **sp);

/* miniexp_string --
   Constructs a string expression by copying zero terminated string s. */

MINILISPAPI miniexp_t miniexp_string(const char *s);

/* miniexp_lstring --
   Constructs a string expression by copying len bytes from s. */

MINILISPAPI miniexp_t miniexp_lstring(size_t len, const char *s);

/* miniexp_substring --
   Constructs a string expression by copying at most len bytes 
   from zero terminated string s. */

MINILISPAPI miniexp_t miniexp_substring(const char *s, int len);

/* miniexp_concat --
   Concat all the string expressions in list <l>. */

MINILISPAPI miniexp_t miniexp_concat(miniexp_t l);



/* -------- OBJECTS (FLOATNUM) -------- */

/* miniexp_floatnump --
   Tests if an expression is an object
   representing a floating point number. */

MINILISPAPI int miniexp_floatnump(miniexp_t p);

/* miniexp_floatnum --
   Returns a new floating point number object. */

MINILISPAPI miniexp_t miniexp_floatnum(double x);

/* miniexp_doublep --
   Tests if an expression can be converted
   to a double precision number. */

MINILISPAPI int miniexp_doublep(miniexp_t p);

/* miniexp_to_double --
   Returns a double precision number corresponding to 
   a lisp expression. */

MINILISPAPI double miniexp_to_double(miniexp_t p);

/* miniexp_double --
   Returns a lisp expression representing a double
   precision number. This will be a number if it fits
   and a floatnum otherwise.
 */

MINILISPAPI miniexp_t miniexp_double(double x);


/* -------------------------------------------------- */
/* GARBAGE COLLECTION                                 */
/* -------------------------------------------------- */


/* The garbage collector reclaims the memory allocated for
   lisp expressions no longer in use.  It is automatically
   invoked by the pair and object allocation functions when
   the available memory runs low.  It is however possible to
   temporarily disable it.

   The trick is to determine which lisp expressions are in
   use at a given moment. This package takes a simplistic
   approach. All objects of type <minivar_t> are chained and
   can reference an arbitrary lisp expression.  Garbage
   collection preserves all lisp expressions referenced by a
   minivar, as well as all lisp expressions that can be
   accessed from these. When called automatically, 
   garbage collection also preserves the sixteen most recently 
   created miniexps in order to make sure that temporaries do 
   not vanish in the middle of complicated C expressions.
     
   The minivar class is designed such that C++ program can
   directly use instances of <minivar_t> as normal
   <miniexp_t> variables.  There is almost no overhead
   accessing or changing the lisp expression referenced by a
   minivar. However, the minivar chain must be updated
   whenever the minivar object is constructed or destructed.
   
   Example (in C++ only):
     miniexp_t copy_in_reverse(miniexp_t p) {
        minivar_t l = miniexp_nil;
        while (miniexp_consp(p)) {
          l = miniexp_cons(miniexp_car(p), l); 
          p = miniexp_cdr(p); 
        }
        return l;
     }

   When to use minivar_t instead of miniexp_t?

   * A function that only navigates properly secured
     s-expressions without modifying them does not need to
     bother about minivars.

   * Only the following miniexp functions can cause a
     garbage collection: miniexp_cons(), miniexp_object(),
     miniexp_string(), miniexp_substring(), miniexp_pname(),
     miniexp_concat(), miniexp_pprin(), miniexp_pprint(),
     miniexp_gc(), and minilisp_release_gc_lock().  A
     function that does not cause calls to these functions
     does not need to bother about minivars.

   * Other functions should make sure that all useful
     s-expression are directly or indirectly secured by a
     minivar_t object. In case of doubt, use minivars
     everywhere.

   * Function arguments should remain <miniexp_t> in order
     to allow interoperability with the C language. 
     It is assumed that these arguments have been properly
     secured by the caller and cannot disappear if a 
     garbage collection occurs.

   C programs cannot use minivars as easily as C++ programs.
   Wrappers are provided to allocate minivars and to access
   their value. This is somehow inconvenient.  It might be
   more practical to control the garbage collector
   invocations with <minilisp_acquire_gc_lock()> and
   <minilisp_release_gc_lock()>...  */
   

/* minilisp_gc --
   Invokes the garbage collector now. */

MINILISPAPI void minilisp_gc(void);

/* minilisp_info --
   Prints garbage collector statistics. */

MINILISPAPI void minilisp_info(void);

/* minilisp_acquire_gc_lock --
   minilisp_release_gc_lock --
   Temporarily disables automatic garbage collection.
   Acquire/release pairs may be nested. 
   Both functions return their argument unmodified.
   This is practical because <minilisp_release_gc_lock>
   can invoke the garbage collector. Before doing
   so it stores its argument in a minivar to 
   preserve it. 

   Example (in C):
     miniexp_t copy_in_reverse(miniexp_t p) {
        miniexp_t l = 0;
        minilisp_acquire_gc_lock(0);
        while (miniexp_consp(p)) {
          l = miniexp_cons(miniexp_car(p), l); 
          p = miniexp_cdr(p); 
        }
        return minilisp_release_gc_lock(l); 
     }
   
   Disabling garbage collection for a long time 
   increases the memory consumption. */

MINILISPAPI miniexp_t minilisp_acquire_gc_lock(miniexp_t);
MINILISPAPI miniexp_t minilisp_release_gc_lock(miniexp_t);

/* minivar_t --
   The minivar type. */
#ifdef __cplusplus
class minivar_t;
#else
typedef struct minivar_s minivar_t;
#endif

/* minivar_alloc --
   minivar_free --
   Wrappers for creating and destroying minivars in C. */

MINILISPAPI minivar_t *minivar_alloc(void);
MINILISPAPI void minivar_free(minivar_t *v);

/* minivar_pointer --
   Wrappers to access the lisp expression referenced
   by a minivar. This function returns a pointer
   to the actual miniexp_t variable. */

MINILISPAPI miniexp_t *minivar_pointer(minivar_t *v);

/* minilisp_debug -- 
   Setting the debug flag runs the garbage collector 
   very often. This is extremely slow, but can be
   useful to debug memory allocation problems. */

MINILISPAPI void minilisp_debug(int debugflag);

/* minilisp_finish --
   Deallocates everything.  This is only useful when using
   development tools designed to check for memory leaks.  
   No miniexp function can be used after calling this. */

MINILISPAPI void minilisp_finish(void);


/* -------------------------------------------------- */
/* INPUT/OUTPUT                                       */
/* -------------------------------------------------- */

/* Notes about the textual representation of miniexps.

   - Special characters are:
     * the parenthesis <(> and <)>,
     * the double quote <">,
     * the vertical bar <|>,
     * any other ascii character with a non zero entry 
       in the macro character array.
     * the dieze character <#>, when followed by another
       dieze or by an ascii character with a non zero entry 
       in the dieze character array.

   - Symbols are represented by their name.
     Symbols whose name contains blanks, special characters, 
     non printable characters, non ascii characters, 
     or can be confused for a number are delimited
     by vertical bars <|> and can contain two consecutive
     vertical bars to represent a single vertical bar character.
     
   - Numbers follow the syntax specified by the C
     function strtol() with base=0, but are required
     to start with a digit or with a sign character
     followed by another character.

   - Floating point follow the syntax specified by the C
     function strtod() with base=0, but are required
     to start with a digit or with a sign character
     followed by another character.

   - Strings are delimited by double quotes.
     All non printable ASCII characters must be escaped. 
     Besides all the usual C string escape sequences,
     UTF8-encoded Unicode characters in range 0..0x10ffff
     can be represented by escape sequence <\u> followed
     by four hexadecimal digits or escape sequence <\U>
     followed by six hexadecimal digits. Surrogate pairs
     are always recognized as a single Unicode character.
     The effect of invalid escape sequences is unspecified.

   - List are represented by an open parenthesis <(>
     followed by the space separated list elements,
     followed by a closing parenthesis <)>.
     When the cdr of the last pair is non zero,
     the closed parenthesis is preceded by 
     a space, a dot <.>, a space, and the textual 
     representation of the cdr.

   - When the parser encounters an ascii character corresponding
     to a non zero function pointer in the macro character array,
     the function is invoked and must return a possibly empty
     list of miniexps to be returned by subsequent 
     invocations of the parser. The same process happens when
     the parser encounters a dieze character followed by an 
     ascii character corresponding to a non zero function pointer
     int the dieze character array. */


/* miniexp_pname --
   Returns a string containing the textual representation
   of a minilisp expression. Set argument <width> to zero
   to output a single line, or to a positive value to
   perform pretty line breaks for this intended number of columns.
   This function can cause a garbage collection to occur. */

MINILISPAPI miniexp_t miniexp_pname(miniexp_t p, int width);


/* miniexp_io_t -- 
   This structure is used to describe how to perform input/output
   operations. Input/output operations are performed through function 
   pointers <fputs>, <fgetc>, and <ungetc>, which are similar to their 
   stdio counterparts. Variable <data> defines four pointers that can 
   be used as a closure by the I/O functions.
      Variable <p_flags> optionally points to a flag word that customize the
   printing operation. All ASCII control characters present in strings are
   displayed using C escapes sequences. Flag <miniexp_io_print7bits> causes
   all other non ASCII characters to be escaped. Flag <miniexp_io_u6escape>
   and <miniexp_io_u4escape> respectively authorize using the long and
   short utf8 escape sequences "\U" and "\u". Their absence may force
   using surrogate short escape sequences or only octal sequences.
   Flag <miniexp_io_quotemoresyms> causes the output code to also quote
   all symbols that start with a digit or with a sign character followed
   by another character.
      When both <p_macrochar> and <p_macroqueue> are non zero, a non zero 
   entry in <p_macrochar[c]> defines a special parsing function that is called
   when <miniexp_read_r> encounters the character <c> (in range 0 to 127.)
   When both <p_diezechar> and <p_macroqueue> are non zero, a non zero entry
   in <p_diezechar[c]> defines a special parsing function that is called when
   <miniexp_read_r> encounters the character '#' followed by character <c> (in
   range 0 to 127.)  These parsing functions return a list of <miniexp_t> that
   function <miniexp_read_r> returns one-by-one before processing more
   input. This list is stored in the variable pointed by <io.p_macroqueue>.  
*/

typedef struct miniexp_io_s miniexp_io_t;
typedef miniexp_t (*miniexp_macrochar_t)(miniexp_io_t*);

struct miniexp_io_s
{
  int (*fputs)(miniexp_io_t*, const char*);
  int (*fgetc)(miniexp_io_t*);
  int (*ungetc)(miniexp_io_t*, int);
  void *data[4];
  int *p_flags; /* previously named p_print7bits */
  miniexp_macrochar_t *p_macrochar;
  miniexp_macrochar_t *p_diezechar;
  minivar_t *p_macroqueue;
  minivar_t *p_reserved;
};

#define miniexp_io_print7bits          0x1
#define miniexp_io_u4escape            0x2
#define miniexp_io_u6escape            0x4
#define miniexp_io_quotemoresymbols    0x20

/* miniexp_io_init --
   Initialize a default <miniexp_io_t> structure
   that reads from stdin and prints to stdout. 
   Field <data[0]> is used to hold the stdin file pointer.
   Field <data[1]> is used to hold the stdout file pointer.
   Fields <p_flags>, <p_macrochar>, <p_diezechar>
   and <p_macroqueue> are set to point to zero-initialized
   shared variables. */

MINILISPAPI void miniexp_io_init(miniexp_io_t *io);

/* miniexp_io_set_{input,output} --
   Override the file descriptor used for input or output.
   You must call <miniexp_io_init> before. */

#if defined(stdin)
MINILISPAPI void miniexp_io_set_output(miniexp_io_t *io, FILE *f);
MINILISPAPI void miniexp_io_set_input(miniexp_io_t *io, FILE *f);
#endif

/* miniexp_read_r --
   Reads an expression by repeatedly
   invoking <minilisp_getc> and <minilisp_ungetc>.
   Returns <miniexp_dummy> when an error occurs. */

MINILISPAPI miniexp_t miniexp_read_r(miniexp_io_t *io);

/* miniexp_prin_r, miniexp_print_r --
   Prints a minilisp expression by repeatedly invoking <minilisp_puts>.
   Only <minilisp_print> outputs a final newline character. 
   These functions are safe to call anytime. */

MINILISPAPI miniexp_t miniexp_prin_r(miniexp_io_t *io, miniexp_t p);
MINILISPAPI miniexp_t miniexp_print_r(miniexp_io_t *io, miniexp_t p);

/* miniexp_pprin_r, miniexp_pprint_r --
   Prints a minilisp expression with reasonably pretty line breaks. 
   Argument <width> is the intended number of columns. 
   Only <minilisp_pprint> outputs a final newline character. 
   These functions can cause a garbage collection to occur. */

MINILISPAPI miniexp_t miniexp_pprin_r(miniexp_io_t *io, miniexp_t p, int w);
MINILISPAPI miniexp_t miniexp_pprint_r(miniexp_io_t *io, miniexp_t p, int w);

/* miniexp_io, miniexp_read, miniexp_{,p}prin{,t} --
   Variable <miniexp_io> contains the pre-initialized input/output data
   structure that is used by the non-reentrant input/output functions. */

extern MINILISPAPI miniexp_io_t miniexp_io;
MINILISPAPI miniexp_t miniexp_read(void);
MINILISPAPI miniexp_t miniexp_prin(miniexp_t p);
MINILISPAPI miniexp_t miniexp_print(miniexp_t p);
MINILISPAPI miniexp_t miniexp_pprin(miniexp_t p, int width);
MINILISPAPI miniexp_t miniexp_pprint(miniexp_t p, int width);

 
/* Backward compatibility (will eventually disappear) */
extern MINILISPAPI int (*minilisp_puts)(const char *);
extern MINILISPAPI int (*minilisp_getc)(void);
extern MINILISPAPI int (*minilisp_ungetc)(int);
extern MINILISPAPI miniexp_t (*minilisp_macrochar_parser[128])(void);
extern MINILISPAPI miniexp_t (*minilisp_diezechar_parser[128])(void);
extern MINILISPAPI miniexp_macrochar_t miniexp_macrochar[128];
extern MINILISPAPI minivar_t miniexp_macroqueue;
extern MINILISPAPI int minilisp_print_7bits;
#if defined(stdin)
MINILISPAPI void minilisp_set_output(FILE *f);
MINILISPAPI void minilisp_set_input(FILE *f);
#endif

/* -------------------------------------------------- */
/* STUFF FOR C++ ONLY                                 */
/* -------------------------------------------------- */

#ifdef __cplusplus
# ifndef __cplusplus
{
# endif
} // extern "C"

typedef void minilisp_mark_t(miniexp_t *pp);

/* -------- MINIVARS -------- */

/* minivar_t --
   A class for protected garbage collector variables. */

class MINILISPAPI 
minivar_t 
{
  miniexp_t data;
  minivar_t *next;
  minivar_t **pprev;
public:
  minivar_t();
  minivar_t(miniexp_t p);
  minivar_t(const minivar_t &v);
  operator miniexp_t&() { return data; }
  miniexp_t* operator&() { return &data; }
  minivar_t& operator=(miniexp_t p) { data = p; return *this; }
  minivar_t& operator=(const minivar_t &v) { data = v.data; return *this; }
  ~minivar_t();
#ifdef MINIEXP_IMPLEMENTATION
  static minivar_t *vars;
  static void mark(minilisp_mark_t*);
#endif
};


/* -------- MINIOBJ -------- */


/* miniobj_t --
   The base class for c++ objects 
   represented by object expressions. */

class MINILISPAPI 
miniobj_t {
 public:
  virtual ~miniobj_t();

  /* --- stuff defined by MINIOBJ_DECLARE --- */
  /* classname: a symbol characterizing this class. */
  static const miniexp_t classname;
  /* classof: class name symbol for this object. */
  virtual miniexp_t classof() const = 0;
  /* isa -- tests if this is an instance of <classname>. */
  virtual bool isa(miniexp_t classname) const;

  /* --- optional stuff --- */
  /* pname: returns a printable name for this object.
     The caller must deallocate the result with delete[]. */
  virtual char *pname() const;
  /* stringp, doublep: tells whether this object should be
     interpreted/printed as a generic string (for miniexp_strinp) 
     or a double (for miniexp_doublep). */
  virtual bool stringp(const char* &s, size_t &l) const;
  virtual bool doublep(double &d) const;
  /* mark: calls action() on all member miniexps of the object,
     for garbage collecting purposes. */
  virtual void mark(minilisp_mark_t *action);
  /* destroy: called by the garbage collector to
     deallocate the object. Defaults to 'delete this'. */
  virtual void destroy();
     
};

/* MINIOBJ_DECLARE --
   MINIOBJ_IMPLEMENT --
   Useful code fragments for implementing 
   the mandatory part of miniobj subclasses. */

#define MINIOBJ_DECLARE(cls, supercls, name) \
  public: static const miniexp_t classname; \
          virtual miniexp_t classof() const; \
          virtual bool isa(miniexp_t) const; 

#define MINIOBJ_IMPLEMENT(cls, supercls, name)\
  const miniexp_t cls::classname = miniexp_symbol(name);\
  miniexp_t cls::classof() const {\
    return cls::classname; }\
  bool cls::isa(miniexp_t n) const {\
    return (cls::classname==n) || (supercls::isa(n)); }


/* miniexp_to_obj --
   Returns a pointer to the object represented by an lisp
   expression. Returns NULL if the expression is not an
   object expression.
*/

static inline miniobj_t *miniexp_to_obj(miniexp_t p) {
  if (miniexp_objectp(p))
    return ((miniobj_t**)(((size_t)p)&~((size_t)3)))[0];
  return 0;
}

/* miniexp_object --
   Create an object expression for a given object. */

MINILISPAPI miniexp_t miniexp_object(miniobj_t *obj);


/* miniexp_mutate -- 
   Atomically modifies a member of a garbage collected object. 
   The object implementation must call this function to change 
   the contents of a member variable <v> of object <obj>.
   Returns <p>*/

MINILISPAPI miniexp_t miniexp_mutate(miniexp_t obj, miniexp_t *v, miniexp_t p);


#endif /* __cplusplus */





/* -------------------------------------------------- */
/* THE END                                            */
/* -------------------------------------------------- */

#endif /* MINIEXP_H */
