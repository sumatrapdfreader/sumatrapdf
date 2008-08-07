/*
 * Copyright (c) 1999-2000 Image Power, Inc. and the University of
 *   British Columbia.
 * Copyright (c) 2001-2002 Michael David Adams.
 * All rights reserved.
 */

/* __START_OF_JASPER_LICENSE__
 * 
 * JasPer License Version 2.0
 * 
 * Copyright (c) 1999-2000 Image Power, Inc.
 * Copyright (c) 1999-2000 The University of British Columbia
 * Copyright (c) 2001-2003 Michael David Adams
 * 
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person (the
 * "User") obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 * 
 * 1.  The above copyright notices and this permission notice (which
 * includes the disclaimer below) shall be included in all copies or
 * substantial portions of the Software.
 * 
 * 2.  The name of a copyright holder shall not be used to endorse or
 * promote products derived from the Software without specific prior
 * written permission.
 * 
 * THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL PART OF THIS
 * LICENSE.  NO USE OF THE SOFTWARE IS AUTHORIZED HEREUNDER EXCEPT UNDER
 * THIS DISCLAIMER.  THE SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
 * INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  NO ASSURANCES ARE
 * PROVIDED BY THE COPYRIGHT HOLDERS THAT THE SOFTWARE DOES NOT INFRINGE
 * THE PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS OF ANY OTHER ENTITY.
 * EACH COPYRIGHT HOLDER DISCLAIMS ANY LIABILITY TO THE USER FOR CLAIMS
 * BROUGHT BY ANY OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL
 * PROPERTY RIGHTS OR OTHERWISE.  AS A CONDITION TO EXERCISING THE RIGHTS
 * GRANTED HEREUNDER, EACH USER HEREBY ASSUMES SOLE RESPONSIBILITY TO SECURE
 * ANY OTHER INTELLECTUAL PROPERTY RIGHTS NEEDED, IF ANY.  THE SOFTWARE
 * IS NOT FAULT-TOLERANT AND IS NOT INTENDED FOR USE IN MISSION-CRITICAL
 * SYSTEMS, SUCH AS THOSE USED IN THE OPERATION OF NUCLEAR FACILITIES,
 * AIRCRAFT NAVIGATION OR COMMUNICATION SYSTEMS, AIR TRAFFIC CONTROL
 * SYSTEMS, DIRECT LIFE SUPPORT MACHINES, OR WEAPONS SYSTEMS, IN WHICH
 * THE FAILURE OF THE SOFTWARE OR SYSTEM COULD LEAD DIRECTLY TO DEATH,
 * PERSONAL INJURY, OR SEVERE PHYSICAL OR ENVIRONMENTAL DAMAGE ("HIGH
 * RISK ACTIVITIES").  THE COPYRIGHT HOLDERS SPECIFICALLY DISCLAIM ANY
 * EXPRESS OR IMPLIED WARRANTY OF FITNESS FOR HIGH RISK ACTIVITIES.
 * 
 * __END_OF_JASPER_LICENSE__
 */

/*
 * Memory Allocator
 *
 * $Id$
 */

/******************************************************************************\
* Includes.
\******************************************************************************/

#include <stdio.h>

/* We need the prototype for memset. */
#include <string.h>

#include "jasper/jas_malloc.h"

/******************************************************************************\
* Code.
\******************************************************************************/

#if defined(DEBUG_MEMALLOC)
#include "../../../local/src/memalloc.c"
#endif

#if !defined(DEBUG_MEMALLOC)

static	int		init, mem_size;
static	int		**mem;

#define	INITIAL_TRACKER_SIZE	(20)
#define	GROW_TRACKER_SIZE		(10)

static void initializeMemTracker( void )
{
	#define	SIZE	INITIAL_TRACKER_SIZE * sizeof(int*)

	mem = malloc( SIZE );
	mem_size = INITIAL_TRACKER_SIZE;
	memset( mem, 0, SIZE );
	init = 1;
}

static void resizeMem( void )
{
	mem_size += GROW_TRACKER_SIZE;
	mem = realloc( mem, mem_size );
}


static void addMem( void *p )
{
	int	i;

	return;

	if( !init )
		initializeMemTracker();

	for( i=0; i<mem_size; i++ )
		if( !mem[i] )
			break;

	if( i == mem_size )
		resizeMem();

	mem[i] = (int*)p;
}

static void removeMem( void *p )
{
	int	i;

	return;

	for( i=0; i<mem_size; i++ )
		if( mem[i] == p )
		{
			mem[i] = NULL;
			return;
		}
}


/* this should be exported to the client for pool cleanup on error */
static void releaseAllMem( void )
{
	int	i;

	return;

	if( mem )
	{
		for( i=0; i<mem_size; i++ )
			if( mem[i] )
				free( mem[i] );
		free(mem);
	}
}



void *jas_malloc(size_t size)
{
	void	*p;

	p = malloc(size);
	addMem(p);
	return p;
}

void jas_free(void *ptr)
{
	free(ptr);
	removeMem(ptr);
}

void *jas_realloc(void *ptr, size_t size)
{
	void	*p;

	removeMem(ptr);
	p = realloc(ptr, size);
	addMem(p);
	return p;
}

void *jas_calloc(size_t nmemb, size_t size)
{
	void *ptr;
	size_t n;
	n = nmemb * size;
	if (!(ptr = jas_malloc(n * sizeof(char)))) {
		return 0;
	}
	memset(ptr, 0, n);
	return ptr;
}

#endif /* !defined(DEBUG_MEMALLOC) */

