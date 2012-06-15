/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* Memento: A library to aid debugging of memory leaks/heap corruption.
 *
 * Usage:
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
 *    every 1024 events. This can be changed at runtime by using
 *    Memento_setParanoia(int level). 0 turns off such checking, 1 sets
 *    checking to happen on every event, any other number n sets checking to
 *    happen once every n events.
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
 *    the the code will wait until it reaches that event and then start
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
 */

#ifndef MEMENTO_H

#include <memory.h>

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

void *Memento_malloc(size_t s);
void *Memento_realloc(void *, size_t s);
void  Memento_free(void *);
void *Memento_calloc(size_t, size_t);

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

#define Memento_checkBlock(A)     0
#define Memento_checkAllMemory()  0
#define Memento_check()           0
#define Memento_setParanoia(A)    0
#define Memento_paranoidAt(A)     0
#define Memento_breakAt(A)        0
#define Memento_breakOnFree(A)    0
#define Memento_breakOnRealloc(A) 0
#define Memento_getBlockNum(A)    0
#define Memento_find(A)           0
#define Memento_breakpoint()      do {} while (0)
#define Memento_failAt(A)         0
#define Memento_failThisEvent()   0
#define Memento_listBlocks()      do {} while (0)
#define Memento_listNewBlocks()   do {} while (0)
#define Memento_setMax(A)         0
#define Memento_stats()           do {} while (0)
#define Memento_label(A,B)        (A)

#endif /* MEMENTO */

#endif /* MEMENTO_H */
