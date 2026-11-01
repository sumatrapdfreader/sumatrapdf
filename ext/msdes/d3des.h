/* d3des.h -
 *
 * Headers and defines for d3des.c
 * Graven Imagery, 1992.
 *
 * THIS SOFTWARE PLACED IN THE PUBLIC DOMAIN BY THE AUTHOUR
 * 920825 19:42 EDST
 *
 * Copyright (c) 1988,1989,1990,1991,1992 by Richard Outerbridge
 *	(GEnie : OUTER; CIS : [71755,204])
 */

#pragma once

#define EN0 0 /* MODE == encrypt */
#define DE1 1 /* MODE == decrypt */

/* Key schedule owned by the caller, so concurrent users don't share state. */
typedef struct DesKey {
    unsigned long kn[32];
} DesKey;

extern void deskey(DesKey *, const unsigned char *, short);
/*		      schedule, hexkey[8]     MODE
 * Sets the key schedule according to the hexadecimal
 * key contained in the 8 bytes of hexkey, according to the DES,
 * for encryption or decryption according to MODE.
 */

extern void des(const DesKey *, const unsigned char *, unsigned char *);
/*		    schedule, from[8]	      to[8]
 * Encrypts/Decrypts (according to the key schedule) one block of
 * eight bytes at address 'from' into the block at address 'to'.
 * They can be the same.
 */

/* d3des.h V5.09 rwo 9208.04 15:06 Graven Imagery
 ********************************************************************/
