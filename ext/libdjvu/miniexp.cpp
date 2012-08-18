/* -*- C++ -*-
// -------------------------------------------------------------------
// MiniExp - Library for handling lisp expressions
// Copyright (c) 2005  Leon Bottou
//
// This software is subject to, and may be distributed under, the
// GNU General Public License, either version 2 of the license
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma implementation "miniexp.h"
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#define MINIEXP_IMPLEMENTATION

#include "miniexp.h"

#ifdef HAVE_NAMESPACES
# define BEGIN_ANONYMOUS_NAMESPACE namespace {
# define END_ANONYMOUS_NAMESPACE }
#else
# define BEGIN_ANONYMOUS_NAMESPACE 
# define END_ANONYMOUS_NAMESPACE
#endif


/* -------------------------------------------------- */
/* ASSERT                                            */
/* -------------------------------------------------- */

#if defined(__GNUC__)
static void 
assertfail(const char *fn, int ln) 
  __attribute__((noreturn));
#endif

static void
assertfail(const char *fn, int ln)
{
  fprintf(stderr,"Assertion failed: %s:%d\n",fn,ln);
  abort();
}

#define ASSERT(x) \
  do { if (!(x)) assertfail(__FILE__,__LINE__); } while(0)


/* -------------------------------------------------- */
/* SYMBOLS                                            */
/* -------------------------------------------------- */

static unsigned int 
hashcode(const char *s)
{
  long h = 0x1013;
  while (*s)
    {
      h = (h<<6) | ((h&0xfc000000)>>26);
      h ^= (*s);
      s++;
    }
  return h;
}

BEGIN_ANONYMOUS_NAMESPACE

class symtable_t 
{
public:
  int nelems;
  int nbuckets;
  struct sym { unsigned int h; struct sym *l; char *n; };
  struct sym **buckets;
  symtable_t();
  ~symtable_t();
  struct sym *lookup(const char *n, bool create=false);
  void resize(int); 
private:
  symtable_t(const symtable_t&);
  symtable_t& operator=(const symtable_t&);
};

symtable_t::symtable_t()
  : nelems(0), nbuckets(0), buckets(0)
{
  resize(7);
}

symtable_t::~symtable_t()
{
  int i=0;
  for (; i<nbuckets; i++)
    while (buckets[i])
      {
        struct sym *r = buckets[i];
        buckets[i] = r->l;
        delete [] r->n;
        delete r;
      }
  delete [] buckets;
}

void
symtable_t::resize(int nb)
{
  struct sym **b = new sym*[nb];
  memset(b, 0, nb*sizeof(sym*));
  int i=0;
  for (; i<nbuckets; i++)
    while (buckets[i])
      {
        struct sym *s = buckets[i];
        int j = s->h % nb;
        buckets[i] = s->l;
        s->l = b[j];
        b[j] = s;
      }
  delete [] buckets;
  buckets = b;
  nbuckets = nb;
}

struct symtable_t::sym *
symtable_t::lookup(const char *n, bool create)
{
  if (nbuckets <= 0) 
    resize(7);
  unsigned int h = hashcode(n);
  int i = h % nbuckets;
  struct sym *r = buckets[i];
  while (r && strcmp(n,r->n))
    r = r->l;
  if (!r && create)
    {
      nelems += 1;
      r = new sym;
      r->h = h;
      r->l = buckets[i];
      r->n = new char [1+strlen(n)];
      strcpy(r->n, n);
      buckets[i] = r;
      if ( 2 * nelems > 3 * nbuckets)
        resize(2*nbuckets-1);
    }
  return r;
}
  
END_ANONYMOUS_NAMESPACE

static symtable_t *symbols;
 
const char *
miniexp_to_name(miniexp_t p)
{
  if (miniexp_symbolp(p))
    {
      struct symtable_t::sym *r;
      r = ((symtable_t::sym*)(((size_t)p)&~((size_t)3)));
      return r->n;
    }
  return 0;
}

miniexp_t 
miniexp_symbol(const char *name)
{
  struct symtable_t::sym *r;
  if (! symbols) 
    symbols = new symtable_t;
  r = symbols->lookup(name, true);
  return (miniexp_t)(((size_t)r)|((size_t)2));
}


/* -------------------------------------------------- */
/* MEMORY AND GARBAGE COLLECTION                      */
/* -------------------------------------------------- */

// A simple mark-and-sweep garbage collector.
//
// Memory is managed in chunks of nptrs_chunk pointers.
// The first two pointers are used to hold mark bytes for the rest.
// Chunks are carved from blocks of nptrs_block pointers.
//
// Dirty hack: The sixteen most recently created pairs are 
// not destroyed by automatic garbage collection, in order
// to preserve transient objects created in the course 
// of evaluating complicated expressions.

#define nptrs_chunk  (4*sizeof(void*))
#define sizeof_chunk (nptrs_chunk*sizeof(void*))
#define nptrs_block  (16384-8)
#define recentlog    (4)
#define recentsize   (1<<recentlog)

static inline char *
markbase(void **p)
{
  return (char*)(((size_t)p) & ~(sizeof_chunk-1));
}

static inline char *
markbyte(void **p)
{
  char *base = markbase(p);
  return base + ((p - (void**)base)>>1);
}

struct block_t {
  block_t *next;
  void **lo;
  void **hi;
  void *ptrs[nptrs_block];
};

static block_t *
new_block(void)
{
  block_t *b = new block_t;
  memset(b, 0, sizeof(block_t));
  b->lo = (void**)markbase(b->ptrs+nptrs_chunk-1);
  b->hi = (void**)markbase(b->ptrs+nptrs_block);
  return b;
}

static void
clear_marks(block_t *b)
{
  for (void** m=b->lo; m<b->hi; m+=nptrs_chunk)
    m[0] = m[1] = 0;
}

static void
collect_free(block_t *b, void **&freelist, int &count, bool destroy)
{
  for (void **m=b->lo; m<b->hi; m+=nptrs_chunk)
    {
      char *c = (char*)m;
      for (unsigned int i=1; i<nptrs_chunk/2; i++)
        if (! c[i])
          {
            miniobj_t *obj = (miniobj_t*)m[i+i];
            if (destroy && obj && m[i+i]==m[i+i+1]) 
              obj->destroy();
            m[i+i] = (void*)freelist;
            m[i+i+1] = 0;
            freelist = &m[i+i];
            count += 1;
          }
    }
}

static struct {
  int lock;
  int request;
  int debug;
  int      pairs_total;
  int      pairs_free;
  void   **pairs_freelist;
  block_t *pairs_blocks;
  int      objs_total;
  int      objs_free;
  void   **objs_freelist;
  block_t *objs_blocks;
  void **recent[recentsize];
  int    recentindex;
} gc;

static void
new_pair_block(void)
{
  int count = 0;
  block_t *b = new_block();
  b->next = gc.pairs_blocks;
  gc.pairs_blocks = b;
  clear_marks(b);
  collect_free(b, gc.pairs_freelist, count, false);
  gc.pairs_total += count;
  gc.pairs_free += count;
}

static void
new_obj_block(void)
{
  int count = 0;
  block_t *b = new_block();
  b->next = gc.objs_blocks;
  gc.objs_blocks = b;
  clear_marks(b);
  collect_free(b, gc.objs_freelist, count, false);
  gc.objs_total += count;
  gc.objs_free += count;
}

#if defined(__GNUC__) && (__GNUC__ >= 3)
static void gc_mark_object(void **v) __attribute__((noinline));
#else
static void gc_mark_object(void **v);
#endif

static bool
gc_mark_check(void *p)
{
  if (((size_t)p) & 2)
    return false;
  void **v = (void**)(((size_t)p) & ~(size_t)3); 
  if (! v)
    return false;
  char *m = markbyte(v);
  if (*m)
    return false;
  *m = 1;
  if (! (((size_t)p) & 1))
    return true;
  gc_mark_object((void**)v);
  return false;
}

static void
gc_mark_pair(void **v)
{
#ifndef MINIEXP_POINTER_REVERSAL
  // This is a simple recursive code.
  // Despite the tail recursion for the cdrs,
  // it consume a stack space that grows like
  // the longest chain of cars.
  for(;;)
    {
      if (gc_mark_check(v[0]))
        gc_mark_pair((void**)v[0]);
      if (! gc_mark_check(v[1]))
        break;
      v = (void**)v[1];
    }
#else
  // This is the classic pointer reversion code
  // It saves stack memory by temporarily reversing the pointers. 
  // This is a bit slower because of all these nonlocal writes.
  // But it could be useful for memory-starved applications.
  // That makes no sense for most uses of miniexp.
  // I leave the code here because of its academic interest.
  void **w = 0;
 docar:
  if (gc_mark_check(v[0]))
    { // reverse car pointer
      void **p = (void**)v[0];
      v[0] = (void*)w;
      w = (void**)(((size_t)v)|(size_t)1);
      v = p;
      goto docar;
    }
 docdr:
  if (gc_mark_check(v[1]))
    { // reverse cdr pointer
      void **p = (void**)v[1];
      v[1] = (void*)w;
      w = v;
      v = p;
      goto docar;
    }
 doup:
  if (w)
    {
      if (((size_t)w)&1)
        { // undo car reversion
          void **p = (void**)(((size_t)w)&~(size_t)1);
          w = (void**)p[0];
          p[0] = (void*)v;
          v = p;
          goto docdr;
        }
      else
        { // undo cdr reversion
          void **p = w;
          w = (void**)p[1];
          p[1] = (void*)v;
          v = p;
          goto doup;
        }
    }
#endif
}

static void
gc_mark(miniexp_t *pp)
{
  void **v = (void**)*pp;
  if (gc_mark_check((void**)*pp))
    gc_mark_pair(v);
}

static void
gc_mark_object(void **v)
{
  ASSERT(v[0] == v[1]);
  miniobj_t *obj = (miniobj_t*)v[0];
  if (obj) 
    obj->mark(gc_mark);
}

static void
gc_run(void)
{
  gc.request++;
  if (gc.lock == 0)
    {
      block_t *b;
      gc.request = 0;
      // clear marks
      for (b=gc.objs_blocks; b; b=b->next)
        clear_marks(b);
      for (b=gc.pairs_blocks; b; b=b->next)
        clear_marks(b);
      // mark
      minivar_t::mark(gc_mark);
      for (int i=0; i<recentsize; i++)
        { // extra cast for strict aliasing rules?
          char *s = (char*)&gc.recent[i];
          gc_mark((miniexp_t*)s);
        }
      // sweep
      gc.objs_free = gc.pairs_free = 0;
      gc.objs_freelist = gc.pairs_freelist = 0;
      for (b=gc.objs_blocks; b; b=b->next)
        collect_free(b, gc.objs_freelist, gc.objs_free, true);
      for (b=gc.pairs_blocks; b; b=b->next)
        collect_free(b, gc.pairs_freelist, gc.pairs_free, false);
      // alloc 33% extra space
      while (gc.objs_free*4 < gc.objs_total)
        new_obj_block();
      while (gc.pairs_free*4 < gc.pairs_total)
        new_pair_block();
    }
}

static void **
gc_alloc_pair(void *a, void *d)
{
  if (!gc.pairs_freelist)
    {
      gc_run();
      if (!gc.pairs_freelist)
        new_pair_block();
    }
  else if (gc.debug)
    gc_run();
  void **p = gc.pairs_freelist;
  gc.pairs_freelist = (void**)p[0];
  gc.pairs_free -= 1;
  p[0] = a;
  p[1] = d;
  return p;
}

static void **
gc_alloc_object(void *obj)
{
  if (!gc.objs_freelist)
    {
      gc_run();
      if (!gc.objs_freelist)
        new_obj_block();
    }
  else if (gc.debug)
    gc_run();
  void **p = gc.objs_freelist;
  gc.objs_freelist = (void**)p[0];
  gc.objs_free -= 1;
  p[0] = p[1] = obj;
  return p;
}





/* ---- USER FUNCTIONS --- */

miniexp_t
minilisp_acquire_gc_lock(miniexp_t x)
{
  gc.lock++;
  return x;
}

miniexp_t
minilisp_release_gc_lock(miniexp_t x)
{
  if (gc.lock > 0)
    if (--gc.lock == 0)
      if (gc.request > 0)
        {
          minivar_t v = x;
          gc_run();
        }
  return x;
}

void 
minilisp_gc(void)
{
  int i;
  for (i=0; i<recentsize; i++)
    gc.recent[i] = 0;
  gc_run();
}

void 
minilisp_debug(int debug)
{
  gc.debug = debug;
}

void 
minilisp_info(void)
{
  time_t tim = time(0);
  const char *dat = ctime(&tim);
  printf("--- begin info -- %s", dat);
  printf("symbols: %d symbols in %d buckets\n", 
         symbols->nelems, symbols->nbuckets);
  if (gc.debug)
    printf("gc.debug: true\n");
  if (gc.lock)
    printf("gc.locked: true, %d requests\n", gc.request);
  printf("gc.pairs: %d free, %d total\n", gc.pairs_free, gc.pairs_total);
  printf("gc.objects: %d free, %d total\n", gc.objs_free, gc.objs_total);
  printf("--- end info -- %s", dat);
}


/* -------------------------------------------------- */
/* MINIVARS                                           */
/* -------------------------------------------------- */

minivar_t::minivar_t()
  : data(0)
{
  if ((next = vars))
    next->pprev = &next;
  pprev = &vars;
  vars = this;
}

minivar_t::minivar_t(miniexp_t p)
  : data(p)
{
  if ((next = vars))
    next->pprev = &next;
  pprev = &vars;
  vars = this;
}

minivar_t::minivar_t(const minivar_t &v)
  : data(v.data)
{
  if ((next = vars))
    next->pprev = &next;
  pprev = &vars;
  vars = this;
}

minivar_t *minivar_t::vars = 0;

void
minivar_t::mark(minilisp_mark_t *f)
{
  for (minivar_t *v = vars; v; v=v->next)
    (*f)(&v->data);
}

minivar_t *
minivar_alloc(void)
{
  return new minivar_t;
}

void 
minivar_free(minivar_t *v)
{
  delete v;
}

miniexp_t *
minivar_pointer(minivar_t *v)
{
  return &(*v);
}


/* -------------------------------------------------- */
/* LISTS                                              */
/* -------------------------------------------------- */

static inline miniexp_t &
car(miniexp_t p) {
  return ((miniexp_t*)p)[0];
}

static inline miniexp_t &
cdr(miniexp_t p) {
  return ((miniexp_t*)p)[1];
}

int 
miniexp_length(miniexp_t p)
{
  int n = 0;
  bool toggle = false;
  miniexp_t q = p;
  while (miniexp_consp(p))
    {
      p = cdr(p);
      if (p == q)
        return -1;
      if ((toggle = !toggle))
        q = cdr(q);
      n += 1;
    }
  return n;
}

miniexp_t 
miniexp_caar(miniexp_t p)
{
  return miniexp_car(miniexp_car(p)); 
}

miniexp_t 
miniexp_cadr(miniexp_t p)
{
  return miniexp_car(miniexp_cdr(p)); 
}

miniexp_t 
miniexp_cdar(miniexp_t p)
{
  return miniexp_cdr(miniexp_car(p)); 
}

miniexp_t 
miniexp_cddr(miniexp_t p)
{
  return miniexp_cdr(miniexp_cdr(p)); 
}

miniexp_t 
miniexp_caddr(miniexp_t p)
{
  return miniexp_car(miniexp_cdr(miniexp_cdr(p)));
}

miniexp_t 
miniexp_cdddr(miniexp_t p)
{
  return miniexp_cdr(miniexp_cdr(miniexp_cdr(p)));
}

miniexp_t 
miniexp_nth(int n, miniexp_t l)
{
  while (--n>=0 && miniexp_consp(l)) 
    l = cdr(l);
  return miniexp_car(l);
}

miniexp_t 
miniexp_cons(miniexp_t a, miniexp_t d)
{
  miniexp_t r = (miniexp_t)gc_alloc_pair((void*)a, (void*)d); 
  gc.recent[(++gc.recentindex) & (recentsize-1)] = (void**)r;
  return r;
}

miniexp_t 
miniexp_rplaca(miniexp_t pair, miniexp_t newcar)
{
  if (miniexp_consp(pair))
    {
      car(pair) = newcar;
      return pair;
    }
  return 0;
}

miniexp_t 
miniexp_rplacd(miniexp_t pair, miniexp_t newcdr)
{
  if (miniexp_consp(pair))
    {
      cdr(pair) = newcdr;
      return pair;
    }
  return 0;
}

miniexp_t 
miniexp_reverse(miniexp_t p)
{
  miniexp_t l = 0;
  while (miniexp_consp(p))
    {
      miniexp_t q = cdr(p);
      cdr(p) = l;
      l = p;
      p = q;
    }
  return l;
}


/* -------------------------------------------------- */
/* MINIOBJ                                            */
/* -------------------------------------------------- */

miniobj_t::~miniobj_t()
{
}

const miniexp_t miniobj_t::classname = 0;

bool
miniobj_t::isa(miniexp_t) const
{
  return false;
}

void 
miniobj_t::mark(minilisp_mark_t*)
{
}

void 
miniobj_t::destroy()
{
  delete this;
}

char *
miniobj_t::pname() const
{
  const char *cname = miniexp_to_name(classof());
  char *res = new char[strlen(cname)+24];
  sprintf(res,"#%s:<%p>",cname,this);
  return res;
}

miniexp_t 
miniexp_object(miniobj_t *obj)
{
  void **v = gc_alloc_object((void*)obj);
  v = (void**)(((size_t)v)|((size_t)1));
  gc.recent[(++gc.recentindex) & (recentsize-1)] = v;
  return (miniexp_t)(v);
}

miniexp_t 
miniexp_classof(miniexp_t p) 
{
  miniobj_t *obj = miniexp_to_obj(p);
  if (obj) return obj->classof();
  return miniexp_nil;
}

miniexp_t 
miniexp_isa(miniexp_t p, miniexp_t c)
{
  miniobj_t *obj = miniexp_to_obj(p);
  if (obj && obj->isa(c))
    return obj->classof();
  return miniexp_nil;
}


/* -------------------------------------------------- */
/* STRINGS                                            */
/* -------------------------------------------------- */

BEGIN_ANONYMOUS_NAMESPACE

class ministring_t : public miniobj_t 
{
  MINIOBJ_DECLARE(ministring_t,miniobj_t,"string");
public:
  ~ministring_t();
  ministring_t(const char *s);
  ministring_t(char *s, bool steal);
  operator const char*() const { return s; }
  virtual char *pname() const;
private:
  char *s;
private:
  ministring_t(const ministring_t &);
  ministring_t& operator=(const ministring_t &);
};

MINIOBJ_IMPLEMENT(ministring_t,miniobj_t,"string");

ministring_t::~ministring_t()
{
  delete [] s;
}

ministring_t::ministring_t(const char *str) 
  : s(new char[strlen(str)+1])
{
  strcpy(s,str);
}

ministring_t::ministring_t(char *str, bool steal) 
  : s(str)
{
  ASSERT(steal);
}

END_ANONYMOUS_NAMESPACE

static bool
char_quoted(int c, bool eightbits)
{
  if (eightbits && c>=0x80)
    return false;
  if (c==0x7f || c=='\"' || c=='\\')
    return true;
  if (c>=0x20 && c<0x7f)
    return false;
  return true;
}

static void
char_out(int c, char* &d, int &n)
{
  n++;
  if (d) 
    *d++ = c;
}

static int
print_c_string(const char *s, char *d, bool eightbits)
{
  int c;
  int n = 0;
  char_out('\"', d, n);
  while ((c = (unsigned char)(*s++)))
    {
      if (char_quoted(c, eightbits))
        {
          char letter = 0;
          static const char *tr1 = "\"\\tnrbf";
          static const char *tr2 = "\"\\\t\n\r\b\f";
          for (int i=0; tr2[i]; i++)
            if (c == tr2[i])
              letter = tr1[i];
          char_out('\\', d, n);
          if (letter)
            char_out(letter, d, n);
          else
            {
              char_out('0'+ ((c>>6)&3), d, n);
              char_out('0'+ ((c>>3)&7), d, n);
              char_out('0'+ (c&7), d, n);
            }
          continue;
        }
      char_out(c, d, n);
    }
  char_out('\"', d, n);
  char_out(0, d, n);
  return n;
}

char *
ministring_t::pname() const
{
  int n = print_c_string(s, 0, true);
  char *d = new char[n];
  if (d) print_c_string(s, d, true);
  return d;
}

int 
miniexp_stringp(miniexp_t p)
{
  // SumatraPDF: don't execute code until asked to
  return miniexp_isa(p, miniexp_symbol("string")) ? 1 : 0;
}

const char *
miniexp_to_str(miniexp_t p)
{
  miniobj_t *obj = miniexp_to_obj(p);
  if (miniexp_stringp(p))
    return (const char*) * (ministring_t*) obj;
  return 0;
}

miniexp_t 
miniexp_string(const char *s)
{
  ministring_t *obj = new ministring_t(s);
  return miniexp_object(obj);
}

miniexp_t 
miniexp_substring(const char *s, int n)
{
  int l = strlen(s);
  n = (n < l) ? n : l;
  char *b = new char[n+1];
  strncpy(b, s, n);
  b[n] = 0;
  ministring_t *obj = new ministring_t(b, true);
  return miniexp_object(obj);
}

miniexp_t 
miniexp_concat(miniexp_t p)
{
  miniexp_t l = p;
  const char *s;
  int n = 0;
  if (miniexp_length(l) < 0)
    return miniexp_nil;
  for (p=l; miniexp_consp(p); p=cdr(p))
    if ((s = miniexp_to_str(car(p))))
      n += strlen(s);
  char *b = new char[n+1];
  char *d = b;
  for (p=l; miniexp_consp(p); p=cdr(p))
    if ((s = miniexp_to_str(car(p)))) {
      strcpy(d, s);
      d += strlen(d);
    }
  ministring_t *obj = new ministring_t(b, true);
  return miniexp_object(obj);
}


/* -------------------------------------------------- */
/* INPUT/OUTPUT                                       */
/* -------------------------------------------------- */

BEGIN_ANONYMOUS_NAMESPACE

struct CompatCounter 
{
  static int count;
  CompatCounter() { count += 1; }
  ~CompatCounter() { count -= 1; }
};

int CompatCounter::count = 0;

END_ANONYMOUS_NAMESPACE

static int
compat_puts(const char *s)
{
  CompatCounter count;
  return miniexp_io.fputs(&miniexp_io, s);
}

static int
compat_getc()
{
  CompatCounter count;
  return miniexp_io.fgetc(&miniexp_io);
}

static int
compat_ungetc(int c)
{
  CompatCounter count;
  return miniexp_io.ungetc(&miniexp_io, c);
}

static int 
stdio_fputs(miniexp_io_t *io, const char *s)
{
  if (io == &miniexp_io && !CompatCounter::count)
    return (*minilisp_puts)(s); /* compatibility hack */
  FILE *f = (io->data[1]) ? (FILE*)(io->data[1]) : stdout;
  return ::fputs(s, f);
}

static int 
stdio_fgetc(miniexp_io_t *io)
{
  if (io == &miniexp_io && !CompatCounter::count)
    return (*minilisp_getc)(); /* compatibility hack */
  FILE *f = (io->data[0]) ? (FILE*)(io->data[0]) : stdin;
  return ::getc(f);
}

static int 
stdio_ungetc(miniexp_io_t *io, int c)
{
  if (io == &miniexp_io && !CompatCounter::count)
    return (*minilisp_ungetc)(c); /* compatibility hack */
  FILE *f = (io->data[0]) ? (FILE*)(io->data[0]) : stdin;
  return ::ungetc(c, f);
}

extern "C" 
{ 
  // SunCC needs this to be defined inside extern "C" { ... }
  // Beware the difference between extern "C" {...} and extern "C".
  miniexp_t (*minilisp_macrochar_parser[128])(void);
  miniexp_t (*minilisp_diezechar_parser[128])(void);
  minivar_t minilisp_macroqueue;
  int minilisp_print_7bits;
}


miniexp_io_t miniexp_io = { 
  stdio_fputs, stdio_fgetc, stdio_ungetc, { 0, 0, 0, 0 },
  (int*)&minilisp_print_7bits,
  (miniexp_macrochar_t*)minilisp_macrochar_parser, 
  (miniexp_macrochar_t*)minilisp_diezechar_parser, 
  (minivar_t*)&minilisp_macroqueue,
  0
};  

int (*minilisp_puts)(const char *) = compat_puts;
int (*minilisp_getc)(void) = compat_getc;
int (*minilisp_ungetc)(int) = compat_ungetc;

void 
miniexp_io_init(miniexp_io_t *io)
{
  io->fputs = stdio_fputs;
  io->fgetc = stdio_fgetc;
  io->ungetc = stdio_ungetc;
  io->data[0] = io->data[1] = io->data[2] = io->data[3] = 0;
  io->p_print7bits = (int*)&minilisp_print_7bits;;
  io->p_macrochar = (miniexp_macrochar_t*)minilisp_macrochar_parser;
  io->p_diezechar = (miniexp_macrochar_t*)minilisp_diezechar_parser;
  io->p_macroqueue = (minivar_t*)&minilisp_macroqueue;
  io->p_reserved = 0;
}

void 
miniexp_io_set_output(miniexp_io_t* io, FILE *f)
{
  io->fputs = stdio_fputs;
  io->data[1] = f;
}

void 
miniexp_io_set_input(miniexp_io_t* io, FILE *f)
{
  io->fgetc = stdio_fgetc;
  io->ungetc = stdio_ungetc;
  io->data[0] = f;
}


/* ---- OUTPUT */

BEGIN_ANONYMOUS_NAMESPACE

struct printer_t 
{
  int tab;
  bool dryrun;
  miniexp_io_t *io;
  printer_t(miniexp_io_t *io) : tab(0), dryrun(false), io(io) {}
  void mlput(const char *s);
  void mltab(int n);
  void print(miniexp_t p);
  bool must_quote_symbol(const char *s);
  virtual miniexp_t begin() { return miniexp_nil; }
  virtual bool newline() { return false; }
  virtual void end(miniexp_t) { }
  virtual ~printer_t() {};
};

void
printer_t::mlput(const char *s)
{
  if (! dryrun)
    io->fputs(io, s);
  while (*s)
    if (*s++ == '\n')
      tab = 0;
    else
      tab += 1;
}

void
printer_t::mltab(int n)
{
  while (tab+8 <= n)
    mlput("        ");
  while (tab+1 <= n)
    mlput(" ");
}

bool
printer_t::must_quote_symbol(const char *s)
{
  int c;
  const char *r = s;
  while ((c = *r++))
    if (c=='(' || c==')' || c=='\"' || c=='|' || 
        isspace(c) || !isascii(c) || !isprint(c) ||
        (c >= 0 && c < 128 && io->p_macrochar && io->p_macrochar[c]) )
      return true;
  char *end;
#ifdef __GNUC__
  long junk __attribute__ ((unused)) =
#endif
  strtol(s, &end, 0);
  return (!*end);
}

void
printer_t::print(miniexp_t p)
{
  static char buffer[32];
  miniexp_t b = begin();
  if (p == miniexp_nil)
    {
      mlput("()");
    }
  else if (p == miniexp_dummy)
    {
      mlput("#dummy");
    }
  else if (miniexp_numberp(p))
    {
      sprintf(buffer, "%d", miniexp_to_int(p));
      mlput(buffer);
    }
  else if (miniexp_symbolp(p))
    {
      const char *s = miniexp_to_name(p);
      bool needquote = must_quote_symbol(s);
      if (needquote) mlput("|");
      mlput(s);
      if (needquote) mlput("|");
    }
  else if (miniexp_stringp(p))
    {
      const char *s = miniexp_to_str(p);
      bool print7bits = (io->p_print7bits && *io->p_print7bits);
      int n = print_c_string(s, 0, !print7bits);
      char *d = new char[n];
      if (d) print_c_string(s, d, !print7bits);
      mlput(d);
      delete [] d;
    }
  else if (miniexp_objectp(p))
    {
      miniobj_t *obj = miniexp_to_obj(p);
      char *s = obj->pname();
      mlput(s);
      delete [] s;
    }
  else if (miniexp_listp(p))
    {
      // TODO - detect more circular structures
      int skip = 1;
      int indent = tab + 1;
      bool multiline = false;
      bool toggle = true;
      miniexp_t q = p;
      mlput("(");
      if (miniexp_consp(p) && miniexp_symbolp(car(p)))
        {
          skip++;
          indent++;
        }
      while (miniexp_consp(p))
        {
          skip -= 1;
	  if (multiline || (newline() && skip<0 && tab>indent))
            {
              mlput("\n"); 
              mltab(indent); 
              multiline=true; 
            }
          print(car(p));
          if ((p = cdr(p)))
            mlput(" ");
          if ((toggle = !toggle))
            q = cdr(q);
          if (p == q)
            {
              mlput("...");
              p = 0;
            }
        }
      if (p)
        {
          skip -= 1;
	  if (multiline || (newline() && skip<0 && tab>indent))
            {
              mlput("\n"); 
              mltab(indent); 
              multiline=true; 
            }
          mlput(". ");
          print(p);
        }
      if (multiline)
        mlput(" )");
      else
        mlput(")");
    }
  end(b);
}

struct pprinter_t : public printer_t 
{
  int width;
  minivar_t l;
  pprinter_t(miniexp_io_t *io) : printer_t(io) {}
  virtual miniexp_t begin();
  virtual bool newline();
  virtual void end(miniexp_t);
};

miniexp_t 
pprinter_t::begin()
{
  if (dryrun)
    {
      l = miniexp_cons(miniexp_number(tab), l);
      return l;
    }
  else
    {
      ASSERT(miniexp_consp(l));
      ASSERT(miniexp_numberp(car(l)));
      l = cdr(l);
      return miniexp_nil;
    }
}

bool 
pprinter_t::newline()
{
  if (! dryrun)
    {
      ASSERT(miniexp_consp(l));
      ASSERT(miniexp_numberp(car(l)));
      int len = miniexp_to_int(car(l));
      if (tab + len >= width)
        return true;
    }
  return false;
}

void 
pprinter_t::end(miniexp_t p)
{
  if (dryrun)
    {
      ASSERT(miniexp_consp(p));
      ASSERT(miniexp_numberp(car(p)));
      int pos = miniexp_to_int(car(p));
      ASSERT(tab >= pos);
      car(p) = miniexp_number(tab - pos);
    }
}

END_ANONYMOUS_NAMESPACE

miniexp_t 
miniexp_prin_r(miniexp_io_t *io, miniexp_t p)
{
  minivar_t xp = p;
  printer_t printer(io);
  printer.print(p);
  return p;
}

miniexp_t 
miniexp_print_r(miniexp_io_t *io, miniexp_t p)
{
  minivar_t xp = p;
  miniexp_prin_r(io, p);
  io->fputs(io, "\n");
  return p;
}

miniexp_t 
miniexp_pprin_r(miniexp_io_t *io, miniexp_t p, int width)
{  
  minivar_t xp = p;
  pprinter_t printer(io);
  printer.width = width;
  // step1 - measure lengths into list <l>
  printer.tab = 0;
  printer.dryrun = true;
  printer.print(p);
  // step2 - print
  printer.tab = 0;
  printer.dryrun = false;
  printer.l = miniexp_reverse(printer.l);
  printer.print(p);
  // check
  ASSERT(printer.l == 0);
  return p;
}

miniexp_t 
miniexp_pprint_r(miniexp_io_t *io, miniexp_t p, int width)
{
  miniexp_pprin_r(io, p, width);
  io->fputs(io, "\n");
  return p;
}


/* ---- PNAME */

static int
pname_fputs(miniexp_io_t *io, const char *s)
{
  char *b = (char*)(io->data[0]);
  size_t l = (size_t)(io->data[2]);
  size_t m = (size_t)(io->data[3]);
  size_t x = strlen(s);
  if (l + x >= m)
    {
      size_t nm = l + x + 256;
      char *nb = new char[nm+1];
      memcpy(nb, b, l);
      delete [] b;
      b = nb;
      m = nm;
    }
  strcpy(b + l, s);
  io->data[0] = (void*)(b);
  io->data[2] = (void*)(l + x);
  io->data[3] = (void*)(m);
  return x;
}

miniexp_t 
miniexp_pname(miniexp_t p, int width)
{
  minivar_t r;
  miniexp_io_t io;
  miniexp_io_init(&io);
  io.fputs = pname_fputs;
  io.data[0] = io.data[2] = io.data[3] = 0;
  try
    {
      if (width > 0)
        miniexp_pprin_r(&io, p, width);
      else
        miniexp_prin_r(&io, p);
      if (io.data[0])
        r = miniexp_string((const char*)io.data[0]);
      delete [] (char*)(io.data[0]);
    }
  catch(...)
    {
      delete [] (char*)(io.data[0]);
    }
  return r;
}


/* ---- INPUT */

static void
append(int c, char* &s, int &l, int &m)
{
  if (l >= m)
    {
      int nm = ((m<256)?256:m) + ((m>32000)?32000:m);
      char *ns = new char[nm+1];
      memcpy(ns, s, l);
      delete [] s;
      m = nm;
      s = ns;
    }
  s[l++] = c;
  s[l] = 0;
}

static void
skip_blank(miniexp_io_t *io, int &c)
{
  while (isspace(c))
    c = io->fgetc(io);
}

static miniexp_t
read_error(miniexp_io_t *io, int &c)
{
  while (c!=EOF && c!='\n')
    c = io->fgetc(io);
  return miniexp_dummy;
}

static miniexp_t
read_c_string(miniexp_io_t *io, int &c)
{
  miniexp_t r;
  char *s = 0;
  int l = 0;
  int m = 0;
  ASSERT(c == '\"');
  c = io->fgetc(io);
  for(;;)
    {
      if (c==EOF || (isascii(c) && !isprint(c)))
        return read_error(io, c);
      else if (c=='\"')
        break;
      else if (c=='\\')
        {
          c = io->fgetc(io);
          if (c == '\n')             // LF
            {
              c = io->fgetc(io);
              if (c == '\r')         // LFCR
                c = io->fgetc(io);
              continue;
            }
          else if (c == '\r')        // CR
            {
              c = io->fgetc(io);
              if (c == '\n')         // CRLF
                c = io->fgetc(io);
              continue;
            }
          else if (c>='0' && c<='7')
            {
              int x = (c-'0');
              c = io->fgetc(io);
              if (c>='0' && c<='7')
                {
                  x = (x<<3)+(c-'0');
                  c = io->fgetc(io);
                  if (c>='0' && c<='7')
                    {
                      x = (x<<3)+(c-'0');
                      c = io->fgetc(io);
                    }
                }
              append((char)x, s, l, m);
              continue;
            }
          else if (c=='x' || c=='X')
            {
              int x = 0;
              int d = c;
              c = io->fgetc(io);
              if (isxdigit(c))
                {
                  x = (x<<4) + (isdigit(c) ? c-'0' : toupper(c)-'A'+10);
                  c = io->fgetc(io);
                  if (isxdigit(c))
                    {
                      x = (x<<4) + (isdigit(c) ? c-'0' : toupper(c)-'A'+10);
                      c = io->fgetc(io);
                    }
                  append((char)x, s, l, m);
                  continue;
                }
              else
                {
                  io->ungetc(io, c);
                  c = d;
                }
            }
          static const char *tr1 = "tnrbfva";
          static const char *tr2 = "\t\n\r\b\f\013\007";
          for (int i=0; tr1[i]; i++)
            if (c == tr1[i])
              c = tr2[i];
        }
      append(c,s,l,m);
      c = io->fgetc(io);
    }
  c = io->fgetc(io);
  r = miniexp_string(s ? s : "");
  delete [] s;
  return r;
}

static miniexp_t
read_quoted_symbol(miniexp_io_t *io, int &c)
{
  miniexp_t r;
  char *s = 0;
  int l = 0;
  int m = 0;
  ASSERT(c == '|');
  for(;;)
    {
      c = io->fgetc(io);
      if (c==EOF || (isascii(c) && !isprint(c)))
        return read_error(io, c);
      if (c=='|')
        break;
      append(c,s,l,m);
    }
  c = io->fgetc(io);
  r = miniexp_symbol(s ? s : "");
  delete [] s;
  return r;
}

static miniexp_t
read_symbol_or_number(miniexp_io_t *io, int &c)
{
  miniexp_t r;
  char *s = 0;
  int l = 0;
  int m = 0;
  for(;;)
    {
      if (c==EOF || c=='(' || c==')' || c=='|' || c=='\"'  
          || isspace(c) || !isascii(c) || !isprint(c) 
          || (io->p_macrochar && io->p_macroqueue  
              && c < 128 && c >= 0 && io->p_macrochar[c] ) )
        break;
      append(c,s,l,m);
      c = io->fgetc(io);
    }
  if (l <= 0)
    return read_error(io, c);
  char *end;
  long x = strtol(s, &end, 0);
  if (*end)
    r = miniexp_symbol(s);
  else
    r = miniexp_number((int)x);
  delete [] s;
  return r;
}

static miniexp_t
read_miniexp(miniexp_io_t *io, int &c)
{
  for(;;)
    {
      if (io->p_macroqueue && miniexp_consp(*io->p_macroqueue))
        {
          miniexp_t p = car(*io->p_macroqueue);
          *io->p_macroqueue = cdr(*io->p_macroqueue);
          return p;
        }
      skip_blank(io, c);
      if (c == EOF)
        {
          return read_error(io, c);
        }
      else if (c == ')')
        {
          c = io->fgetc(io);
          continue;
        }
      else if (c == '(')
        {
          minivar_t l;
          miniexp_t *where = &l;
          minivar_t p;
          c = io->fgetc(io);
          for(;;)
            {
              skip_blank(io, c);
              if (c == ')')
                break;
              if (c == '.')
                {
                  int d = io->fgetc(io);
                  io->ungetc(io, d);
                  if (isspace(d)) 
                    break;
                }
              p = read_miniexp(io, c);
              if ((miniexp_t)p == miniexp_dummy)
                return miniexp_dummy;
              *where = miniexp_cons(p, miniexp_nil);
              where = &cdr(*where);
            }
          if (c == '.')
            {
              c = io->fgetc(io);
              skip_blank(io, c);
              if (c != ')')
                *where = read_miniexp(io, c);
            }
          skip_blank(io, c);
          if (c != ')')
            return read_error(io, c);
          c = io->fgetc(io);
          return l;
        }
      else if (c == '"')
        {
          return read_c_string(io, c);
        }
      else if (c == '|')
        {
          return read_quoted_symbol(io, c);
        }
      else if (io->p_macrochar && io->p_macroqueue 
               && c >= 0 && c < 128 && io->p_macrochar[c])
        {
          miniexp_t p = io->p_macrochar[c](io);
          if (miniexp_length(p) > 0)
            *io->p_macroqueue = p;
          c = io->fgetc(io);
          continue;
        }
      else if (c == '#' && io->p_diezechar && io->p_macroqueue)
        {
          int nc = io->fgetc(io);
          if (nc >= 0 && nc < 128 && io->p_diezechar[nc])
            {
              miniexp_t p = io->p_macrochar[nc](io);
              if (miniexp_length(p) > 0)
                *io->p_macroqueue = p;
              c = io->fgetc(io);
              continue;
            }
          io->ungetc(io, nc);
          // fall thru
        }
      // default
      return read_symbol_or_number(io, c);
    }
}

miniexp_t 
miniexp_read_r(miniexp_io_t *io)
{
  int c = io->fgetc(io);
  miniexp_t p = read_miniexp(io, c);
  io->ungetc(io, c);
  return p;
}


/* ---- COMPAT */

miniexp_t miniexp_read(void)
{
  return miniexp_read_r(&miniexp_io);
}

miniexp_t miniexp_prin(miniexp_t p)
{
  return miniexp_prin_r(&miniexp_io, p);
}

miniexp_t miniexp_print(miniexp_t p)
{
  return miniexp_print_r(&miniexp_io, p);
}

miniexp_t miniexp_pprin(miniexp_t p, int w)
{
  return miniexp_pprin_r(&miniexp_io, p, w);
}

miniexp_t miniexp_pprint(miniexp_t p, int w)
{
  return miniexp_pprint_r(&miniexp_io, p, w);
}

void 
minilisp_set_output(FILE *f)
{
  minilisp_puts = compat_puts;
  miniexp_io_set_output(&miniexp_io, f);
}

void 
minilisp_set_input(FILE *f)
{
  minilisp_getc = compat_getc;
  minilisp_ungetc = compat_ungetc;
  miniexp_io_set_input(&miniexp_io, f);
}




/* -------------------------------------------------- */
/* CLEANUP (SEE GC ABOVE)                             */
/* -------------------------------------------------- */

static void
gc_clear(miniexp_t *pp)
{
  *pp = 0;
}

void
minilisp_finish(void)
{
  ASSERT(!gc.lock);
  // clear minivars
  minivar_t::mark(gc_clear);
  for (int i=0; i<recentsize; i++)
    gc.recent[i] = 0;
  // collect everything
  gc_run();
  // deallocate mblocks
  ASSERT(gc.pairs_free == gc.pairs_total);
  while (gc.pairs_blocks)
    {
      block_t *b = gc.pairs_blocks;
      gc.pairs_blocks = b->next;
      delete b;
    }
  ASSERT(gc.objs_free == gc.objs_total);
  while (gc.objs_blocks)
    {
      block_t *b = gc.objs_blocks;
      gc.objs_blocks = b->next;
      delete b;
    }
  // deallocate symbol table
  delete symbols;
}


