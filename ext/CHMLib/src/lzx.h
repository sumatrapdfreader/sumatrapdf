/* $Id: lzx.h,v 1.5 2002/10/08 03:43:33 jedwin Exp $ */
/***************************************************************************
 *                        lzx.h - LZX decompression routines               *
 *                           -------------------                           *
 *                                                                         *
 *  maintainer: Jed Wing <jedwin@ugcs.caltech.edu>                         *
 *  source:     modified lzx.c from cabextract v0.5                        *
 *  notes:      This file was taken from cabextract v0.5, which was,       *
 *              itself, a modified version of the lzx decompression code   *
 *              from unlzx.                                                *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.  Note that an exemption to this   *
 *   license has been granted by Stuart Caie for the purposes of           *
 *   distribution with chmlib.  This does not, to the best of my           *
 *   knowledge, constitute a change in the license of this (the LZX) code  *
 *   in general.                                                           *
 *                                                                         *
 ***************************************************************************/

#ifndef INCLUDED_LZX_H
#define INCLUDED_LZX_H

#ifdef __cplusplus
extern "C" {
#endif

/* return codes */
#define DECR_OK           (0)
#define DECR_DATAFORMAT   (1)
#define DECR_ILLEGALDATA  (2)
#define DECR_NOMEMORY     (3)

/* opaque state structure */
struct LZXstate;

/* create an lzx state object */
struct LZXstate *LZXinit(int window);

/* destroy an lzx state object */
void LZXteardown(struct LZXstate *pState);

/* reset an lzx stream */
int LZXreset(struct LZXstate *pState);

/* decompress an LZX compressed block */
int LZXdecompress(struct LZXstate *pState,
                  unsigned char *inpos,
                  unsigned char *outpos,
                  int inlen,
                  int outlen);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_LZX_H */
