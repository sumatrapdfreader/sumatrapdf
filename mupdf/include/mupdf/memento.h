/* Copyright (C) 2009-2018 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license. Refer to licensing information at http://www.artifex.com
   or contact Artifex Software, Inc.,  1305 Grant Avenue - Suite 200,
   Novato, CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/* Memento: A library to aid debugging of memory leaks/heap corruption.
 *
 * Usage (with C):
 *    First, build your project with MEMENTO defined, and include this
 *    header file wherever you use malloc, realloc or free.
 *    This header file will use macros to point malloc, realloc and free to
 *    point to Memento_malloc, Memento_realloc, Memento_free.
 *
 *    Run your program, and all mallocs/frees/reallocs should be redirected
 *    through here. When the program exits, you will get a list of all the
 *    leaked blocks, together with some helpful statistics. You can get the
 *    same list of allocated blocks at any point during program execution by
 *    calling Memento_listBlocks();
 *
 *    Every call to malloc/free/realloc counts as an 'allocation event'.
 *    On each event Memento increments a counter. Every block is tagged with
 *    the current counter on allocation. Every so often during program
 *    execution, the heap is checked for consistency. By default this happens
 *    after 1024 events, then after 2048 events, then after 4096 events, etc.
 *    This can be changed at runtime by using Memento_setParanoia(int level).
 *    0 turns off such checking, 1 sets checking to happen on every event,
 *    any positive number n sets checking to happen once every n events,
 *    and any negative number n sets checking to happen after -n events, then
 *    after -2n events etc.
 *
 *    The default paranoia level is therefore -1024.
 *
 *    Memento keeps blocks around for a while after they have been freed, and
 *    checks them as part of these heap checks to see if they have been
 *    written to (or are freed twice etc).
 *
 *    A given heap block can be checked for consistency (it's 'pre' and
 *    'post' guard blocks are checked to see if they have been written to)
 *    by calling Memento_checkBlock(void *blockAddress);
 *
 *    A check of all the memory can be triggered by calling Memento_check();
 *    (or Memento_checkAllMemory(); if you'd like it to be quieter).
 *
 *    A good place to breakpoint is Memento_breakpoint, as this will then
 *    trigger your debugger if an error is detected. This is done
 *    automatically for debug windows builds.
 *
 *    If a block is found to be corrupt, information will be printed to the
 *    console, including the address of the block, the size of the block,
 *    the type of corruption, the number of the block and the event on which
 *    it last passed a check for correctness.
 *
 *    If you rerun, and call Memento_paranoidAt(int event); with this number
 *    the code will wait until it reaches that event and then start
 *    checking the heap after every allocation event. Assuming it is a
 *    deterministic failure, you should then find out where in your program
 *    the error is occurring (between event x-1 and event x).
 *
 *    Then you can rerun the program again, and call
 *    Memento_breakAt(int event); and the program will call
 *    Memento_Breakpoint() when event x is reached, enabling you to step
 *    through.
 *
 *    Memento_find(address) will tell you what block (if any) the given
 *    address is in.
 *
 * An example:
 *    Suppose we have a gs invocation that crashes with memory corruption.
 *     * Build with -DMEMENTO.
 *     * In your debugger put breakpoints on Memento_inited and
 *       Memento_Breakpoint.
 *     * Run the program. It will stop in Memento_inited.
 *     * Execute Memento_setParanoia(1);  (In VS use Ctrl-Alt-Q). (Note #1)
 *     * Continue execution.
 *     * It will detect the memory corruption on the next allocation event
 *       after it happens, and stop in Memento_breakpoint. The console should
 *       show something like:
 *
 *       Freed blocks:
 *         0x172e610(size=288,num=1415) index 256 (0x172e710) onwards corrupted
 *           Block last checked OK at allocation 1457. Now 1458.
 *
 *     * This means that the block became corrupted between allocation 1457
 *       and 1458 - so if we rerun and stop the program at 1457, we can then
 *       step through, possibly with a data breakpoint at 0x172e710 and see
 *       when it occurs.
 *     * So restart the program from the beginning. When we hit Memento_inited
 *       execute Memento_breakAt(1457); (and maybe Memento_setParanoia(1), or
 *       Memento_setParanoidAt(1457))
 *     * Continue execution until we hit Memento_breakpoint.
 *     * Now you can step through and watch the memory corruption happen.
 *
 *    Note #1: Using Memento_setParanoia(1) can cause your program to run
 *    very slowly. You may instead choose to use Memento_setParanoia(100)
 *    (or some other figure). This will only exhaustively check memory on
 *    every 100th allocation event. This trades speed for the size of the
 *    average allocation event range in which detection of memory corruption
 *    occurs. You may (for example) choose to run once checking every 100
 *    allocations and discover that the corruption happens between events
 *    X and X+100. You can then rerun using Memento_paranoidAt(X), and
 *    it'll only start exhaustively checking when it reaches X.
 *
 * More than one memory allocator?
 *
 *    If you have more than one memory allocator in the system (like for
 *    instance the ghostscript chunk allocator, that builds on top of the
 *    standard malloc and returns chunks itself), then there are some things
 *    to note:
 *
 *    * If the secondary allocator gets its underlying blocks from calling
 *      malloc, then those will be checked by Memento, but 'subblocks' that
 *      are returned to the secondary allocator will not. There is currently
 *      no way to fix this other than trying to bypass the secondary
 *      allocator. One way I have found to do this with the chunk allocator
 *      is to tweak its idea of a 'large block' so that it puts every
 *      allocation in its own chunk. Clearly this negates the point of having
 *      a secondary allocator, and is therefore not recommended for general
 *      use.
 *
 *    * Again, if the secondary allocator gets its underlying blocks from
 *      calling malloc (and hence Memento) leak detection should still work
 *      (but whole blocks will be detected rather than subblocks).
 *
 *    * If on every allocation attempt the secondary allocator calls into
 *      Memento_failThisEvent(), and fails the allocation if it returns true
 *      then more useful features can be used; firstly memory squeezing will
 *      work, and secondly, Memento will have a "finer grained" paranoia
 *      available to it.
 *
 * Usage with C++:
 *
 *    Memento has some experimental code in it to trap new/delete (and
 *    new[]/delete[] if required) calls.
 *
 *    In order for this to work, either:
 *
 *    1) Build memento.c with the c++ compiler.
 *
 *    or
 *
 *    2) Build memento.c as normal with the C compiler, then from any
 *       one of your .cpp files, do:
 *
 *       #define MEMENTO_CPP_EXTRAS_ONLY
 *       #include "memento.c"
 *
 *       In the case where MEMENTO is not defined, this will not do anything.
 *
 *    Both Windows and GCC provide separate new[] and delete[] operators
 *    for arrays. Apparently some systems do not. If this is the case for
 *    your system, define MEMENTO_CPP_NO_ARRAY_CONSTRUCTORS.
 */

#ifndef MEMENTO_H

#include <stdlib.h>

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

void Memento_fin(void);

void Memento_bt(void);

#ifdef MEMENTO

#ifndef COMPILING_MEMENTO_C
#define malloc  Memento_malloc
#define free    Memento_free
#define realloc Memento_realloc
#define calloc  Memento_calloc
#endif

#else

#define Memento_malloc  MEMENTO_UNDERLYING_MALLOC
#define Memento_free    MEMENTO_UNDERLYING_FREE
#define Memento_realloc MEMENTO_UNDERLYING_REALLOC
#define Memento_calloc  MEMENTO_UNDERLYING_CALLOC

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

#endif /* MEMENTO */

#endif /* MEMENTO_H */
