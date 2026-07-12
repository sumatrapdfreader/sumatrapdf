#ifndef MEMENTO_H

#include <stdlib.h>
#include <stdarg.h>

#define MEMENTO_H

#ifndef MEMENTO_UNDERLYING_MALLOC
#define MEMENTO_UNDERLYING_MALLOC malloc
#endif
#ifndef MEMENTO_UNDERLYING_FREE
#define MEMENTO_UNDERLYING_FREE free
#endif
#ifndef MEMENTO_UNDERLYING_REALLOC
#define MEMENTO_UNDERLYING_REALLOC realloc
#endif
#ifndef MEMENTO_UNDERLYING_CALLOC
#define MEMENTO_UNDERLYING_CALLOC calloc
#endif

#ifndef MEMENTO_MAXALIGN
#define MEMENTO_MAXALIGN (sizeof(int))
#endif

#define MEMENTO_PREFILL   0xa6
#define MEMENTO_POSTFILL  0xa7
#define MEMENTO_ALLOCFILL 0xa8
#define MEMENTO_FREEFILL  0xa9

#define MEMENTO_FREELIST_MAX 0x2000000

int Memento_checkBlock(void *);
int Memento_checkAllMemory(void);
int Memento_check(void);

int Memento_setParanoia(int);
int Memento_paranoidAt(int);
int Memento_breakAt(int);
void Memento_breakOnFree(void *a);
void Memento_breakOnRealloc(void *a);
int Memento_getBlockNum(void *);
int Memento_find(void *a);
void Memento_breakpoint(void);
int Memento_failAt(int);
int Memento_failThisEvent(void);
void Memento_listBlocks(void);
void Memento_listNewBlocks(void);
size_t Memento_setMax(size_t);
void Memento_stats(void);
void *Memento_label(void *, const char *);
void Memento_tick(void);

void *Memento_malloc(size_t s);
void *Memento_realloc(void *, size_t s);
void  Memento_free(void *);
void *Memento_calloc(size_t, size_t);
char *Memento_strdup(const char*);
int Memento_asprintf(char **ret, const char *format, ...);
int Memento_vasprintf(char **ret, const char *format, va_list ap);

void Memento_info(void *addr);
void Memento_listBlockInfo(void);
void *Memento_takeByteRef(void *blk);
void *Memento_dropByteRef(void *blk);
void *Memento_takeShortRef(void *blk);
void *Memento_dropShortRef(void *blk);
void *Memento_takeIntRef(void *blk);
void *Memento_dropIntRef(void *blk);
void *Memento_takeRef(void *blk);
void *Memento_dropRef(void *blk);
void *Memento_adjustRef(void *blk, int adjust);
void *Memento_reference(void *blk);

int Memento_checkPointerOrNull(void *blk);
int Memento_checkBytePointerOrNull(void *blk);
int Memento_checkShortPointerOrNull(void *blk);
int Memento_checkIntPointerOrNull(void *blk);

void Memento_startLeaking(void);
void Memento_stopLeaking(void);

int Memento_sequence(void);

int Memento_squeezing(void);

void Memento_fin(void);

void Memento_bt(void);

#ifdef MEMENTO

#ifndef COMPILING_MEMENTO_C
#define malloc    Memento_malloc
#define free      Memento_free
#define realloc   Memento_realloc
#define calloc    Memento_calloc
#define strdup    Memento_strdup
#define asprintf  Memento_asprintf
#define vasprintf Memento_vasprintf
#endif

#else

#define Memento_malloc    MEMENTO_UNDERLYING_MALLOC
#define Memento_free      MEMENTO_UNDERLYING_FREE
#define Memento_realloc   MEMENTO_UNDERLYING_REALLOC
#define Memento_calloc    MEMENTO_UNDERLYING_CALLOC
#define Memento_strdup    strdup
#define Memento_asprintf  asprintf
#define Memento_vasprintf vasprintf

#define Memento_checkBlock(A)              0
#define Memento_checkAllMemory()           0
#define Memento_check()                    0
#define Memento_setParanoia(A)             0
#define Memento_paranoidAt(A)              0
#define Memento_breakAt(A)                 0
#define Memento_breakOnFree(A)             0
#define Memento_breakOnRealloc(A)          0
#define Memento_getBlockNum(A)             0
#define Memento_find(A)                    0
#define Memento_breakpoint()               do {} while (0)
#define Memento_failAt(A)                  0
#define Memento_failThisEvent()            0
#define Memento_listBlocks()               do {} while (0)
#define Memento_listNewBlocks()            do {} while (0)
#define Memento_setMax(A)                  0
#define Memento_stats()                    do {} while (0)
#define Memento_label(A,B)                 (A)
#define Memento_info(A)                    do {} while (0)
#define Memento_listBlockInfo()            do {} while (0)
#define Memento_takeByteRef(A)             (A)
#define Memento_dropByteRef(A)             (A)
#define Memento_takeShortRef(A)            (A)
#define Memento_dropShortRef(A)            (A)
#define Memento_takeIntRef(A)              (A)
#define Memento_dropIntRef(A)              (A)
#define Memento_takeRef(A)                 (A)
#define Memento_dropRef(A)                 (A)
#define Memento_adjustRef(A,V)             (A)
#define Memento_reference(A)               (A)
#define Memento_checkPointerOrNull(A)      0
#define Memento_checkBytePointerOrNull(A)  0
#define Memento_checkShortPointerOrNull(A) 0
#define Memento_checkIntPointerOrNull(A)   0

#define Memento_tick()                     do {} while (0)
#define Memento_startLeaking()             do {} while (0)
#define Memento_stopLeaking()              do {} while (0)
#define Memento_fin()                      do {} while (0)
#define Memento_bt()                       do {} while (0)
#define Memento_sequence()                 (0)
#define Memento_squeezing()                (0)

#endif

#endif
