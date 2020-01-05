/* -*- C++ -*-
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
#include <errno.h>
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
/* GLOBAL MUTEX                                       */
/* -------------------------------------------------- */

#ifndef WITHOUT_THREADS
# ifdef _WIN32
#  include <windows.h>
#  define USE_WINTHREADS 1
# elif defined(HAVE_PTHREAD)
#  include <pthread.h>
#  define USE_PTHREADS 1 
# endif
#endif

#if defined(USE_WINTHREADS)
// Windows critical section
# define CSLOCK(name) CSLocker name
BEGIN_ANONYMOUS_NAMESPACE
struct CS { 
  CRITICAL_SECTION cs; 
  CS() { InitializeCriticalSection(&cs); }
  ~CS() { DeleteCriticalSection(&cs); } };
static CS globalCS;
struct CSLocker {
  CSLocker() { EnterCriticalSection(&globalCS.cs); }
  ~CSLocker() { LeaveCriticalSection(&globalCS.cs); } };
END_ANONYMOUS_NAMESPACE

#elif defined (USE_PTHREADS)
// Pthread critical section
# define CSLOCK(name) CSLocker name
BEGIN_ANONYMOUS_NAMESPACE
static pthread_mutex_t globalCS = PTHREAD_MUTEX_INITIALIZER;
struct CSLocker {
  CSLocker() { pthread_mutex_lock(&globalCS); }
  ~CSLocker() { pthread_mutex_unlock(&globalCS); } };
END_ANONYMOUS_NAMESPACE
  
#else
// No critical sections
# define CSLOCK(name) /**/
#endif


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
  unsigned int h = hashcode(n);
  int i = h % nbuckets;
  struct sym *r = buckets[i];
  while (r && strcmp(n,r->n))
    r = r->l;
  if (!r && create)
    {
      CSLOCK(lock);
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
      return (r) ? r->n : "##(dummy)";
    }
  return 0;
}

miniexp_t 
miniexp_symbol(const char *name)
{
  struct symtable_t::sym *r;
  if (! symbols) 
    {
      CSLOCK(lock);
      if (! symbols)
    symbols = new symtable_t;
    }
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

BEGIN_ANONYMOUS_NAMESPACE

struct gctls_t {
  gctls_t  *next;
  gctls_t **pprev;
  void    **recent[recentsize];
  int       recentindex;
  gctls_t();
  ~gctls_t();
};

struct block_t 
{
  block_t *next;
  void **lo;
  void **hi;
  void *ptrs[nptrs_block];
};

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
  gctls_t *tls;
} gc;

gctls_t::gctls_t()
{
  // CSLOCK(locker); [already locked]
  recentindex = 0;
  for (int i=0; i<recentsize; i++)
    recent[i] = 0;
  if ((next = gc.tls))
    next->pprev = &next;
  pprev = &gc.tls;
  gc.tls = this;
  //fprintf(stderr,"Created gctls %p\n", this);
}

gctls_t::~gctls_t()
{
  //CSLOCK(locker); [already locked]
  //fprintf(stderr,"Deleting gctls %p\n", this);
  if  ((*pprev = next))
    next->pprev = pprev;
}

END_ANONYMOUS_NAMESPACE

#if USE_PTHREADS

// Manage thread specific data with pthreads
static pthread_key_t gctls_key;
static pthread_once_t gctls_once;
static void gctls_destroy(void* arg) {
  CSLOCK(locker); delete (gctls_t*)arg;
}
static void gctls_key_alloc() {
  pthread_key_create(&gctls_key, gctls_destroy);
}
# if HAVE_GCCTLS
static __thread gctls_t *gctls_tv = 0;
static void gctls_alloc() {
  pthread_once(&gctls_once, gctls_key_alloc);
  gctls_tv = new gctls_t();
  pthread_setspecific(gctls_key, (void*)gctls_tv);
}
static gctls_t *gctls() {
  if (! gctls_tv) gctls_alloc();
  return gctls_tv;
}
# else
static  gctls_t *gctls_alloc() {
  gctls_t *res = new gctls_t();
  pthread_setspecific(gctls_key, (void*)res);
  return res;
}
static gctls_t *gctls() {
  pthread_once(&gctls_once, gctls_key_alloc);
  void *arg = pthread_getspecific(gctls_key);
  return (arg) ? (gctls_t*)(arg) : gctls_alloc();
}
# endif

#elif USE_WINTHREADS 

// Manage thread specific data with win32
#if defined(_MSC_VER) && defined(USE_MSVC_TLS)
// -- Pre-vista os sometimes crashes on this.
static __declspec(thread) gctls_t *gctls_tv = 0;
static gctls_t *gctls() {
  if (! gctls_tv)  gctls_tv = new gctls_t();
  return gctls_tv;
}
static void NTAPI gctls_cb(PVOID, DWORD dwReason, PVOID) {
  if (dwReason == DLL_THREAD_DETACH && gctls_tv) 
    { CSLOCK(locker); delete gctls_tv; gctls_tv=0; } }
# else
// -- Using Tls{Alloc,SetValue,GetValue,Free} instead.
static DWORD tlsIndex = TLS_OUT_OF_INDEXES;
static gctls_t *gctls() {
  if (tlsIndex == TLS_OUT_OF_INDEXES) tlsIndex = TlsAlloc();
  ASSERT(tlsIndex != TLS_OUT_OF_INDEXES);
  gctls_t *addr = (gctls_t*)TlsGetValue(tlsIndex);
  if (! addr) TlsSetValue(tlsIndex, (LPVOID)(addr = new gctls_t()));
  ASSERT(addr != 0);
  return addr;
}
static void NTAPI gctls_cb(PVOID, DWORD dwReason, PVOID) {
  if (dwReason == DLL_THREAD_DETACH && tlsIndex != TLS_OUT_OF_INDEXES)
    {CSLOCK(r);delete(gctls_t*)TlsGetValue(tlsIndex);TlsSetValue(tlsIndex,0);}
  if (dwReason == DLL_PROCESS_DETACH && tlsIndex != TLS_OUT_OF_INDEXES)
    {CSLOCK(r);TlsFree(tlsIndex);tlsIndex=TLS_OUT_OF_INDEXES;}
}
# endif
// -- Very black magic to clean the TLS variables
# if !defined(_MSC_VER)
#  warning "This only works with MSVC. Memory leak otherwise"
# elif !defined(MINILISPAPI_EXPORT)
#  pragma message("This only works for a DLL. Memory leak otherwise")
# else
#  ifdef _M_IX86
#   pragma comment (linker, "/INCLUDE:_tlscb")
#  else
#   pragma comment (linker, "/INCLUDE:tlscb")
#  endif
#  pragma const_seg(".CRT$XLB")
extern "C" PIMAGE_TLS_CALLBACK tlscb = gctls_cb;
#  pragma const_seg()
# endif

#else

// No threads
static gctls_t *gctls() {
  static gctls_t g;
  return &g;
}

#endif

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
      // mark recents
      for (gctls_t *tls = gc.tls; tls; tls=tls->next)
        for (int i=0; i<recentsize; i++)
          gc_mark((miniexp_t*)(char*)&(tls->recent[i]));
      // mark roots
      minivar_t::mark(gc_mark);
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
  CSLOCK(locker);
  gc.lock++;
  return x;
}

miniexp_t
minilisp_release_gc_lock(miniexp_t x)
{
  minivar_t v = x;
  {
    CSLOCK(locker);
  if (gc.lock > 0)
    if (--gc.lock == 0)
      if (gc.request > 0)
          gc_run();
        }
  return x;
}

void 
minilisp_gc(void)
{
  CSLOCK(locker);
  for (gctls_t *tls = gc.tls; tls; tls=tls->next)
    for (int i=0; i<recentsize; i++)
      tls->recent[i] = 0;
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
  CSLOCK(locker);
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

miniexp_t
miniexp_mutate(miniexp_t, miniexp_t *var, miniexp_t val)
{
  CSLOCK(locker);
  *var = val;
  return val;
}


/* -------------------------------------------------- */
/* MINIVARS                                           */
/* -------------------------------------------------- */

minivar_t::minivar_t()
  : data(0)
{
  CSLOCK(locker);
  if ((next = vars))
    next->pprev = &next;
  pprev = &vars;
  vars = this;
}

minivar_t::minivar_t(miniexp_t p)
  : data(p)
{
  CSLOCK(locker);
  if ((next = vars))
    next->pprev = &next;
  pprev = &vars;
  vars = this;
}

minivar_t::minivar_t(const minivar_t &v)
  : data(v.data)
{
  CSLOCK(locker);
  if ((next = vars))
    next->pprev = &next;
  pprev = &vars;
  vars = this;
}

minivar_t::~minivar_t()
{ 
  CSLOCK(locker);
  if ((*pprev = next)) 
    next->pprev = pprev; 
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
  CSLOCK(locker);
  miniexp_t r = (miniexp_t)gc_alloc_pair((void*)a, (void*)d); 
  gctls_t *tls = gctls();
  tls->recent[(++(tls->recentindex)) & (recentsize-1)] = (void**)r;
  return r;
}

miniexp_t 
miniexp_rplaca(miniexp_t pair, miniexp_t newcar)
{
  if (miniexp_consp(pair))
    return miniexp_mutate(pair, &car(pair), newcar);
  return 0;
}

miniexp_t 
miniexp_rplacd(miniexp_t pair, miniexp_t newcdr)
{
  if (miniexp_consp(pair))
    return miniexp_mutate(pair, &cdr(pair), newcdr);
  return 0;
}

miniexp_t 
miniexp_reverse(miniexp_t p)
{
  miniexp_t l = 0;
  while (miniexp_consp(p))
    {
      miniexp_t q = cdr(p);
      miniexp_mutate(p, &cdr(p), l);
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

bool
miniobj_t::stringp(const char* &, size_t &) const
{
  return false;
}

bool
miniobj_t::doublep(double&) const
{
  return false;
}

miniexp_t 
miniexp_object(miniobj_t *obj)
{
  CSLOCK(locker);
  void **v = gc_alloc_object((void*)obj);
  v = (void**)(((size_t)v)|((size_t)1));
  gctls_t *tls = gctls();
  tls->recent[(++(tls->recentindex)) & (recentsize-1)] = (void**)v;
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
  ministring_t(size_t len, const char *s);
  ministring_t(size_t len, char *s, bool steal);
  operator const char*() const { return s; }
  virtual bool stringp(const char* &s, size_t &l) const;
private:
  char *s;
  size_t l;
private:
  ministring_t(const ministring_t &);
  ministring_t& operator=(const ministring_t &);
};

MINIOBJ_IMPLEMENT(ministring_t,miniobj_t,"string");

ministring_t::~ministring_t()
{
  delete [] s;
}

ministring_t::ministring_t(size_t len, const char *str)
  : s(0), l(len)
{
  s = new char[l+1];
  memcpy(s, str, l);
  s[l] = 0;
}

ministring_t::ministring_t(size_t len, char *str, bool steal)
  : s(str), l(len)
{
  ASSERT(steal);
}

bool
ministring_t::stringp(const char* &rs, size_t &rl) const
{
  rs = s;
  rl = l;
  return true;
}


END_ANONYMOUS_NAMESPACE

static bool
char_quoted(int c, int flags)
{
  bool print7bits = (flags & miniexp_io_print7bits);
  if (c>=0x80 && !print7bits)
    return false;
  if (c==0x7f || c=='\"' || c=='\\')
    return true;
  if (c>=0x20 && c<0x7f)
    return false;
  return true;
}

static bool
char_utf8(int &c, const char* &s, size_t &len)
{
  if (c < 0xc0)
    return (c < 0x80);
  if (c >= 0xf8)
    return false;
  int n = (c < 0xe0) ? 1 : (c < 0xf0) ? 2 : 3;
  if ((size_t)n > len)
    return false;
  int x = c & (0x3f >> n);
  for (int i=0; i<n; i++)
    if ((s[i] & 0xc0) == 0x80)
      x = (x << 6) + (s[i] & 0x3f);
    else
      return false;
  static int lim[] = {0, 0x80, 0x800, 0x10000};
  if (x < lim[n])
    return false;
  if (x > 0x10ffff)
    return false;
  if (x >= 0xd800 && x <= 0xdfff)
    return false;
  len -= n;
  s += n;
  c = x;
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
print_c_string(const char *s, char *d, int flags, size_t len)
{
  int c;
  int n = 0;
  char_out('\"', d, n);
  while (len-- > 0) 
    {
      c = (unsigned char)(*s++);
      if (char_quoted(c, flags))
        {
          char buffer[16]; /* 10+1 */
          static const char *tr1 = "\"\\tnrbf";
          static const char *tr2 = "\"\\\t\n\r\b\f";
          buffer[0] = buffer[1] = 0;
          char_out('\\', d, n);
          for (int i=0; tr2[i]; i++)
            if (c == tr2[i])
              buffer[0] = tr1[i];
          if (buffer[0] == 0 && c >= 0x80 
              && (flags & (miniexp_io_u4escape | miniexp_io_u6escape))
              && char_utf8(c, s, len) )
            {
              if (c <= 0xffff && (flags & miniexp_io_u4escape))
                sprintf(buffer,"u%04X", c);
              else if (flags & miniexp_io_u6escape) // c# style
                sprintf(buffer,"U%06X", c);
              else if (flags & miniexp_io_u4escape) // json style
                sprintf(buffer,"u%04X\\u%04X", 
                        0xd800+(((c-0x10000)>>10)&0x3ff), 
                        0xdc00+(c&0x3ff));
            }
          if (buffer[0] == 0 && c == 0)
            if (*s < '0' || *s > '7')
              buffer[0] = '0';
          if (buffer[0] == 0)
            sprintf(buffer, "%03o", c);
          for (int i=0; buffer[i]; i++)
            char_out(buffer[i], d, n);
          continue;
        }
      char_out(c, d, n);
    }
  char_out('\"', d, n);
  char_out(0, d, n);
  return n;
}

int 
miniexp_stringp(miniexp_t p)
{
  const char *s; size_t l;
  if (miniexp_objectp(p) && miniexp_to_obj(p)->stringp(s,l))
    return 1;
  return 0;
}

const char *
miniexp_to_str(miniexp_t p)
{
  const char *s = 0;
  miniexp_to_lstr(p, &s);
  return s;
}

size_t
miniexp_to_lstr(miniexp_t p, const char **sp)
{
  const char *s = 0;
  size_t l = 0;
  if (miniexp_objectp(p))
    miniexp_to_obj(p)->stringp(s,l);
  if (sp)
    *sp = s;
  return l;
}

miniexp_t 
miniexp_string(const char *s)
{
  return miniexp_lstring(strlen(s), s);
}

miniexp_t 
miniexp_lstring(size_t len, const char *s)
{
  ministring_t *obj = new ministring_t(len,s);
  return miniexp_object(obj);
}

miniexp_t 
miniexp_substring(const char *s, int len)
{
  size_t l = strlen(s);
  size_t n = (size_t)len;
  return miniexp_lstring((l < n) ? l : n, s);
}

miniexp_t 
miniexp_concat(miniexp_t p)
{
  miniexp_t l = p;
  const char *s;
  size_t n = 0;
  if (miniexp_length(l) < 0)
    return miniexp_nil;
  for (p=l; miniexp_consp(p); p=cdr(p))
    n += miniexp_to_lstr(car(p), 0);
  char *b = new char[n+1];
  char *d = b;
  for (p=l; miniexp_consp(p); p=cdr(p))
    if ((n = miniexp_to_lstr(car(p), &s))) {
      memcpy(d, s, n);
      d += n;
    }
  ministring_t *obj = new ministring_t(d-b, b, true);
  return miniexp_object(obj);
}


/* -------------------------------------------------- */
/* FLOATNUMS                                          */
/* -------------------------------------------------- */


BEGIN_ANONYMOUS_NAMESPACE

class minifloat_t : public miniobj_t 
{
  MINIOBJ_DECLARE(minifloat_t,miniobj_t,"floatnum");
public:
  minifloat_t(double x) : val(x) {}
  operator double() const { return val; }
  virtual char *pname() const;
  virtual bool doublep(double &d) const { d=val; return true; }
private:
  double val;
};


MINIOBJ_IMPLEMENT(minifloat_t,miniobj_t,"floatnum");

END_ANONYMOUS_NAMESPACE

int 
miniexp_floatnump(miniexp_t p)
{
  return miniexp_isa(p, minifloat_t::classname) ? 1 : 0;
}

miniexp_t 
miniexp_floatnum(double x)
{
  minifloat_t *obj = new minifloat_t(x);
  return miniexp_object(obj);
}

int
miniexp_doublep(miniexp_t p)
{
  double v = 0.0;
  if (miniexp_numberp(p) ||
      (miniexp_objectp(p) && miniexp_to_obj(p)->doublep(v)) )
    return 1;
  return 0;
}

double 
miniexp_to_double(miniexp_t p)
{
  double v = 0.0;
  if (miniexp_numberp(p))
    v = (double) miniexp_to_int(p);
  else if (miniexp_objectp(p))
    miniexp_to_obj(p)->doublep(v);
  return v;
}

miniexp_t 
miniexp_double(double x)
{
  miniexp_t exp = miniexp_number((int)(x));
  if (x != (double)miniexp_to_int(exp))
    exp = miniexp_floatnum(x);
  return exp;
}

static bool
str_looks_like_double(const char *s)
{
  if (isascii(*s) && isdigit(*s))
    return true;
  if ((s[0] == '+' || s[0] == '-') && s[1])
    return true;
  return false;
}

char *
minifloat_t::pname() const
{
  char *r = new char[64];
  sprintf(r,"%f",val);
  if (! str_looks_like_double(r))
    sprintf(r,"+%f",val);
  return r;
}

static bool
str_is_double(const char *s, double &x)
{
  if (str_looks_like_double(s))
    {
      char *end;
      errno = 0;
      x = (double) strtol(s, &end, 0);
      if (*end == 0 && errno == 0) 
        return true;
      x = (double) strtod(s, &end);
      if (*end == 0 && errno == 0) 
        return true;
    }
  return false;
}



/* -------------------------------------------------- */
/* INPUT/OUTPUT                                       */
/* -------------------------------------------------- */

static int true_stdio_fputs(miniexp_io_t *io, const char *s) {
  FILE *f = (io->data[1]) ? (FILE*)(io->data[1]) : stdout;
  return ::fputs(s, f);
}
static int compat_puts(const char *s) { 
  return true_stdio_fputs(&miniexp_io, s); 
}
static int stdio_fputs(miniexp_io_t *io, const char *s) {
  if (io == &miniexp_io) 
    return (*minilisp_puts)(s);
  return true_stdio_fputs(io, s);
}

static int true_stdio_fgetc(miniexp_io_t *io) {
  FILE *f = (io->data[0]) ? (FILE*)(io->data[0]) : stdin;
  return ::getc(f);
}
static int compat_getc() { 
  return true_stdio_fgetc(&miniexp_io); 
}
static int stdio_fgetc(miniexp_io_t *io)
{
  if (io == &miniexp_io) 
    return (*minilisp_getc)();
  return true_stdio_fgetc(io);
}

static int true_stdio_ungetc(miniexp_io_t *io, int c) {
  FILE *f = (io->data[0]) ? (FILE*)(io->data[0]) : stdin;
  return ::ungetc(c, f);
}
static int compat_ungetc(int c) { 
  return true_stdio_ungetc(&miniexp_io, c); 
}
static int stdio_ungetc(miniexp_io_t *io, int c) {
  if (io == &miniexp_io) 
    return (*minilisp_ungetc)(c);
  return true_stdio_ungetc(io, c);
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
  io->p_flags = (int*)&minilisp_print_7bits;;
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
  bool must_quote_symbol(const char *s, int flags);
  void mlput_quoted_symbol(const char *s);
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
printer_t::must_quote_symbol(const char *s, int flags)
{
  int c;
  const char *r = s;
  while ((c = *r++))
    if (c=='(' || c==')' || c=='\"' || c=='|' || 
        !isascii(c) || isspace(c) || !isprint(c) ||
        (c >= 0 && c < 128 && io->p_macrochar && io->p_macrochar[c]) )
      return true;
  double x;
  if (flags & miniexp_io_quotemoresymbols)
    return str_looks_like_double(s);
  return str_is_double(s, x);
}

void
printer_t::mlput_quoted_symbol(const char *s)
{
  int l = strlen(s);
  char *r = new char[l+l+3];
  char *z = r;
  *z++ = '|';
  while (*s)
    if ((*z++ = *s++) == '|')
      *z++ = '|';
  *z++ = '|';
  *z++ = 0;
  mlput(r);
  delete [] r;
}

void
printer_t::print(miniexp_t p)
{
  int flags = (io->p_flags) ? *io->p_flags : 0;
  static char buffer[32];
  miniexp_t b = begin();
  if (p == miniexp_nil)
    {
      mlput("()");
    }
  else if (miniexp_numberp(p))
    {
      sprintf(buffer, "%d", miniexp_to_int(p));
      mlput(buffer);
    }
  else if (miniexp_symbolp(p))
    {
      const char *s = miniexp_to_name(p);
      if (must_quote_symbol(s, flags))
        mlput_quoted_symbol(s);
      else
      mlput(s);
    }
  else if (miniexp_stringp(p))
    {
      const char *s;
      size_t len = miniexp_to_lstr(p, &s);
      int n = print_c_string(s, 0, flags, len);
      char *d = new char[n];
      if (d) 
        print_c_string(s, d, flags, len);
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
      miniexp_rplaca(p, miniexp_number(tab - pos));
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

// SumatraPDF: don't compile as it's not used and it's the only place
// using try/catch, which is not compatible with compiling as /EHs-c-
#if 0
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
#endif

/* ---- INPUT */

static void
grow(char* &s, size_t &l, size_t &m)
{
      int nm = ((m<256)?256:m) + ((m>32000)?32000:m);
      char *ns = new char[nm+1];
      memcpy(ns, s, l);
      delete [] s;
      m = nm;
      s = ns;
}

static void
append(int c, char* &s, size_t &l, size_t &m)
{
  if (l >= m)
    grow(s, l, m);
  s[l++] = c;
  s[l] = 0;
}

static void
append_utf8(int x, char *&s, size_t &l, size_t &m)
{
  if (x >= 0 && x <= 0x10ffff)
    { 
      if (l + 4 >= m)
        grow(s, l, m);
      if (x <= 0x7f) {
        s[l++] = (char)x;
      } else if (x <= 0x7ff) {
        s[l++] = (char)((x>>6)|0xc0);
        s[l++] = (char)((x|0x80)&0xbf);
      } else if (x <= 0xffff) {
        s[l++] = (char)((x>>12)|0xe0);
        s[l++] = (char)(((x>>6)|0x80)&0xbf);
        s[l++] = (char)((x|0x80)&0xbf);
      } else {
        s[l++] = (char)((x>>18)|0xf0);
        s[l++] = (char)(((x>>12)|0x80)&0xbf);
        s[l++] = (char)(((x>>6)|0x80)&0xbf);
        s[l++] = (char)((x|0x80)&0xbf);
    }
  s[l] = 0;
    }
}

static void
skip_blank(miniexp_io_t *io, int &c)
{
  while (isspace(c))
    c = io->fgetc(io);
}

static void
skip_newline(miniexp_io_t *io, int &c)
{
  int d = c;
  if (c == '\n' || c == '\r')
    c = io->fgetc(io);
  if ((c == '\n' || c == '\r') && (c != d))
    c = io->fgetc(io);
}

static int
skip_octal(miniexp_io_t *io, int &c, int maxlen=3)
{
  int n = 0;
  int x = 0;
  while (c >= '0' && c < '8' && n++ < maxlen)
    {
      x = (x<<3) + c - '0';
      c = io->fgetc(io);
    }
  return x;
}

static int
skip_hexadecimal(miniexp_io_t *io, int &c, int maxlen=2)
{
  int n = 0;
  int x = 0;
  while (isxdigit(c) && n++ < maxlen && x <= 0x10fff) // unicode range only
    {
      x = (x<<4) + (isdigit(c) ? c-'0' : toupper(c)-'A'+10);
      c = io->fgetc(io);
    }
  return x;
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
  size_t l = 0;
  size_t m = 0;
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
          if (c == '\n' || c == '\r')
            {
              skip_newline(io, c);
              continue;
            }
          else if (c>='0' && c<='7')
            {
              int x = skip_octal(io, c, 3);
              append((char)x, s, l, m);
              continue;
            }
          else if (c=='x' || c=='X')
            {
              int d = c;
              c = io->fgetc(io);
              if (isxdigit(c))
                    {
                  int x = skip_hexadecimal(io, c, 2);
              append((char)x, s, l, m);
              continue;
            }
              io->ungetc(io, c);
              c = d;
            }
          else if (c == 'u' || c == 'U')
            {
              int x = -1;
              int d = c;
              c = io->fgetc(io);
              if (isxdigit(c))
                x = skip_hexadecimal(io, c, isupper(d) ? 6 : 4);
              while (x >= 0xd800 && x <= 0xdbff && c == '\\')
                {
                  c = io->fgetc(io);
                  if (c != 'u' && c != 'U') 
                    {
                      io->ungetc(io, c);
                      c = '\\';
                      break;
                    }
                  d = c;
                  c = io->fgetc(io);
                  int z = -1;
                  if (isxdigit(c))
                    z = skip_hexadecimal(io, c, isupper(d) ? 6 : 4);
                  if (z >= 0xdc00 && z <= 0xdfff)
                    {
                      x = 0x10000 + ((x & 0x3ff) << 10) + (z & 0x3ff);
                      break;
                    }
                  append_utf8(x, s, l, m);
                  x = z;
                    }
              if (x >= 0)
                {
                  append_utf8(x, s, l, m);
                  continue;
                }
                  io->ungetc(io, c);
                  c = d;
                }
          static const char *tr1 = "tnrbfvae?";
          static const char *tr2 = "\t\n\r\b\f\013\007\033?";
          for (int i=0; tr1[i]; i++)
            if (c == tr1[i])
              c = tr2[i];
        }
      append(c,s,l,m);
      c = io->fgetc(io);
    }
  c = io->fgetc(io);
  r = miniexp_lstring(l, s);
  delete [] s;
  return r;
}

static miniexp_t
read_quoted_symbol(miniexp_io_t *io, int &c)
{
  miniexp_t r;
  char *s = 0;
  size_t l = 0;
  size_t m = 0;
  ASSERT(c == '|');
  for(;;)
    {
      c = io->fgetc(io);
      if (c==EOF || (isascii(c) && !isprint(c)))
        return read_error(io, c);
      if (c=='|')
        if ((c = io->fgetc(io)) != '|')
        break;
      append(c,s,l,m);
    }
  r = miniexp_symbol(s ? s : "");
  delete [] s;
  return r;
}

static miniexp_t
read_symbol_or_number(miniexp_io_t *io, int &c)
{
  miniexp_t r;
  char *s = 0;
  size_t l = 0;
  size_t m = 0;
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
  double x;
  if (str_is_double(s, x))
    r = miniexp_double(x);
  else
    r = miniexp_symbol(s);
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
          // clean end-of-file.
          return miniexp_dummy;
        }
      else if (c == ')')
        {
          c = io->fgetc(io);
          continue;
        }
      else if (c == '(')
        {
          minivar_t l = miniexp_cons(miniexp_nil, miniexp_nil);
          miniexp_t tail = l;
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
                return read_error(io, c);
              p = miniexp_cons(p, miniexp_nil);
              miniexp_rplacd(tail, p);
              tail = p;
            }
          if (c == '.')
            {
              c = io->fgetc(io);
              skip_blank(io, c);
              if (c != ')')
                miniexp_rplacd(tail, read_miniexp(io, c));
            }
          skip_blank(io, c);
          if (c != ')')
            return read_error(io, c);
          c = io->fgetc(io);
          return cdr(l);
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
          else if (p)
            return read_error(io, c);
          c = io->fgetc(io);
          continue;
        }
      else if (c == '#')
        {
          int nc = io->fgetc(io);
          if (io->p_diezechar && io->p_macroqueue
              && nc >= 0 && nc < 128 && io->p_diezechar[nc])
            {
              miniexp_t p = io->p_macrochar[nc](io);
              if (miniexp_length(p) > 0)
                *io->p_macroqueue = p;
              else if (p)
                return read_error(io, c);
              c = io->fgetc(io);
              continue;
            }
          else if (nc == '#')
            return read_error(io, c);
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
  if (c != EOF)
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
  CSLOCK(locker);
  ASSERT(!gc.lock);
  // clear minivars
  minivar_t::mark(gc_clear);
  for (gctls_t *tls = gc.tls; tls; tls=tls->next)
  for (int i=0; i<recentsize; i++)
      tls->recent[i] = 0;
  // collect everything
  gc_run();
  // deallocate everything
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
  delete symbols;
  symbols = 0;
}


