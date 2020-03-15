/*
  Expression Evaluator Library (NS-EEL) v2
  Copyright (C) 2004-2013 Cockos Incorporated
  Copyright (C) 1999-2003 Nullsoft, Inc.
  
  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "ns-eel.h"
#include "ns-eel-int.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#ifdef _WIN32
#include <malloc.h>
#ifdef _MSC_VER
#define inline __inline
#endif

#endif

unsigned int NSEEL_RAM_limitmem=0;
unsigned int NSEEL_RAM_memused=0;
int NSEEL_RAM_memused_errors=0;



int NSEEL_VM_wantfreeRAM(NSEEL_VMCTX ctx)
{
	if (ctx)
  {
    compileContext *c=(compileContext*)ctx;
    if (c->ram_state.needfree) 
      return 1;
  }
  return 0;
}

void NSEEL_VM_freeRAMIfCodeRequested(NSEEL_VMCTX ctx) // check to see if our free flag was set
{
	if (ctx)
	{
  	compileContext *c=(compileContext*)ctx;
  	if (c->ram_state.needfree) 
		{
      NSEEL_HOSTSTUB_EnterMutex();
      {
			  INT_PTR startpos=((INT_PTR)c->ram_state.needfree)-1;
	 		  EEL_F **blocks = c->ram_state.blocks;
			  INT_PTR pos=0;
			  int x;
  		  for (x = 0; x < NSEEL_RAM_BLOCKS; x ++)
  		  {
				  if (pos >= startpos)
				  {
					  if (blocks[x])
					  {
						  if (NSEEL_RAM_memused >= sizeof(EEL_F) * NSEEL_RAM_ITEMSPERBLOCK) 
							  NSEEL_RAM_memused -= sizeof(EEL_F) * NSEEL_RAM_ITEMSPERBLOCK;
						  else NSEEL_RAM_memused_errors++;
       	 	    free(blocks[x]);
       	 	    blocks[x]=0;
					  }
				  }
				  pos+=NSEEL_RAM_ITEMSPERBLOCK;
 			  }
			  c->ram_state.needfree=0;
      }
      NSEEL_HOSTSTUB_LeaveMutex();
		}

	}
}

EEL_F nseel_ramalloc_onfail;
EEL_F * volatile  nseel_gmembuf_default;


void *(*nseel_gmem_calloc)(size_t a, size_t b);

EEL_F * NSEEL_CGEN_CALL __NSEEL_RAMAllocGMEM(EEL_F ***blocks, unsigned int w)
{
  if (blocks) 
  {
    EEL_F **pblocks=*blocks;

    if (w < NSEEL_RAM_BLOCKS*NSEEL_RAM_ITEMSPERBLOCK)
    {
      const unsigned int whichblock = w/NSEEL_RAM_ITEMSPERBLOCK;
      EEL_F *p=NULL;
      if (!pblocks || !(p=pblocks[whichblock]))
      {
        NSEEL_HOSTSTUB_EnterMutex();
        if (!nseel_gmem_calloc) nseel_gmem_calloc=calloc;

        if (!(pblocks=*blocks)) pblocks = *blocks = (EEL_F **)nseel_gmem_calloc(sizeof(EEL_F *),NSEEL_RAM_BLOCKS);
        else p = pblocks[whichblock];

        if (!p && pblocks)
        {
          p=pblocks[whichblock]=(EEL_F *)nseel_gmem_calloc(sizeof(EEL_F),NSEEL_RAM_ITEMSPERBLOCK);
        }
        NSEEL_HOSTSTUB_LeaveMutex();
      }
      if (p) return p + (w&(NSEEL_RAM_ITEMSPERBLOCK-1));
    }
    return &nseel_ramalloc_onfail;
  }

  if (!nseel_gmembuf_default)
  {
    NSEEL_HOSTSTUB_EnterMutex(); 
    if (!nseel_gmembuf_default) nseel_gmembuf_default=(EEL_F*)calloc(sizeof(EEL_F),NSEEL_SHARED_GRAM_SIZE);
    NSEEL_HOSTSTUB_LeaveMutex();
    if (!nseel_gmembuf_default) return &nseel_ramalloc_onfail;
  }

  return nseel_gmembuf_default+(((unsigned int)w)&((NSEEL_SHARED_GRAM_SIZE)-1));
}


EEL_F * NSEEL_CGEN_CALL  __NSEEL_RAMAlloc(EEL_F **pblocks, unsigned int w)
{
//  fprintf(stderr,"got request at %d, %d\n",w/NSEEL_RAM_ITEMSPERBLOCK, w&(NSEEL_RAM_ITEMSPERBLOCK-1));
  if (w < NSEEL_RAM_BLOCKS*NSEEL_RAM_ITEMSPERBLOCK)
  {
    unsigned int whichblock = w/NSEEL_RAM_ITEMSPERBLOCK;
    EEL_F *p=pblocks[whichblock];
    if (!p && whichblock < ((unsigned int *)pblocks)[-3]) // pblocks -1/-2 are closefact, -3 is maxblocks
    {
      NSEEL_HOSTSTUB_EnterMutex();

      if (!(p=pblocks[whichblock]))
      {

      	const int msize=sizeof(EEL_F) * NSEEL_RAM_ITEMSPERBLOCK;
      	if (!NSEEL_RAM_limitmem || NSEEL_RAM_memused+msize < NSEEL_RAM_limitmem) 
      	{
	      	p=pblocks[whichblock]=(EEL_F *)calloc(sizeof(EEL_F),NSEEL_RAM_ITEMSPERBLOCK);
      		if (p) NSEEL_RAM_memused+=msize;
      	}
      }
      NSEEL_HOSTSTUB_LeaveMutex();
    }	  
    if (p) return p + (w&(NSEEL_RAM_ITEMSPERBLOCK-1));
  }
//  fprintf(stderr,"ret 0\n");
  return &nseel_ramalloc_onfail;
}


EEL_F * NSEEL_CGEN_CALL __NSEEL_RAM_MemFree(void *blocks, EEL_F *which)
{
  // blocks points to ram_state.blocks, so back it up past closefact and maxblocks to needfree
  int *flag = (int *)((char *)blocks - sizeof(double) - 2*sizeof(int));
	int d=(int)(*which);
	if (d < 0) d=0;
	if (d < flag[1]*NSEEL_RAM_ITEMSPERBLOCK) flag[0]=1+d;
	return which;
}

EEL_F * NSEEL_CGEN_CALL __NSEEL_RAM_MemTop(void *blocks, EEL_F *which)
{
  // blocks points to ram_state.blocks, so back it up past closefact to maxblocks
  const int *flag = (int *)((char *)blocks - sizeof(double) - sizeof(int));
  *which = flag[0]*NSEEL_RAM_ITEMSPERBLOCK;
  return which;
}






EEL_F * NSEEL_CGEN_CALL __NSEEL_RAM_MemCpy(EEL_F **blocks,EEL_F *dest, EEL_F *src, EEL_F *lenptr)
{
  const int mem_size=NSEEL_RAM_BLOCKS*NSEEL_RAM_ITEMSPERBLOCK;
  int dest_offs = (int)(*dest + 0.0001);
  int src_offs = (int)(*src + 0.0001);
  int len = (int)(*lenptr + 0.0001);
  int want_mmove=0;

  // trim to front
  if (src_offs<0)
  {
    len += src_offs;
    dest_offs -= src_offs;
    src_offs=0;
  }
  if (dest_offs<0)
  {
    len += dest_offs;
    src_offs -= dest_offs;
    dest_offs=0;
  }
  if (src_offs + len > mem_size) len = mem_size-src_offs;
  if (dest_offs + len > mem_size) len = mem_size-dest_offs;

  if (src_offs == dest_offs || len < 1) return dest;

  if (src_offs < dest_offs && src_offs+len > dest_offs)
  {
    // if src_offs < dest_offs and overlapping, must copy right to left
    if ((dest_offs - src_offs) < NSEEL_RAM_ITEMSPERBLOCK) want_mmove = 1;
    src_offs += len;
    dest_offs += len;
    while (len > 0)
    {
      const int maxdlen=((dest_offs-1)&(NSEEL_RAM_ITEMSPERBLOCK-1)) + 1;
      const int maxslen=((src_offs-1)&(NSEEL_RAM_ITEMSPERBLOCK-1)) + 1;
      int copy_len = len;
      EEL_F *srcptr,*destptr;

      if (copy_len > maxdlen) copy_len=maxdlen;
      if (copy_len > maxslen) copy_len=maxslen;

      srcptr = __NSEEL_RAMAlloc(blocks,src_offs - copy_len);
      destptr = __NSEEL_RAMAlloc(blocks,dest_offs - copy_len);
      if (srcptr==&nseel_ramalloc_onfail || destptr==&nseel_ramalloc_onfail) break;

      if (want_mmove) memmove(destptr,srcptr,sizeof(EEL_F)*copy_len);
      else memcpy(destptr,srcptr,sizeof(EEL_F)*copy_len);
      src_offs-=copy_len;
      dest_offs-=copy_len;
      len-=copy_len;
    }
    return dest;
  }

  if (dest_offs < src_offs && dest_offs+len > src_offs)
  {
    // if dest_offs < src_offs and overlapping, and less than NSEEL_RAM_ITEMSPERBLOCK apart, use memmove()
    if ((src_offs-dest_offs) < NSEEL_RAM_ITEMSPERBLOCK) want_mmove = 1;
  }

  while (len > 0)
  {
    const int maxdlen=NSEEL_RAM_ITEMSPERBLOCK - (dest_offs&(NSEEL_RAM_ITEMSPERBLOCK-1));
    const int maxslen=NSEEL_RAM_ITEMSPERBLOCK -  (src_offs&(NSEEL_RAM_ITEMSPERBLOCK-1));
    int copy_len = len;
    EEL_F *srcptr,*destptr;

    if (copy_len > maxdlen) copy_len=maxdlen;
    if (copy_len > maxslen) copy_len=maxslen;

    srcptr = __NSEEL_RAMAlloc(blocks,src_offs);
    destptr = __NSEEL_RAMAlloc(blocks,dest_offs);
    if (srcptr==&nseel_ramalloc_onfail || destptr==&nseel_ramalloc_onfail) break;

    if (want_mmove) memmove(destptr,srcptr,sizeof(EEL_F)*copy_len);
    else memcpy(destptr,srcptr,sizeof(EEL_F)*copy_len);
    src_offs+=copy_len;
    dest_offs+=copy_len;
    len-=copy_len;
  }
  return dest;
}

EEL_F * NSEEL_CGEN_CALL __NSEEL_RAM_MemSet(EEL_F **blocks,EEL_F *dest, EEL_F *v, EEL_F *lenptr)
{  
  int offs = (int)(*dest + 0.0001);
  int len = (int)(*lenptr + 0.0001);
  EEL_F t;
  if (offs<0) 
  {
    len += offs;
    offs=0;
  }
  if (offs >= NSEEL_RAM_BLOCKS*NSEEL_RAM_ITEMSPERBLOCK) return dest;

  if (offs+len > NSEEL_RAM_BLOCKS*NSEEL_RAM_ITEMSPERBLOCK) len = NSEEL_RAM_BLOCKS*NSEEL_RAM_ITEMSPERBLOCK - offs;

  if (len < 1) return dest;
  

  t=*v; // set value

//  int lastBlock=-1;
  while (len > 0)
  {
    int lcnt;
    EEL_F *ptr=__NSEEL_RAMAlloc(blocks,offs);
    if (ptr==&nseel_ramalloc_onfail) break;

    lcnt=NSEEL_RAM_ITEMSPERBLOCK-(offs&(NSEEL_RAM_ITEMSPERBLOCK-1));
    if (lcnt > len) lcnt=len;

    len -= lcnt;
    offs += lcnt;

    while (lcnt--)
    {
      *ptr++=t;
    }       
  }
  return dest;
}


static inline int __getset_values(EEL_F **blocks, int isset, int len, EEL_F **parms)
{
  int offs, lout=0;
  unsigned int pageidx, sub_offs;
  if (--len < 1) return 0;
  offs = (int)(parms++[0][0] + 0.0001);

  if (offs<=0) 
  {
    len += offs;
    parms -= offs;
    offs=0;
    pageidx=sub_offs=0;
    if (len<1) return 0;
  }
  else
  {
    sub_offs = ((unsigned int)offs) & (NSEEL_RAM_ITEMSPERBLOCK-1);
    pageidx = ((unsigned int)offs)>>NSEEL_RAM_ITEMSPERBLOCK_LOG2;
    if (pageidx>=NSEEL_RAM_BLOCKS) return 0;
  }

  for (;;)
  {
    int lcnt=NSEEL_RAM_ITEMSPERBLOCK-sub_offs;
    EEL_F *ptr=blocks[pageidx];
    if (!ptr)
    {
      ptr = __NSEEL_RAMAlloc(blocks,offs + lout);
      if (ptr==&nseel_ramalloc_onfail) return lout;
    }
    else
    {
      ptr += sub_offs;
    }

    if (lcnt >= len) 
    { 
      // this page satisfies the request (normal behavior)
      lout += len;
      if (isset) while (len--) *ptr++=parms++[0][0];
      else while (len--) parms++[0][0] = *ptr++;
      return lout;
    }

    // crossing a page boundary
    len -= lcnt;
    lout += lcnt;
    if (isset) while (lcnt--) *ptr++=parms++[0][0];
    else while (lcnt--) parms++[0][0] = *ptr++;

    if (len <= 0 || ++pageidx >= NSEEL_RAM_BLOCKS) return lout;
    sub_offs=0;
  }
}

EEL_F NSEEL_CGEN_CALL __NSEEL_RAM_Mem_SetValues(EEL_F **blocks, INT_PTR np, EEL_F **parms)
{
  return __getset_values(blocks,1,(int)np,parms);
}

EEL_F NSEEL_CGEN_CALL __NSEEL_RAM_Mem_GetValues(EEL_F **blocks, INT_PTR np, EEL_F **parms)
{
  return __getset_values(blocks,0,(int)np,parms);
}

void NSEEL_VM_SetGRAM(NSEEL_VMCTX ctx, void **gram)
{
  if (ctx)
  {
    compileContext *c=(compileContext*)ctx;
    c->gram_blocks = gram;
  }
}


void NSEEL_VM_freeRAM(NSEEL_VMCTX ctx)
{
  if (ctx)
  {
    int x;
    compileContext *c=(compileContext*)ctx;
    EEL_F **blocks = c->ram_state.blocks;
    for (x = 0; x < NSEEL_RAM_BLOCKS; x ++)
    {
	    if (blocks[x])
	    {
		    if (NSEEL_RAM_memused >= sizeof(EEL_F) * NSEEL_RAM_ITEMSPERBLOCK) 
			    NSEEL_RAM_memused -= sizeof(EEL_F) * NSEEL_RAM_ITEMSPERBLOCK;
		    else NSEEL_RAM_memused_errors++;
        free(blocks[x]);
        blocks[x]=0;
	    }
    }
    c->ram_state.needfree=0; // no need to free anymore
  }
}

void NSEEL_VM_FreeGRAM(void **ufd)
{
  if (ufd[0])
  {
    EEL_F **blocks = (EEL_F **)ufd[0];
    int x;
    for (x = 0; x < NSEEL_RAM_BLOCKS; x ++)
    {
	    if (blocks[x])
	    {
		    if (NSEEL_RAM_memused >= sizeof(EEL_F) * NSEEL_RAM_ITEMSPERBLOCK) 
			    NSEEL_RAM_memused -= sizeof(EEL_F) * NSEEL_RAM_ITEMSPERBLOCK;
		    else NSEEL_RAM_memused_errors++;
	    }
      free(blocks[x]);
      blocks[x]=0;
    }
    free(blocks);
    ufd[0]=0;
  }
}

EEL_F *NSEEL_VM_getramptr(NSEEL_VMCTX ctx, unsigned int offs, int *validCount)
{
  EEL_F *d=__NSEEL_RAMAlloc(ctx ? ((compileContext*)ctx)->ram_state.blocks : 0,offs);
  if (!d || d == &nseel_ramalloc_onfail) return NULL;
  if (validCount) *validCount = NSEEL_RAM_ITEMSPERBLOCK - (offs%NSEEL_RAM_ITEMSPERBLOCK);

  return d;
}

EEL_F *NSEEL_VM_getramptr_noalloc(NSEEL_VMCTX ctx, unsigned int offs, int *validCount)
{
  EEL_F *d;
  compileContext *cc = (compileContext *)ctx;

  if (!cc ||
      offs >= NSEEL_RAM_ITEMSPERBLOCK*NSEEL_RAM_BLOCKS ||
      NULL == (d = cc->ram_state.blocks[offs/NSEEL_RAM_ITEMSPERBLOCK])
      ) 
  {
    if (validCount) *validCount = 0;
    return NULL;
  }

  offs %= NSEEL_RAM_ITEMSPERBLOCK;
  if (validCount) *validCount = NSEEL_RAM_ITEMSPERBLOCK - offs;
  return d + offs;
}
