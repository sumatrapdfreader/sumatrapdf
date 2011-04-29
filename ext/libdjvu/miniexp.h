/* -*- C -*-
// -------------------------------------------------------------------
// MiniExp - Library for handling lisp expressions
// Copyright (c) 2005  Leon Bottou
//
// This software is subject to, and may be distributed under, the
// GNU General Public License, either Version 2 of the license,
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
# ifdef WIN32
#  ifdef DLL_EXPORT
#   define MINILISPAPI __declspec(dllexport)
#  else
#   define MINILISPAPI __declspec(dllimport)
#  endif
# endif
#endif
#ifndef MINILISPAPI
# define MINILISPAPI /**/
#endif
  

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
   The only such objects defined in this file are strings. */


/* -------- NUMBERS -------- */

/* Minilisp numbers can represent any integer 
   in range [-2^29...2^29-1] */


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

/* miniexp_string --
   Constructs a string expression by copying string s. */

MINILISPAPI miniexp_t miniexp_string(const char *s);

/* miniexp_substring --
   Constructs a string expression by copying 
   at most n character from string s. */

MINILISPAPI miniexp_t miniexp_substring(const char *s, int n);

/* miniexp_concat --
   Concat all the string expressions in list <l>. */

MINILISPAPI miniexp_t miniexp_concat(miniexp_t l);





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
     miniexp_string(), miniexp_substring(),
     miniexp_concat(), miniexp_pprin(), miniexp_pprint(),
     miniexp_gc(), and minilisp_release_gc_lock().  A
     function that does not cause calls to these functions
     does not need to bother about minivars.

   * Other functions should make sure that all useful
     s-expression are directly or indirectly secured by a
     minivar_t object. In case of doubt, use minivars
     everywhere.

   * Function arguments should remain <miniexp_t> in order
     to allow interoperability with the C language. As a
     consequence, functions must often copy their arguments
     into minivars in order to make sure they remain
     allocated. A small performance improvement can be
     achieved by deciding that the function should always be
     called using properly secured arguments. This is more
     difficult to get right.

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
     * any ascii character with a non zero entry 
       in array <minilisp_macrochar_parser>.

   - Symbols are represented by their name.
     Vertical bars <|> can be used to delimit names that
     contain blanks, special characters, non printable
     characters, non ascii characters, or 
     can be confused for a number.
     
   - Numbers follow the syntax specified by the C
     function strtol() with base=0.

   - Strings are delimited by double quotes.
     All C string escapes are recognized.
     Non printable ascii characters must be escaped.

   - List are represented by an open parenthesis <(>
     followed by the space separated list elements,
     followed by a closing parenthesis <)>.
     When the cdr of the last pair is non zero,
     the closed parenthesis is preceded by 
     a space, a dot <.>, a space, and the textual 
     representation of the cdr.

   - When the parser encounters an ascii character corresponding
     to a non zero function pointer in <minilisp_macrochar_parser>,
     the function is invoked and must return a possibly empty
     list of miniexps to be returned by subsequent 
     invocations of the parser. */


/* minilisp_puts/getc/ungetc --
   All minilisp i/o is performed by invoking 
   these functions pointers. */

extern MINILISPAPI int (*minilisp_puts)(const char *s);
extern MINILISPAPI int (*minilisp_getc)(void);
extern MINILISPAPI int (*minilisp_ungetc)(int c);

/* minilisp_set_output --
   minilisp_set_input --
   Sets the above function to read/write from/to file f. 
   Only defined when <stdio.h> has been included. */

#if defined(stdin)
MINILISPAPI void minilisp_set_output(FILE *f);
MINILISPAPI void minilisp_set_input(FILE *f);
#endif

/* miniexp_read --
   Reads an expression by repeatedly
   invoking <minilisp_getc> and <minilisp_ungetc>.
   Returns <miniexp_dummy> when an error occurs. */

MINILISPAPI miniexp_t miniexp_read(void);

/* miniexp_prin --
   miniexp_print --
   Prints a minilisp expression by repeatedly invoking <minilisp_puts>.
   Only <minilisp_print> outputs a final newline character. 
   These functions are safe to call anytime. */

MINILISPAPI miniexp_t miniexp_prin(miniexp_t p);
MINILISPAPI miniexp_t miniexp_print(miniexp_t p);

/* miniexp_pprin --
   miniexp_pprint --
   Prints a minilisp expression with reasonably pretty line breaks. 
   Argument <width> is the intended number of columns. 
   Only <minilisp_pprint> outputs a final newline character. 
   These functions can cause a garbage collection to occur. */

MINILISPAPI miniexp_t miniexp_pprin(miniexp_t p, int width);
MINILISPAPI miniexp_t miniexp_pprint(miniexp_t p, int width);

/* miniexp_pname --
   Returns a string containing the textual representation
   of a minilisp expression. Set argument <width> to zero
   to output a single line, or to a positive value to
   perform pretty line breaks for this intended number of columns.
   These functions can cause a garbage collection to occur.
   It works by temporarily redefining <minilisp_puts>. */

MINILISPAPI miniexp_t miniexp_pname(miniexp_t p, int width);

/* minilisp_print_7bits --
   When this flag is set, all non ascii characters 
   in strings are escaped in octal. */

extern MINILISPAPI int minilisp_print_7bits;

/* minilisp_macrochar_parser --
   A non zero entry in this array defines a special parsing
   function that runs when the corresponding character is
   encountered. */

extern MINILISPAPI miniexp_t (*minilisp_macrochar_parser[128])(void);



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
  ~minivar_t() { if ((*pprev = next)) next->pprev = pprev; }
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
  /* mark: iterates over miniexps contained by this object
     for garbage collecting purposes. */
  virtual void mark(minilisp_mark_t*);
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
  /* SumatraPDF: don't execute code until asked to */\
  const miniexp_t cls::classname = 0;\
  miniexp_t cls::classof() const {\
    return miniexp_symbol(name); }\
  bool cls::isa(miniexp_t n) const {\
    return (classof()==n) || (supercls::isa(n)); }


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


#endif /* __cplusplus */





/* -------------------------------------------------- */
/* THE END                                            */
/* -------------------------------------------------- */

#endif /* MINIEXP_H */
