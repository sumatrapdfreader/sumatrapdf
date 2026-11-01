/* D3DES (V5.09) -
 *
 * A portable, public domain, version of the Data Encryption Standard.
 *
 * Written with Symantec's THINK (Lightspeed) C by Richard Outerbridge.
 * Thanks to: Dan Hoey for his excellent Initial and Inverse permutation
 * code;  Jim Gillogly & Phil Karn for the DES key schedule code; Dennis
 * Ferguson, Eric Young and Dana How for comparing notes; and Ray Lau,
 * for humouring me on.
 *
 * THIS SOFTWARE PLACED IN THE PUBLIC DOMAIN BY THE AUTHOUR
 * 920825 19:42 EDST
 *
 * Copyright (c) 1988,1989,1990,1991,1992 by Richard Outerbridge.
 * (GEnie : OUTER; CIS : [71755,204]) Graven Imagery, 1992.
 */

#include "d3des.h"

static void scrunch(const unsigned char *, unsigned long *);
static void unscrun(unsigned long *, unsigned char *);
static void desfunc(unsigned long *, const unsigned long *);
static void cookey(DesKey *, unsigned long *);

static const unsigned short bytebit[8] = {0200, 0100, 040, 020, 010, 04, 02, 01};

static const unsigned long bigbyte[24] = {0x800000L, 0x400000L, 0x200000L, 0x100000L, 0x80000L, 0x40000L, 0x20000L, 0x10000L, 0x8000L, 0x4000L, 0x2000L, 0x1000L,
                                          0x800L,    0x400L,    0x200L,    0x100L,    0x80L,    0x40L,    0x20L,    0x10L,    0x8L,    0x4L,    0x2L,    0x1L};

/* Use the key schedule specified in the Standard (ANSI X3.92-1981). */

static const unsigned char pc1[56] = {56, 48, 40, 32, 24, 16, 8,  0, 57, 49, 41, 33, 25, 17, 9,  1, 58, 50, 42, 34, 26, 18, 10, 2, 59, 51, 43, 35,
                                      62, 54, 46, 38, 30, 22, 14, 6, 61, 53, 45, 37, 29, 21, 13, 5, 60, 52, 44, 36, 28, 20, 12, 4, 27, 19, 11, 3};

static const unsigned char totrot[16] = {1, 2, 4, 6, 8, 10, 12, 14, 15, 17, 19, 21, 23, 25, 27, 28};

static const unsigned char pc2[48] = {13, 16, 10, 23, 0,  4,  2,  27, 14, 5,  20, 9,  22, 18, 11, 3,  25, 7,  15, 6,  26, 19, 12, 1,
                                      40, 51, 30, 36, 46, 54, 29, 39, 50, 44, 32, 47, 43, 48, 38, 55, 33, 52, 45, 41, 49, 35, 28, 31};

void
deskey(DesKey *dk, const unsigned char *key, short edf) /* Thanks to James Gillogly & Phil Karn! */
{
    int i, j, l, m, n;
    unsigned char pc1m[56], pcr[56];
    unsigned long kn[32];

    for (j = 0; j < 56; j++) {
        l = pc1[j];
        m = l & 07;
        pc1m[j] = (key[l >> 3] & bytebit[m]) ? 1 : 0;
    }
    for (i = 0; i < 16; i++) {
        if (edf == DE1) m = (15 - i) << 1;
        else m = i << 1;
        n = m + 1;
        kn[m] = kn[n] = 0L;
        for (j = 0; j < 28; j++) {
            l = j + totrot[i];
            if (l < 28) pcr[j] = pc1m[l];
            else pcr[j] = pc1m[l - 28];
        }
        for (j = 28; j < 56; j++) {
            l = j + totrot[i];
            if (l < 56) pcr[j] = pc1m[l];
            else pcr[j] = pc1m[l - 28];
        }
        for (j = 0; j < 24; j++) {
            if (pcr[pc2[j]]) kn[m] |= bigbyte[j];
            if (pcr[pc2[j + 24]]) kn[n] |= bigbyte[j];
        }
    }
    cookey(dk, kn);
    return;
}

static void
cookey(DesKey *dk, unsigned long *raw1) {
    unsigned long *cook, *raw0;
    int i;

    cook = dk->kn;
    for (i = 0; i < 16; i++, raw1++) {
        raw0 = raw1++;
        *cook = (*raw0 & 0x00fc0000L) << 6;
        *cook |= (*raw0 & 0x00000fc0L) << 10;
        *cook |= (*raw1 & 0x00fc0000L) >> 10;
        *cook++ |= (*raw1 & 0x00000fc0L) >> 6;
        *cook = (*raw0 & 0x0003f000L) << 12;
        *cook |= (*raw0 & 0x0000003fL) << 16;
        *cook |= (*raw1 & 0x0003f000L) >> 4;
        *cook++ |= (*raw1 & 0x0000003fL);
    }
    return;
}

void
des(const DesKey *dk, const unsigned char *inblock, unsigned char *outblock) {
    unsigned long work[2];

    scrunch(inblock, work);
    desfunc(work, dk->kn);
    unscrun(work, outblock);
    return;
}


static void
scrunch(const unsigned char *outof, unsigned long *into) {
    *into = (*outof++ & 0xffUL) << 24;
    *into |= (*outof++ & 0xffUL) << 16;
    *into |= (*outof++ & 0xffUL) << 8;
    *into++ |= (*outof++ & 0xffUL);
    *into = (*outof++ & 0xffUL) << 24;
    *into |= (*outof++ & 0xffUL) << 16;
    *into |= (*outof++ & 0xffUL) << 8;
    *into |= (*outof & 0xffUL);
    return;
}


static void
unscrun(unsigned long *outof, unsigned char *into) {
    *into++ = (*outof >> 24) & 0xffL;
    *into++ = (*outof >> 16) & 0xffL;
    *into++ = (*outof >> 8) & 0xffL;
    *into++ = *outof++ & 0xffL;
    *into++ = (*outof >> 24) & 0xffL;
    *into++ = (*outof >> 16) & 0xffL;
    *into++ = (*outof >> 8) & 0xffL;
    *into = *outof & 0xffL;
    return;
}

/* Not the standard DES tables, so the output isn't standard DES. LIT needs these. */
/*--[crypto/des/spr.h]---------------------------------------------------------
 | Copyright (C) 2002 Dan A. Jackson
 |
 | This file is part of the "openclit" library for processing .LIT files.
 |
 | "Openclit" is free software; you can redistribute it and/or modify
 | it under the terms of the GNU General Public License as published by
 | the Free Software Foundation; either version 2 of the License, or
 | (at your option) any later version.
 |
 | This program is distributed in the hope that it will be useful,
 | but WITHOUT ANY WARRANTY; without even the implied warranty of
 | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 | GNU General Public License for more details.
 |
 | You should have received a copy of the GNU General Public License
 | along with this program; if not, write to the Free Software
 | Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 |
 | The GNU General Public License may also be available at the following
 | URL: http://www.gnu.org/licenses/gpl.html
*/

static const unsigned long SP1[64] = {
    0x02080800L, 0x00080000L, 0x02000002L, 0x02080802L, 0x02000000L, 0x00080802L, 0x00080002L, 0x02000002L, 0x00080802L, 0x02080800L, 0x02080000L,
    0x00000802L, 0x02000802L, 0x02000000L, 0x00000000L, 0x00080002L, 0x00080000L, 0x00000002L, 0x02000800L, 0x00080800L, 0x02080802L, 0x02080000L,
    0x00000802L, 0x02000800L, 0x00000002L, 0x00000800L, 0x00080800L, 0x02080002L, 0x00000800L, 0x02000802L, 0x02080002L, 0x00000000L, 0x00000000L,
    0x02080802L, 0x02000800L, 0x00080002L, 0x02080800L, 0x00080000L, 0x00000802L, 0x02000800L, 0x02080002L, 0x00000800L, 0x00080800L, 0x02000002L,
    0x00080802L, 0x00000002L, 0x02000002L, 0x02080000L, 0x02080802L, 0x00080800L, 0x02080000L, 0x02000802L, 0x02000000L, 0x00000802L, 0x00080002L,
    0x00000000L, 0x00080000L, 0x02000000L, 0x02000802L, 0x02080800L, 0x00000002L, 0x02080002L, 0x00000800L, 0x00080802L};
static const unsigned long SP2[64] = {
    0x40108010L, 0x00000000L, 0x00108000L, 0x40100000L, 0x40000010L, 0x00008010L, 0x40008000L, 0x00108000L, 0x00008000L, 0x40100010L, 0x00000010L,
    0x40008000L, 0x00100010L, 0x40108000L, 0x40100000L, 0x00000010L, 0x00100000L, 0x40008010L, 0x40100010L, 0x00008000L, 0x00108010L, 0x40000000L,
    0x00000000L, 0x00100010L, 0x40008010L, 0x00108010L, 0x40108000L, 0x40000010L, 0x40000000L, 0x00100000L, 0x00008010L, 0x40108010L, 0x00100010L,
    0x40108000L, 0x40008000L, 0x00108010L, 0x40108010L, 0x00100010L, 0x40000010L, 0x00000000L, 0x40000000L, 0x00008010L, 0x00100000L, 0x40100010L,
    0x00008000L, 0x40000000L, 0x00108010L, 0x40008010L, 0x40108000L, 0x00008000L, 0x00000000L, 0x40000010L, 0x00000010L, 0x40108010L, 0x00108000L,
    0x40100000L, 0x40100010L, 0x00100000L, 0x00008010L, 0x40008000L, 0x40008010L, 0x00000010L, 0x40100000L, 0x00108000L};
static const unsigned long SP3[64] = {
    0x04000001L, 0x04040100L, 0x00000100L, 0x04000101L, 0x00040001L, 0x04000000L, 0x04000101L, 0x00040100L, 0x04000100L, 0x00040000L, 0x04040000L,
    0x00000001L, 0x04040101L, 0x00000101L, 0x00000001L, 0x04040001L, 0x00000000L, 0x00040001L, 0x04040100L, 0x00000100L, 0x00000101L, 0x04040101L,
    0x00040000L, 0x04000001L, 0x04040001L, 0x04000100L, 0x00040101L, 0x04040000L, 0x00040100L, 0x00000000L, 0x04000000L, 0x00040101L, 0x04040100L,
    0x00000100L, 0x00000001L, 0x00040000L, 0x00000101L, 0x00040001L, 0x04040000L, 0x04000101L, 0x00000000L, 0x04040100L, 0x00040100L, 0x04040001L,
    0x00040001L, 0x04000000L, 0x04040101L, 0x00000001L, 0x00040101L, 0x04000001L, 0x04000000L, 0x04040101L, 0x00040000L, 0x04000100L, 0x04000101L,
    0x00040100L, 0x04000100L, 0x00000000L, 0x04040001L, 0x00000101L, 0x04000001L, 0x00040101L, 0x00000100L, 0x04040000L};
static const unsigned long SP4[64] = {
    0x00401008L, 0x10001000L, 0x00000008L, 0x10401008L, 0x00000000L, 0x10400000L, 0x10001008L, 0x00400008L, 0x10401000L, 0x10000008L, 0x10000000L,
    0x00001008L, 0x10000008L, 0x00401008L, 0x00400000L, 0x10000000L, 0x10400008L, 0x00401000L, 0x00001000L, 0x00000008L, 0x00401000L, 0x10001008L,
    0x10400000L, 0x00001000L, 0x00001008L, 0x00000000L, 0x00400008L, 0x10401000L, 0x10001000L, 0x10400008L, 0x10401008L, 0x00400000L, 0x10400008L,
    0x00001008L, 0x00400000L, 0x10000008L, 0x00401000L, 0x10001000L, 0x00000008L, 0x10400000L, 0x10001008L, 0x00000000L, 0x00001000L, 0x00400008L,
    0x00000000L, 0x10400008L, 0x10401000L, 0x00001000L, 0x10000000L, 0x10401008L, 0x00401008L, 0x00400000L, 0x10401008L, 0x00000008L, 0x10001000L,
    0x00401008L, 0x00400008L, 0x00401000L, 0x10400000L, 0x10001008L, 0x00001008L, 0x10000000L, 0x10000008L, 0x10401000L};
static const unsigned long SP5[64] = {
    0x08000000L, 0x00010000L, 0x00000400L, 0x08010420L, 0x08010020L, 0x08000400L, 0x00010420L, 0x08010000L, 0x00010000L, 0x00000020L, 0x08000020L,
    0x00010400L, 0x08000420L, 0x08010020L, 0x08010400L, 0x00000000L, 0x00010400L, 0x08000000L, 0x00010020L, 0x00000420L, 0x08000400L, 0x00010420L,
    0x00000000L, 0x08000020L, 0x00000020L, 0x08000420L, 0x08010420L, 0x00010020L, 0x08010000L, 0x00000400L, 0x00000420L, 0x08010400L, 0x08010400L,
    0x08000420L, 0x00010020L, 0x08010000L, 0x00010000L, 0x00000020L, 0x08000020L, 0x08000400L, 0x08000000L, 0x00010400L, 0x08010420L, 0x00000000L,
    0x00010420L, 0x08000000L, 0x00000400L, 0x00010020L, 0x08000420L, 0x00000400L, 0x00000000L, 0x08010420L, 0x08010020L, 0x08010400L, 0x00000420L,
    0x00010000L, 0x00010400L, 0x08010020L, 0x08000400L, 0x00000420L, 0x00000020L, 0x00010420L, 0x08010000L, 0x08000020L};
static const unsigned long SP6[64] = {
    0x80000040L, 0x00200040L, 0x00000000L, 0x80202000L, 0x00200040L, 0x00002000L, 0x80002040L, 0x00200000L, 0x00002040L, 0x80202040L, 0x00202000L,
    0x80000000L, 0x80002000L, 0x80000040L, 0x80200000L, 0x00202040L, 0x00200000L, 0x80002040L, 0x80200040L, 0x00000000L, 0x00002000L, 0x00000040L,
    0x80202000L, 0x80200040L, 0x80202040L, 0x80200000L, 0x80000000L, 0x00002040L, 0x00000040L, 0x00202000L, 0x00202040L, 0x80002000L, 0x00002040L,
    0x80000000L, 0x80002000L, 0x00202040L, 0x80202000L, 0x00200040L, 0x00000000L, 0x80002000L, 0x80000000L, 0x00002000L, 0x80200040L, 0x00200000L,
    0x00200040L, 0x80202040L, 0x00202000L, 0x00000040L, 0x80202040L, 0x00202000L, 0x00200000L, 0x80002040L, 0x80000040L, 0x80200000L, 0x00202040L,
    0x00000000L, 0x00002000L, 0x80000040L, 0x80002040L, 0x80202000L, 0x80200000L, 0x00002040L, 0x00000040L, 0x80200040L,
};
static const unsigned long SP7[64] = {
    0x00004000L, 0x00000200L, 0x01000200L, 0x01000004L, 0x01004204L, 0x00004004L, 0x00004200L, 0x00000000L, 0x01000000L, 0x01000204L, 0x00000204L,
    0x01004000L, 0x00000004L, 0x01004200L, 0x01004000L, 0x00000204L, 0x01000204L, 0x00004000L, 0x00004004L, 0x01004204L, 0x00000000L, 0x01000200L,
    0x01000004L, 0x00004200L, 0x01004004L, 0x00004204L, 0x01004200L, 0x00000004L, 0x00004204L, 0x01004004L, 0x00000200L, 0x01000000L, 0x00004204L,
    0x01004000L, 0x01004004L, 0x00000204L, 0x00004000L, 0x00000200L, 0x01000000L, 0x01004004L, 0x01000204L, 0x00004204L, 0x00004200L, 0x00000000L,
    0x00000200L, 0x01000004L, 0x00000004L, 0x01000200L, 0x00000000L, 0x01000204L, 0x01000200L, 0x00004200L, 0x00000204L, 0x00004000L, 0x01004204L,
    0x01000000L, 0x01004200L, 0x00000004L, 0x00004004L, 0x01004204L, 0x01000004L, 0x01004200L, 0x01004000L, 0x00004004L,
};
static const unsigned long SP8[64] = {
    0x20800080L, 0x20820000L, 0x00020080L, 0x00000000L, 0x20020000L, 0x00800080L, 0x20800000L, 0x20820080L, 0x00000080L, 0x20000000L, 0x00820000L,
    0x00020080L, 0x00820080L, 0x20020080L, 0x20000080L, 0x20800000L, 0x00020000L, 0x00820080L, 0x00800080L, 0x20020000L, 0x20820080L, 0x20000080L,
    0x00000000L, 0x00820000L, 0x20000000L, 0x00800000L, 0x20020080L, 0x20800080L, 0x00800000L, 0x00020000L, 0x20820000L, 0x00000080L, 0x00800000L,
    0x00020000L, 0x20000080L, 0x20820080L, 0x00020080L, 0x20000000L, 0x00000000L, 0x00820000L, 0x20800080L, 0x20020080L, 0x20020000L, 0x00800080L,
    0x20820000L, 0x00000080L, 0x00800080L, 0x20020000L, 0x20820080L, 0x00800000L, 0x20800000L, 0x20000080L, 0x00820000L, 0x00020080L, 0x20020080L,
    0x20800000L, 0x00000080L, 0x20820000L, 0x00820080L, 0x00000000L, 0x20000000L, 0x20800080L, 0x00020000L, 0x00820080L,
};


static void
desfunc(unsigned long *block, const unsigned long *keys) {
    unsigned long fval, work, right, leftt;
    int round;

    leftt = block[0];
    right = block[1];
    work = ((leftt >> 4) ^ right) & 0x0f0f0f0fL;
    right ^= work;
    leftt ^= (work << 4);
    work = ((leftt >> 16) ^ right) & 0x0000ffffL;
    right ^= work;
    leftt ^= (work << 16);
    work = ((right >> 2) ^ leftt) & 0x33333333L;
    leftt ^= work;
    right ^= (work << 2);
    work = ((right >> 8) ^ leftt) & 0x00ff00ffL;
    leftt ^= work;
    right ^= (work << 8);
    right = ((right << 1) | ((right >> 31) & 1L)) & 0xffffffffL;
    work = (leftt ^ right) & 0xaaaaaaaaL;
    leftt ^= work;
    right ^= work;
    leftt = ((leftt << 1) | ((leftt >> 31) & 1L)) & 0xffffffffL;

    for (round = 0; round < 8; round++) {
        work = (right << 28) | (right >> 4);
        work ^= *keys++;
        fval = SP7[work & 0x3fL];
        fval |= SP5[(work >> 8) & 0x3fL];
        fval |= SP3[(work >> 16) & 0x3fL];
        fval |= SP1[(work >> 24) & 0x3fL];
        work = right ^ *keys++;
        fval |= SP8[work & 0x3fL];
        fval |= SP6[(work >> 8) & 0x3fL];
        fval |= SP4[(work >> 16) & 0x3fL];
        fval |= SP2[(work >> 24) & 0x3fL];
        leftt ^= fval;
        work = (leftt << 28) | (leftt >> 4);
        work ^= *keys++;
        fval = SP7[work & 0x3fL];
        fval |= SP5[(work >> 8) & 0x3fL];
        fval |= SP3[(work >> 16) & 0x3fL];
        fval |= SP1[(work >> 24) & 0x3fL];
        work = leftt ^ *keys++;
        fval |= SP8[work & 0x3fL];
        fval |= SP6[(work >> 8) & 0x3fL];
        fval |= SP4[(work >> 16) & 0x3fL];
        fval |= SP2[(work >> 24) & 0x3fL];
        right ^= fval;
    }

    right = (right << 31) | (right >> 1);
    work = (leftt ^ right) & 0xaaaaaaaaL;
    leftt ^= work;
    right ^= work;
    leftt = (leftt << 31) | (leftt >> 1);
    work = ((leftt >> 8) ^ right) & 0x00ff00ffL;
    right ^= work;
    leftt ^= (work << 8);
    work = ((leftt >> 2) ^ right) & 0x33333333L;
    right ^= work;
    leftt ^= (work << 2);
    work = ((right >> 16) ^ leftt) & 0x0000ffffL;
    leftt ^= work;
    right ^= (work << 16);
    work = ((right >> 4) ^ leftt) & 0x0f0f0f0fL;
    leftt ^= work;
    right ^= (work << 4);
    *block++ = right;
    *block = leftt;
    return;
}

/* d3des V5.09 rwo 9208.04 20:31 Graven Imagery
 **********************************************************************/
