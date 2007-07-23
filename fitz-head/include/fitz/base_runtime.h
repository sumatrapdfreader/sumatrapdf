/*
 * Base Fitz runtime.
 * Contains: errors, memory manager, utf-8 strings, "standard" macros
 */

#ifndef __printflike
#if __GNUC__ > 2 || __GNUC__ == 2 && __GNUC_MINOR__ >= 7
#define __printflike(fmtarg, firstvararg) \
    __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#else
#define __printflike(fmtarg, firstvararg)
#endif
#endif

#undef nil
#define nil ((void*)0)

#ifndef _MSC_VER
#undef offsetof
#define offsetof(s, m) (unsigned long)(&(((s*)0)->m))
#endif

#undef nelem
#define nelem(x) (sizeof(x)/sizeof((x)[0]))

#undef ABS
#define ABS(x) ( (x) < 0 ? -(x) : (x) )

#undef MAX
#define MAX(a,b) ( (a) > (b) ? (a) : (b) )

#undef MIN
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )

#undef CLAMP
#define CLAMP(x,a,b) ( (x) > (b) ? (b) : ( (x) < (a) ? (a) : (x) ) )

#define MAX4(a,b,c,d) MAX(MAX(a,b), MAX(c,d))
#define MIN4(a,b,c,d) MIN(MIN(a,b), MIN(c,d))

#define STRIDE(n, bcp) (((bpc) * (n) + 7) / 8)

/* plan9 stuff for utf-8 and path munging */
int chartorune(int *rune, char *str);
int runetochar(char *str, int *rune);
int runelen(long c);
int runenlen(int *r, int nrune);
int fullrune(char *str, int n);
char *cleanname(char *name);

typedef struct fz_error_s fz_error;

struct fz_error_s
{
    int refs;
    char msg[184];
    char file[32];
    char func[32];
    int line;
    fz_error *cause;
};

#define fz_outofmem (&fz_koutofmem)
extern fz_error fz_koutofmem;

void fz_printerror(fz_error *eo);
fz_error *fz_keeperror(fz_error *eo);
void fz_droperror(fz_error *eo);
void fz_warn(char *fmt, ...) __printflike(1,2);

#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif

#define fz_throw(...) fz_throwimp(nil, __func__, __FILE__, __LINE__, __VA_ARGS__)
#define fz_rethrow(cause, ...) fz_throwimp(cause, __func__, __FILE__, __LINE__, __VA_ARGS__)
#define fz_okay ((fz_error*)0)

fz_error *fz_throwimp(fz_error *cause, const char *func, const char *file, int line, char *fmt, ...);

//fz_error *fz_throwimp(fz_error *cause, const char *func, const char *file, int line, char *fmt, ...) __printflike(5, 6);

typedef struct fz_memorycontext_s fz_memorycontext;

struct fz_memorycontext_s
{
    void * (*malloc)(fz_memorycontext *, int);
    void * (*realloc)(fz_memorycontext *, void *, int);
    void (*free)(fz_memorycontext *, void *);
};

fz_memorycontext *fz_currentmemorycontext(void);
void fz_setmemorycontext(fz_memorycontext *memorycontext);

void *fz_malloc(int n);
void *fz_realloc(void *p, int n);
void fz_free(void *p);

char *fz_strdup(char *s);

