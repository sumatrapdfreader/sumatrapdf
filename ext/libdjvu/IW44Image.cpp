//C-  -*- C++ -*-
//C- -------------------------------------------------------------------
//C- DjVuLibre-3.5
//C- Copyright (c) 2002  Leon Bottou and Yann Le Cun.
//C- Copyright (c) 2001  AT&T
//C-
//C- This software is subject to, and may be distributed under, the
//C- GNU General Public License, either Version 2 of the license,
//C- or (at your option) any later version. The license should have
//C- accompanied the software or you may obtain a copy of the license
//C- from the Free Software Foundation at http://www.fsf.org .
//C-
//C- This program is distributed in the hope that it will be useful,
//C- but WITHOUT ANY WARRANTY; without even the implied warranty of
//C- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//C- GNU General Public License for more details.
//C- 
//C- DjVuLibre-3.5 is derived from the DjVu(r) Reference Library from
//C- Lizardtech Software.  Lizardtech Software has authorized us to
//C- replace the original DjVu(r) Reference Library notice by the following
//C- text (see doc/lizard2002.djvu and doc/lizardtech2007.djvu):
//C-
//C-  ------------------------------------------------------------------
//C- | DjVu (r) Reference Library (v. 3.5)
//C- | Copyright (c) 1999-2001 LizardTech, Inc. All Rights Reserved.
//C- | The DjVu Reference Library is protected by U.S. Pat. No.
//C- | 6,058,214 and patents pending.
//C- |
//C- | This software is subject to, and may be distributed under, the
//C- | GNU General Public License, either Version 2 of the license,
//C- | or (at your option) any later version. The license should have
//C- | accompanied the software or you may obtain a copy of the license
//C- | from the Free Software Foundation at http://www.fsf.org .
//C- |
//C- | The computer code originally released by LizardTech under this
//C- | license and unmodified by other parties is deemed "the LIZARDTECH
//C- | ORIGINAL CODE."  Subject to any third party intellectual property
//C- | claims, LizardTech grants recipient a worldwide, royalty-free, 
//C- | non-exclusive license to make, use, sell, or otherwise dispose of 
//C- | the LIZARDTECH ORIGINAL CODE or of programs derived from the 
//C- | LIZARDTECH ORIGINAL CODE in compliance with the terms of the GNU 
//C- | General Public License.   This grant only confers the right to 
//C- | infringe patent claims underlying the LIZARDTECH ORIGINAL CODE to 
//C- | the extent such infringement is reasonably necessary to enable 
//C- | recipient to make, have made, practice, sell, or otherwise dispose 
//C- | of the LIZARDTECH ORIGINAL CODE (or portions thereof) and not to 
//C- | any greater extent that may be necessary to utilize further 
//C- | modifications or combinations.
//C- |
//C- | The LIZARDTECH ORIGINAL CODE is provided "AS IS" WITHOUT WARRANTY
//C- | OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//C- | TO ANY WARRANTY OF NON-INFRINGEMENT, OR ANY IMPLIED WARRANTY OF
//C- | MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
//C- +------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma implementation
#endif

// - Author: Leon Bottou, 08/1998

// From: Leon Bottou, 1/31/2002
// Lizardtech has split this file into a decoder and an encoder.
// Only superficial changes.  The meat is mine.

#define IW44IMAGE_IMPLIMENTATION /* */
// -------------------^  not my spelling mistake (Leon Bottou)

#include "IW44Image.h"
#include "ZPCodec.h"
#include "GBitmap.h"
#include "GPixmap.h"
#include "IFFByteStream.h"
#include "GRect.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "MMX.h"
#undef IWTRANSFORM_TIMER
#ifdef IWTRANSFORM_TIMER
#include "GOS.h"
#endif

#include <assert.h>
#include <string.h>
#include <math.h>


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


#define IWALLOCSIZE    4080
#define IWCODEC_MAJOR     1
#define IWCODEC_MINOR     2
#define DECIBEL_PRUNE   5.0


//////////////////////////////////////////////////////
// WAVELET DECOMPOSITION CONSTANTS
//////////////////////////////////////////////////////

// Parameters for IW44 wavelet.
// - iw_quant: quantization for all 16 sub-bands
// - iw_norm: norm of all wavelets (for db estimation)
// - iw_border: pixel border required to run filters
// - iw_shift: scale applied before decomposition


static const int iw_quant[16] = {
  0x004000, 
  0x008000, 0x008000, 0x010000,
  0x010000, 0x010000, 0x020000,
  0x020000, 0x020000, 0x040000,
  0x040000, 0x040000, 0x080000, 
  0x040000, 0x040000, 0x080000
};

static const int iw_border = 3;
static const int iw_shift  = 6;
static const int iw_round  = (1<<(iw_shift-1));

class IW44Image::Codec::Decode : public IW44Image::Codec 
{
public:
  // Construction
  Decode(IW44Image::Map &map) : Codec(map) {}
  // Coding
  virtual int code_slice(ZPCodec &zp);
};

//////////////////////////////////////////////////////
// MMX IMPLEMENTATION HELPERS
//////////////////////////////////////////////////////


// Note:
// MMX implementation for vertical transforms only.
// Speedup is basically related to faster memory transfer
// The IW44 transform is not CPU bound, it is memory bound.

#ifdef MMX

static const short w9[]  = {9,9,9,9};
static const short w1[]  = {1,1,1,1};
static const int   d8[]  = {8,8};
static const int   d16[] = {16,16};

static void
mmx_bv_1 ( short* &q, short* e, int s, int s3 )
{
  while (q<e && (((size_t)q)&0x7))
    {
      int a = (int)q[-s] + (int)q[s];
      int b = (int)q[-s3] + (int)q[s3];
      *q -= (((a<<3)+a-b+16)>>5);
      q ++;
  }
  while (q+3 < e)
    {
      MMXar( movq,       q-s,mm0);  // MM0=[ b3, b2, b1, b0 ]
      MMXar( movq,       q+s,mm2);  // MM2=[ c3, c2, c1, c0 ]
      MMXrr( movq,       mm0,mm1);  
      MMXrr( punpcklwd,  mm2,mm0);  // MM0=[ c1, b1, c0, b0 ]
      MMXrr( punpckhwd,  mm2,mm1);  // MM1=[ c3, b3, c2, b2 ]
      MMXar( pmaddwd,    w9,mm0);   // MM0=[ (c1+b1)*9, (c0+b0)*9 ]
      MMXar( pmaddwd,    w9,mm1);   // MM1=[ (c3+b3)*9, (c2+b2)*9 ]
      MMXar( movq,       q-s3,mm2);
      MMXar( movq,       q+s3,mm4);
      MMXrr( movq,       mm2,mm3);
      MMXrr( punpcklwd,  mm4,mm2);  // MM2=[ d1, a1, d0, a0 ]
      MMXrr( punpckhwd,  mm4,mm3);  // MM3=[ d3, a3, d2, a2 ]
      MMXar( pmaddwd,    w1,mm2);   // MM2=[ (a1+d1)*1, (a0+d0)*1 ]
      MMXar( pmaddwd,    w1,mm3);   // MM3=[ (a3+d3)*1, (a2+d2)*1 ]
      MMXar( paddd,      d16,mm0);
      MMXar( paddd,      d16,mm1);
      MMXrr( psubd,      mm2,mm0);  // MM0=[ (c1+b1)*9-a1-d1+8, ...
      MMXrr( psubd,      mm3,mm1);  // MM1=[ (c3+b3)*9-a3-d3+8, ...
      MMXir( psrad,      5,mm0);
      MMXar( movq,       q,mm7);    // MM7=[ p3,p2,p1,p0 ]
      MMXir( psrad,      5,mm1);
      MMXrr( packssdw,   mm1,mm0);  // MM0=[ x3,x2,x1,x0 ]
      MMXrr( psubw,      mm0,mm7);  // MM7=[ p3-x3, p2-x2, ... ]
      MMXra( movq,       mm7,q);
#if defined(_MSC_VER) && defined(_DEBUG)
      MMXemms;
#endif
      q += 4;
    }
}


static void
mmx_bv_2 ( short* &q, short* e, int s, int s3 )
{
  while (q<e && (((size_t)q)&0x7))
    {
      int a = (int)q[-s] + (int)q[s];
      int b = (int)q[-s3] + (int)q[s3];
      *q += (((a<<3)+a-b+8)>>4);
      q ++;
    }
  while (q+3 < e)
    {
      MMXar( movq,       q-s,mm0);  // MM0=[ b3, b2, b1, b0 ]
      MMXar( movq,       q+s,mm2);  // MM2=[ c3, c2, c1, c0 ]
      MMXrr( movq,       mm0,mm1);  
      MMXrr( punpcklwd,  mm2,mm0);  // MM0=[ c1, b1, c0, b0 ]
      MMXrr( punpckhwd,  mm2,mm1);  // MM1=[ c3, b3, c2, b2 ]
      MMXar( pmaddwd,    w9,mm0);   // MM0=[ (c1+b1)*9, (c0+b0)*9 ]
      MMXar( pmaddwd,    w9,mm1);   // MM1=[ (c3+b3)*9, (c2+b2)*9 ]
      MMXar( movq,       q-s3,mm2);
      MMXar( movq,       q+s3,mm4);
      MMXrr( movq,       mm2,mm3);
      MMXrr( punpcklwd,  mm4,mm2);  // MM2=[ d1, a1, d0, a0 ]
      MMXrr( punpckhwd,  mm4,mm3);  // MM3=[ d3, a3, d2, a2 ]
      MMXar( pmaddwd,    w1,mm2);   // MM2=[ (a1+d1)*1, (a0+d0)*1 ]
      MMXar( pmaddwd,    w1,mm3);   // MM3=[ (a3+d3)*1, (a2+d2)*1 ]
      MMXar( paddd,      d8,mm0);
      MMXar( paddd,      d8,mm1);
      MMXrr( psubd,      mm2,mm0);  // MM0=[ (c1+b1)*9-a1-d1+8, ...
      MMXrr( psubd,      mm3,mm1);  // MM1=[ (c3+b3)*9-a3-d3+8, ...
      MMXir( psrad,      4,mm0);
      MMXar( movq,       q,mm7);    // MM7=[ p3,p2,p1,p0 ]
      MMXir( psrad,      4,mm1);
      MMXrr( packssdw,   mm1,mm0);  // MM0=[ x3,x2,x1,x0 ]
      MMXrr( paddw,      mm0,mm7);  // MM7=[ p3+x3, p2+x2, ... ]
      MMXra( movq,       mm7,q);
#if defined(_MSC_VER) && defined(_DEBUG)
      MMXemms;
#endif
      q += 4;
    }
}
#endif /* MMX */

static void 
filter_bv(short *p, int w, int h, int rowsize, int scale)
{
  int y = 0;
  int s = scale*rowsize;
  int s3 = s+s+s;
  h = ((h-1)/scale)+1;
  while (y-3 < h)
    {
      // 1-Lifting
      {
        short *q = p;
        short *e = q+w;
        if (y>=3 && y+3<h)
          {
            // Generic case
#ifdef MMX
            if (scale==1 && MMXControl::mmxflag>0)
              mmx_bv_1(q, e, s, s3);
#endif
            while (q<e)
              {
                int a = (int)q[-s] + (int)q[s];
                int b = (int)q[-s3] + (int)q[s3];
                *q -= (((a<<3)+a-b+16)>>5);
                q += scale;
              }
          }
        else if (y<h)
          {
            // Special cases
            short *q1 = (y+1<h ? q+s : 0);
            short *q3 = (y+3<h ? q+s3 : 0);
            if (y>=3)
              {
                while (q<e)
                  {
                    int a = (int)q[-s] + (q1 ? (int)(*q1) : 0);
                    int b = (int)q[-s3] + (q3 ? (int)(*q3) : 0);
                    *q -= (((a<<3)+a-b+16)>>5);
                    q += scale;
                    if (q1) q1 += scale;
                    if (q3) q3 += scale;
                  }
              }
            else if (y>=1)
              {
                while (q<e)
                  {
                    int a = (int)q[-s] + (q1 ? (int)(*q1) : 0);
                    int b = (q3 ? (int)(*q3) : 0);
                    *q -= (((a<<3)+a-b+16)>>5);
                    q += scale;
                    if (q1) q1 += scale;
                    if (q3) q3 += scale;
                  }
              }
            else
              {
                while (q<e)
                  {
                    int a = (q1 ? (int)(*q1) : 0);
                    int b = (q3 ? (int)(*q3) : 0);
                    *q -= (((a<<3)+a-b+16)>>5);
                    q += scale;
                    if (q1) q1 += scale;
                    if (q3) q3 += scale;
                  }
              }
          }
      }
      // 2-Interpolation
      {
        short *q = p-s3;
        short *e = q+w;
        if (y>=6 && y<h)
          {
            // Generic case
#ifdef MMX
            if (scale==1 && MMXControl::mmxflag>0)
              mmx_bv_2(q, e, s, s3);
#endif
            while (q<e)
              {
                int a = (int)q[-s] + (int)q[s];
                int b = (int)q[-s3] + (int)q[s3];
                *q += (((a<<3)+a-b+8)>>4);
                q += scale;
              }
          }
        else if (y>=3)
          {
            // Special cases
            short *q1 = (y-2<h ? q+s : q-s);
            while (q<e)
              {
                int a = (int)q[-s] + (int)(*q1);
                *q += ((a+1)>>1);
                q += scale;
                q1 += scale;
              }
          }
      }
      y += 2;
      p += s+s;
    }
}

static void 
filter_bh(short *p, int w, int h, int rowsize, int scale)
{
  int y = 0;
  int s = scale;
  int s3 = s+s+s;
  rowsize *= scale;
  while (y<h)
    {
      short *q = p;
      short *e = p+w;
      int a0=0, a1=0, a2=0, a3=0;
      int b0=0, b1=0, b2=0, b3=0;
      if (q<e)
        {
          // Special case:  x=0
          if (q+s < e)
            a2 = q[s];
          if (q+s3 < e)
            a3 = q[s3];
          b2 = b3 = q[0] - ((((a1+a2)<<3)+(a1+a2)-a0-a3+16) >> 5);
          q[0] = b3;
          q += s+s;
        }
      if (q<e)
        {
          // Special case:  x=2
          a0 = a1;
          a1 = a2;
          a2 = a3;
          if (q+s3 < e)
            a3 = q[s3];
          b3 = q[0] - ((((a1+a2)<<3)+(a1+a2)-a0-a3+16) >> 5);
          q[0] = b3;
          q += s+s;
        }
      if (q<e)
        {
          // Special case:  x=4
          b1 = b2;
          b2 = b3;
          a0 = a1;
          a1 = a2;
          a2 = a3;
          if (q+s3 < e)
            a3 = q[s3];
          b3 = q[0] - ((((a1+a2)<<3)+(a1+a2)-a0-a3+16) >> 5);
          q[0] = b3;
          q[-s3] = q[-s3] + ((b1+b2+1)>>1);
          q += s+s;
        }
      while (q+s3 < e)
        {
          // Generic case
          a0=a1; 
          a1=a2; 
          a2=a3;
          a3=q[s3];
          b0=b1; 
          b1=b2; 
          b2=b3;
          b3 = q[0] - ((((a1+a2)<<3)+(a1+a2)-a0-a3+16) >> 5);
          q[0] = b3;
          q[-s3] = q[-s3] + ((((b1+b2)<<3)+(b1+b2)-b0-b3+8) >> 4);
          q += s+s;
        }
      while (q < e)
        {
          // Special case:  w-3 <= x < w
          a0=a1;
          a1=a2; 
          a2=a3;
          a3=0;
          b0=b1; 
          b1=b2; 
          b2=b3;
          b3 = q[0] - ((((a1+a2)<<3)+(a1+a2)-a0-a3+16) >> 5);
          q[0] = b3;
          q[-s3] = q[-s3] + ((((b1+b2)<<3)+(b1+b2)-b0-b3+8) >> 4);
          q += s+s;
        }
      while (q-s3 < e)
        {
          // Special case  w <= x < w+3
          b0=b1; 
          b1=b2; 
          b2=b3;
          if (q-s3 >= p)
            q[-s3] = q[-s3] + ((b1+b2+1)>>1);
          q += s+s;
        }
      y += scale;
      p += rowsize;
    }
}


//////////////////////////////////////////////////////
// REPRESENTATION OF WAVELET DECOMPOSED IMAGES
//////////////////////////////////////////////////////



//---------------------------------------------------------------
// Zig zag location in a 1024 liftblock.
// These numbers have been generated with the following program:
//
// int x=0, y=0;
// for (int i=0; i<5; i++) {
//   x = (x<<1) | (n&1);  n >>= 1;
//   y = (y<<1) | (n&1);  n >>= 1;
// }


static int zigzagloc[1024] = {
   0,  16, 512, 528,   8,  24, 520, 536, 256, 272, 768, 784, 264, 280, 776, 792,
   4,  20, 516, 532,  12,  28, 524, 540, 260, 276, 772, 788, 268, 284, 780, 796,
 128, 144, 640, 656, 136, 152, 648, 664, 384, 400, 896, 912, 392, 408, 904, 920,
 132, 148, 644, 660, 140, 156, 652, 668, 388, 404, 900, 916, 396, 412, 908, 924,
   2,  18, 514, 530,  10,  26, 522, 538, 258, 274, 770, 786, 266, 282, 778, 794,
   6,  22, 518, 534,  14,  30, 526, 542, 262, 278, 774, 790, 270, 286, 782, 798,
 130, 146, 642, 658, 138, 154, 650, 666, 386, 402, 898, 914, 394, 410, 906, 922,
 134, 150, 646, 662, 142, 158, 654, 670, 390, 406, 902, 918, 398, 414, 910, 926,
  64,  80, 576, 592,  72,  88, 584, 600, 320, 336, 832, 848, 328, 344, 840, 856,
  68,  84, 580, 596,  76,  92, 588, 604, 324, 340, 836, 852, 332, 348, 844, 860,
 192, 208, 704, 720, 200, 216, 712, 728, 448, 464, 960, 976, 456, 472, 968, 984,
 196, 212, 708, 724, 204, 220, 716, 732, 452, 468, 964, 980, 460, 476, 972, 988,
  66,  82, 578, 594,  74,  90, 586, 602, 322, 338, 834, 850, 330, 346, 842, 858,
  70,  86, 582, 598,  78,  94, 590, 606, 326, 342, 838, 854, 334, 350, 846, 862,
 194, 210, 706, 722, 202, 218, 714, 730, 450, 466, 962, 978, 458, 474, 970, 986,
 198, 214, 710, 726, 206, 222, 718, 734, 454, 470, 966, 982, 462, 478, 974, 990, // 255
   1,  17, 513, 529,   9,  25, 521, 537, 257, 273, 769, 785, 265, 281, 777, 793,
   5,  21, 517, 533,  13,  29, 525, 541, 261, 277, 773, 789, 269, 285, 781, 797,
 129, 145, 641, 657, 137, 153, 649, 665, 385, 401, 897, 913, 393, 409, 905, 921,
 133, 149, 645, 661, 141, 157, 653, 669, 389, 405, 901, 917, 397, 413, 909, 925,
   3,  19, 515, 531,  11,  27, 523, 539, 259, 275, 771, 787, 267, 283, 779, 795,
   7,  23, 519, 535,  15,  31, 527, 543, 263, 279, 775, 791, 271, 287, 783, 799,
 131, 147, 643, 659, 139, 155, 651, 667, 387, 403, 899, 915, 395, 411, 907, 923,
 135, 151, 647, 663, 143, 159, 655, 671, 391, 407, 903, 919, 399, 415, 911, 927,
  65,  81, 577, 593,  73,  89, 585, 601, 321, 337, 833, 849, 329, 345, 841, 857,
  69,  85, 581, 597,  77,  93, 589, 605, 325, 341, 837, 853, 333, 349, 845, 861,
 193, 209, 705, 721, 201, 217, 713, 729, 449, 465, 961, 977, 457, 473, 969, 985,
 197, 213, 709, 725, 205, 221, 717, 733, 453, 469, 965, 981, 461, 477, 973, 989,
  67,  83, 579, 595,  75,  91, 587, 603, 323, 339, 835, 851, 331, 347, 843, 859,
  71,  87, 583, 599,  79,  95, 591, 607, 327, 343, 839, 855, 335, 351, 847, 863,
 195, 211, 707, 723, 203, 219, 715, 731, 451, 467, 963, 979, 459, 475, 971, 987,
 199, 215, 711, 727, 207, 223, 719, 735, 455, 471, 967, 983, 463, 479, 975, 991, // 511
  32,  48, 544, 560,  40,  56, 552, 568, 288, 304, 800, 816, 296, 312, 808, 824,
  36,  52, 548, 564,  44,  60, 556, 572, 292, 308, 804, 820, 300, 316, 812, 828,
 160, 176, 672, 688, 168, 184, 680, 696, 416, 432, 928, 944, 424, 440, 936, 952,
 164, 180, 676, 692, 172, 188, 684, 700, 420, 436, 932, 948, 428, 444, 940, 956,
  34,  50, 546, 562,  42,  58, 554, 570, 290, 306, 802, 818, 298, 314, 810, 826,
  38,  54, 550, 566,  46,  62, 558, 574, 294, 310, 806, 822, 302, 318, 814, 830,
 162, 178, 674, 690, 170, 186, 682, 698, 418, 434, 930, 946, 426, 442, 938, 954,
 166, 182, 678, 694, 174, 190, 686, 702, 422, 438, 934, 950, 430, 446, 942, 958,
  96, 112, 608, 624, 104, 120, 616, 632, 352, 368, 864, 880, 360, 376, 872, 888,
 100, 116, 612, 628, 108, 124, 620, 636, 356, 372, 868, 884, 364, 380, 876, 892,
 224, 240, 736, 752, 232, 248, 744, 760, 480, 496, 992,1008, 488, 504,1000,1016,
 228, 244, 740, 756, 236, 252, 748, 764, 484, 500, 996,1012, 492, 508,1004,1020,
  98, 114, 610, 626, 106, 122, 618, 634, 354, 370, 866, 882, 362, 378, 874, 890,
 102, 118, 614, 630, 110, 126, 622, 638, 358, 374, 870, 886, 366, 382, 878, 894,
 226, 242, 738, 754, 234, 250, 746, 762, 482, 498, 994,1010, 490, 506,1002,1018,
 230, 246, 742, 758, 238, 254, 750, 766, 486, 502, 998,1014, 494, 510,1006,1022, // 767
  33,  49, 545, 561,  41,  57, 553, 569, 289, 305, 801, 817, 297, 313, 809, 825,
  37,  53, 549, 565,  45,  61, 557, 573, 293, 309, 805, 821, 301, 317, 813, 829,
 161, 177, 673, 689, 169, 185, 681, 697, 417, 433, 929, 945, 425, 441, 937, 953,
 165, 181, 677, 693, 173, 189, 685, 701, 421, 437, 933, 949, 429, 445, 941, 957,
  35,  51, 547, 563,  43,  59, 555, 571, 291, 307, 803, 819, 299, 315, 811, 827,
  39,  55, 551, 567,  47,  63, 559, 575, 295, 311, 807, 823, 303, 319, 815, 831,
 163, 179, 675, 691, 171, 187, 683, 699, 419, 435, 931, 947, 427, 443, 939, 955,
 167, 183, 679, 695, 175, 191, 687, 703, 423, 439, 935, 951, 431, 447, 943, 959,
  97, 113, 609, 625, 105, 121, 617, 633, 353, 369, 865, 881, 361, 377, 873, 889,
 101, 117, 613, 629, 109, 125, 621, 637, 357, 373, 869, 885, 365, 381, 877, 893,
 225, 241, 737, 753, 233, 249, 745, 761, 481, 497, 993,1009, 489, 505,1001,1017,
 229, 245, 741, 757, 237, 253, 749, 765, 485, 501, 997,1013, 493, 509,1005,1021,
  99, 115, 611, 627, 107, 123, 619, 635, 355, 371, 867, 883, 363, 379, 875, 891,
 103, 119, 615, 631, 111, 127, 623, 639, 359, 375, 871, 887, 367, 383, 879, 895,
 227, 243, 739, 755, 235, 251, 747, 763, 483, 499, 995,1011, 491, 507,1003,1019,
 231, 247, 743, 759, 239, 255, 751, 767, 487, 503, 999,1015, 495, 511,1007,1023, // 1023
};

//---------------------------------------------------------------
// *** Class IW44Image::Alloc [declaration]

struct IW44Image::Alloc // DJVU_CLASS
{
  Alloc *next;
  short data[IWALLOCSIZE];
  Alloc(Alloc *n);
};

//---------------------------------------------------------------
// *** Class IW44Image::Block [implementation]


IW44Image::Block::Block(void)
{
  pdata[0] = pdata[1] = pdata[2] = pdata[3] = 0;
}

void 
IW44Image::Block::zero(int n)
{
  if (pdata[n>>4])
    pdata[n>>4][n&15] = 0;
}

void  
IW44Image::Block::read_liftblock(const short *coeff, IW44Image::Map *map)
{
  int n=0;
  for (int n1=0; n1<64; n1++)
    {
      short *d = data(n1,map);
      for (int n2=0; n2<16; n2++,n++)
        d[n2] = coeff[zigzagloc[n]];
    }
}

void  
IW44Image::Block::write_liftblock(short *coeff, int bmin, int bmax) const
{
  int n = bmin<<4;
  memset(coeff, 0, 1024*sizeof(short));
  for (int n1=bmin; n1<bmax; n1++)
    {
      const short *d = data(n1);
      if (d == 0)
        n += 16;
      else
        for (int n2=0; n2<16; n2++,n++)
          coeff[zigzagloc[n]] = d[n2];
    }
}

//---------------------------------------------------------------
// *** Class IW44Image::Map [implementation]


IW44Image::Map::Map(int w, int h)
  :  blocks(0), iw(w), ih(h), chain(0)
{
  bw = (w+0x20-1) & ~0x1f;
  bh = (h+0x20-1) & ~0x1f;
  nb = (unsigned int)(bw*bh) / (32 * 32);
  blocks = new IW44Image::Block[nb];
  top = IWALLOCSIZE;
}

IW44Image::Map::~Map()
{
  while (chain)
    {
      IW44Image::Alloc *next = chain->next;
      delete chain;
      chain = next;
    }
  delete [] blocks;
}


IW44Image::Alloc::Alloc(Alloc *n)
  : next(n) 
{ 
  // see note in IW44Image::Map::alloc
  memset(data, 0, sizeof(data)); 
}

short *
IW44Image::Map::alloc(int n)
{
  if (top+n > IWALLOCSIZE)
    {
      // note: everything is cleared long before we use it
      // in order to avoid the need for a memory fence.
      chain = new IW44Image::Alloc(chain);
      top = 0;
    }
  short *ans = chain->data + top;
  top += n;
  return ans;
}

short **
IW44Image::Map::allocp(int n)
{
  // Allocate enough room for pointers plus alignment
  short *p = alloc( (n+1) * sizeof(short*) / sizeof(short) );
  // Align on pointer size
  while ( ((size_t)p) & (sizeof(short*)-1) )
    p += 1;
  // Cast and return
  return (short**)p;
}

int 
IW44Image::Map::get_bucket_count(void) const
{
  int buckets = 0;
  for (int blockno=0; blockno<nb; blockno++)
    for (int buckno=0; buckno<64; buckno++)
      if (blocks[blockno].data(buckno))
        buckets += 1;
  return buckets;
}

unsigned int 
IW44Image::Map::get_memory_usage(void) const
{
  unsigned int usage = sizeof(Map);
  usage += sizeof(IW44Image::Block) * nb;
  for (IW44Image::Alloc *n = chain; n; n=n->next)
    usage += sizeof(IW44Image::Alloc);
  return usage;
}




void 
IW44Image::Map::image(signed char *img8, int rowsize, int pixsep, int fast)
{
  // Allocate reconstruction buffer
  short *data16;
  size_t sz = bw * bh;
  if (sz / (size_t)bw != (size_t)bh) // multiplication overflow
    G_THROW("IW44Image: image size exceeds maximum (corrupted file?)");
  GPBuffer<short> gdata16(data16,sz);
  // Copy coefficients
  int i;
  short *p = data16;
  const IW44Image::Block *block = blocks;
  for (i=0; i<bh; i+=32)
    {
      for (int j=0; j<bw; j+=32)
        {
          short liftblock[1024];
          // transfer into IW44Image::Block (apply zigzag and scaling)
          block->write_liftblock(liftblock);
          block++;
          // transfer into coefficient matrix at (p+j)
          short *pp = p + j;
          short *pl = liftblock;
          for (int ii=0; ii<32; ii++, pp+=bw,pl+=32)
            memcpy((void*)pp, (void*)pl, 32*sizeof(short));
        }
      // next row of blocks
      p += 32*bw;
    }
  // Reconstruction
  if (fast)
    {
      IW44Image::Transform::Decode::backward(data16, iw, ih, bw, 32, 2);  
      p = data16;
      for (i=0; i<bh; i+=2,p+=bw)
        for (int jj=0; jj<bw; jj+=2,p+=2)
          p[bw] = p[bw+1] = p[1] = p[0];
    }
  else
    {
      IW44Image::Transform::Decode::backward(data16, iw, ih, bw, 32, 1);  
    }
  // Copy result into image
  p = data16;
  signed char *row = img8;  
  for (i=0; i<ih; i++)
    {
      signed char *pix = row;
      for (int j=0; j<iw; j+=1,pix+=pixsep)
        {
          int x = (p[j] + iw_round) >> iw_shift;
          if (x < -128)
            x = -128;
          else if (x > 127)
            x = 127;
          *pix = x;
        }
      row += rowsize;
      p += bw;
    }
}

void 
IW44Image::Map::image(int subsample, const GRect &rect, 
              signed char *img8, int rowsize, int pixsep, int fast)
{
  int i;
  // Compute number of decomposition levels
  int nlevel = 0;
  while (nlevel<5 && (32>>nlevel)>subsample)
    nlevel += 1;
  int boxsize = 1<<nlevel;
  // Parameter check
  if (subsample!=(32>>nlevel))
    G_THROW( ERR_MSG("IW44Image.sample_factor") );
  if (rect.isempty())
    G_THROW( ERR_MSG("IW44Image.empty_rect") );    
  GRect irect(0,0,(iw+subsample-1)/subsample,(ih+subsample-1)/subsample);
  if (rect.xmin<0 || rect.ymin<0 || rect.xmax>irect.xmax || rect.ymax>irect.ymax)
    G_THROW( ERR_MSG("IW44Image.bad_rect") );
  // Multiresolution rectangles 
  // -- needed[i] tells which coeffs are required for the next step
  // -- recomp[i] tells which coeffs need to be computed at this level
  GRect needed[8];
  GRect recomp[8];
  int r = 1;
  needed[nlevel] = rect;
  recomp[nlevel] = rect;
  for (i=nlevel-1; i>=0; i--)
    {
      needed[i] = recomp[i+1];
      needed[i].inflate(iw_border*r, iw_border*r);
      needed[i].intersect(needed[i], irect);
      r += r;
      recomp[i].xmin = (needed[i].xmin + r-1) & ~(r-1);
      recomp[i].xmax = (needed[i].xmax) & ~(r-1);
      recomp[i].ymin = (needed[i].ymin + r-1) & ~(r-1);
      recomp[i].ymax = (needed[i].ymax) & ~(r-1);
    }
  // Working rectangle
  // -- a rectangle large enough to hold all the data
  GRect work;
  work.xmin = (needed[0].xmin) & ~(boxsize-1);
  work.ymin = (needed[0].ymin) & ~(boxsize-1);
  work.xmax = ((needed[0].xmax-1) & ~(boxsize-1) ) + boxsize;
  work.ymax = ((needed[0].ymax-1) & ~(boxsize-1) ) + boxsize;
  // -- allocate work buffer
  int dataw = work.xmax - work.xmin;     // Note: cannot use inline width() or height()
  int datah = work.ymax - work.ymin;     // because Intel C++ compiler optimizes it wrong !
  short *data;
  GPBuffer<short> gdata(data,dataw*datah);
  // Fill working rectangle
  // -- loop over liftblocks rows
  short *ldata = data;
  int blkw = (bw>>5);
  const IW44Image::Block *lblock = blocks + (work.ymin>>nlevel)*blkw + (work.xmin>>nlevel);
  for (int by=work.ymin; by<work.ymax; by+=boxsize)
    {
      // -- loop over liftblocks in row
      const IW44Image::Block *block = lblock;
      short *rdata = ldata;
      for (int bx=work.xmin; bx<work.xmax; bx+=boxsize)        
        {
          // -- decide how much to load
          int mlevel = nlevel;
          if (nlevel>2)
            if (bx+31<needed[2].xmin || bx>needed[2].xmax ||
                by+31<needed[2].ymin || by>needed[2].ymax )
              mlevel = 2;
          int bmax   = ((1<<(mlevel+mlevel))+15)>>4;
          int ppinc  = (1<<(nlevel-mlevel));
          int ppmod1 = (dataw<<(nlevel-mlevel));
          int ttmod0 = (32 >> mlevel);
          int ttmod1 = (ttmod0 << 5);
          // -- get current block
          short liftblock[1024];
          block->write_liftblock(liftblock, 0, bmax );
          // -- copy liftblock into image
          short *tt = liftblock;
          short *pp = rdata;
          for (int ii=0; ii<boxsize; ii+=ppinc,pp+=ppmod1,tt+=ttmod1-32)
            for (int jj=0; jj<boxsize; jj+=ppinc,tt+=ttmod0)
              pp[jj] = *tt;
          // -- next block in row
          rdata += boxsize;
          block += 1;
        }
      // -- next row of blocks
      ldata += dataw << nlevel;
      lblock += blkw;
    }
  // Perform reconstruction
  r = boxsize;
  for (i=0; i<nlevel; i++)
    {
      GRect comp = needed[i];
      comp.xmin = comp.xmin & ~(r-1);
      comp.ymin = comp.ymin & ~(r-1);
      comp.translate(-work.xmin, -work.ymin);
      // Fast mode shortcuts finer resolution
      if (fast && i>=4) 
        {
          short *pp = data + comp.ymin*dataw;
          for (int ii=comp.ymin; ii<comp.ymax; ii+=2, pp+=dataw+dataw)
            for (int jj=comp.xmin; jj<comp.xmax; jj+=2)
              pp[jj+dataw] = pp[jj+dataw+1] = pp[jj+1] = pp[jj];
          break;
        }
      else
        {
          short *pp = data + comp.ymin*dataw + comp.xmin;
          IW44Image::Transform::Decode::backward(pp, comp.width(), comp.height(), dataw, r, r>>1);
        }
      r = r>>1;
    }
  // Copy result into image
  GRect nrect = rect;
  nrect.translate(-work.xmin, -work.ymin);
  short *p = data + nrect.ymin*dataw;
  signed char *row = img8;  
  for (i=nrect.ymin; i<nrect.ymax; i++)
    {
      int j;
      signed char *pix = row;
      for (j=nrect.xmin; j<nrect.xmax; j+=1,pix+=pixsep)
        {
          int x = (p[j] + iw_round) >> iw_shift;
          if (x < -128)
            x = -128;
          else if (x > 127)
            x = 127;
          *pix = x;
        }
      row += rowsize;
      p += dataw;
    }
}




//////////////////////////////////////////////////////
// ENCODING/DECODING WAVELET COEFFICIENTS 
//    USING HIERARCHICAL SET DIFFERENCE
//////////////////////////////////////////////////////


//-----------------------------------------------
// Class IW44Image::Codec [implementation]
// Maintains information shared while encoding or decoding


// Constant

static const struct { int start; int size; }  
bandbuckets[] = 
{
  // Code first bucket and number of buckets in each band
  { 0, 1 }, // -- band zero contains all lores info
  { 1, 1 }, { 2, 1 }, { 3, 1 }, 
  { 4, 4 }, { 8, 4 }, { 12,4 }, 
  { 16,16 }, { 32,16 }, { 48,16 }, 
};


// IW44Image::Codec constructor

IW44Image::Codec::Codec(IW44Image::Map &xmap)
  : map(xmap), 
    curband(0),
    curbit(1)
{
  // Initialize quantification
  int  j;
  int  i = 0;
  const int *q = iw_quant;
  // -- lo coefficients
  for (j=0; i<4; j++)
    quant_lo[i++] = *q++;
  for (j=0; j<4; j++)
    quant_lo[i++] = *q;
  q += 1;
  for (j=0; j<4; j++)
    quant_lo[i++] = *q;
  q += 1;
  for (j=0; j<4; j++)
    quant_lo[i++] = *q;
  q += 1;
  // -- hi coefficients
  quant_hi[0] = 0;
  for (j=1; j<10; j++)
    quant_hi[j] = *q++;
  // Initialize coding contexts
  memset((void*)ctxStart, 0, sizeof(ctxStart));
  memset((void*)ctxBucket, 0, sizeof(ctxBucket));
  ctxMant = 0;
  ctxRoot = 0;
}


// IW44Image::Codec destructor

IW44Image::Codec::~Codec() {}

// is_null_slice
// -- check if data can be produced for this band/mask
// -- also fills the sure_zero array

int 
IW44Image::Codec::is_null_slice(int bit, int band)
{
  if (band == 0)
    {
      int is_null = 1;
      for (int i=0; i<16; i++) 
        {
          int threshold = quant_lo[i];
          coeffstate[i] = ZERO;
          if (threshold>0 && threshold<0x8000)
            {
              coeffstate[i] = UNK;
              is_null = 0;
            }
        }
      return is_null;
    }
  else
    {
      int threshold = quant_hi[band];
      return (! (threshold>0 && threshold<0x8000));
    }
}


// code_slice
// -- read/write a slice of datafile

int
IW44Image::Codec::Decode::code_slice(ZPCodec &zp)
{
  // Check that code_slice can still run
  if (curbit < 0)
    return 0;
  // Perform coding
  if (! is_null_slice(curbit, curband))
    {
      for (int blockno=0; blockno<map.nb; blockno++)
        {
          int fbucket = bandbuckets[curband].start;
          int nbucket = bandbuckets[curband].size;
          decode_buckets(zp, curbit, curband, 
                           map.blocks[blockno], 
                           fbucket, nbucket);
        }
    }
  return finish_code_slice(zp);
}

// code_slice
// -- read/write a slice of datafile

int
IW44Image::Codec::finish_code_slice(ZPCodec &zp)
{
  // Reduce quantization threshold
  quant_hi[curband] = quant_hi[curband] >> 1;
  if (curband == 0)
    for (int i=0; i<16; i++) 
      quant_lo[i] = quant_lo[i] >> 1;
  // Proceed to the next slice
  if (++curband >= (int)(sizeof(bandbuckets)/sizeof(bandbuckets[0])))
    {
      curband = 0;
      curbit += 1;
      if (quant_hi[(sizeof(bandbuckets)/sizeof(bandbuckets[0]))-1] == 0)
        {
          // All quantization thresholds are null
          curbit = -1;
          return 0;
        }
    }
  return 1;
}

// decode_prepare
// -- prepare the states before decoding buckets

int
IW44Image::Codec::decode_prepare(int fbucket, int nbucket, IW44Image::Block &blk)
{  
  int bbstate = 0;
  char *cstate = coeffstate;
  if (fbucket)
    {
      // Band other than zero
      for (int buckno=0; buckno<nbucket; buckno++, cstate+=16)
        {
          int bstatetmp = 0;
          const short *pcoeff = blk.data(fbucket+buckno);
          if (! pcoeff)
            {
              // cstate[0..15] will be filled later
              bstatetmp = UNK;
            }
          else
            {
              for (int i=0; i<16; i++)
                {
                  int cstatetmp = UNK;
                  if (pcoeff[i])
                    cstatetmp = ACTIVE;
                  cstate[i] = cstatetmp;
                  bstatetmp |= cstatetmp;
                }
            }
          bucketstate[buckno] = bstatetmp;
          bbstate |= bstatetmp;
        }
    }
  else
    {
      // Band zero ( fbucket==0 implies band==zero and nbucket==1 )
      const short *pcoeff = blk.data(0);
      if (! pcoeff)
        {
          // cstate[0..15] will be filled later
          bbstate = UNK;      
        }
      else
        {
          for (int i=0; i<16; i++)
            {
              int cstatetmp = cstate[i];
              if (cstatetmp != ZERO)
                {
                  cstatetmp = UNK;
                  if (pcoeff[i])
                    cstatetmp = ACTIVE;
                }
              cstate[i] = cstatetmp;
              bbstate |= cstatetmp;
            }
        }
      bucketstate[0] = bbstate;
    }
  return bbstate;
}


// decode_buckets
// -- code a sequence of buckets in a given block

void
IW44Image::Codec::decode_buckets(ZPCodec &zp, int bit, int band, 
                         IW44Image::Block &blk,
                         int fbucket, int nbucket)
{
  // compute state of all coefficients in all buckets
  int bbstate = decode_prepare(fbucket, nbucket, blk);
  // code root bit
  if ((nbucket<16) || (bbstate&ACTIVE))
    {
      bbstate |= NEW;
    }
  else if (bbstate & UNK)
    {
      if (zp.decoder(ctxRoot))
        bbstate |= NEW;
#ifdef TRACE
      DjVuPrintMessage("bbstate[bit=%d,band=%d] = %d\n", bit, band, bbstate);
#endif
    }
  
  // code bucket bits
  if (bbstate & NEW)
    for (int buckno=0; buckno<nbucket; buckno++)
      {
        // Code bucket bit
        if (bucketstate[buckno] & UNK)
          {
            // Context
            int ctx = 0;
#ifndef NOCTX_BUCKET_UPPER
            if (band>0)
              {
                int k = (fbucket+buckno)<<2;
                const short *b = blk.data(k>>4);
                if (b)
                  {
                    k = k & 0xf;
                    if (b[k])
                      ctx += 1;
                    if (b[k+1])
                      ctx += 1;
                    if (b[k+2])
                      ctx += 1;
                    if (ctx<3 && b[k+3])
                      ctx += 1;
                  }
              }
#endif // NOCTX_BUCKET_UPPER
#ifndef NOCTX_BUCKET_ACTIVE
            if (bbstate & ACTIVE)
              ctx |= 4; 
#endif
            // Code
            if (zp.decoder( ctxBucket[band][ctx] ))
              bucketstate[buckno] |= NEW;
#ifdef TRACE
            DjVuPrintMessage("  bucketstate[bit=%d,band=%d,buck=%d] = %d\n", 
                   bit, band, buckno, bucketstate[buckno]);
#endif
          }
      }

  // code new active coefficient (with their sign)
  if (bbstate & NEW)
    {
      int thres = quant_hi[band];
      char *cstate = coeffstate;
      for (int buckno=0; buckno<nbucket; buckno++, cstate+=16)
        if (bucketstate[buckno] & NEW)
          {
            int i;
            short *pcoeff = (short*)blk.data(fbucket+buckno);
            if (!pcoeff)
              {
                pcoeff = blk.data(fbucket+buckno, &map);
                // time to fill cstate[0..15]
                if (fbucket == 0) // band zero
                  {
                    for (i=0; i<16; i++)
                      if (cstate[i] != ZERO)
                        cstate[i] = UNK;
                  }
                else
                  {
                    for (i=0; i<16; i++)
                      cstate[i] = UNK;
                  }
              }
#ifndef NOCTX_EXPECT
            int gotcha = 0;
            const int maxgotcha = 7;
            for (i=0; i<16; i++)
              if (cstate[i] & UNK)
                gotcha += 1;
#endif
            for (i=0; i<16; i++)
              {
                if (cstate[i] & UNK)
                  {
                    // find lores threshold
                    if (band == 0)
                      thres = quant_lo[i];
                    // prepare context
                    int ctx = 0;
#ifndef NOCTX_EXPECT
                    if (gotcha>=maxgotcha)
                      ctx = maxgotcha;
                    else
                      ctx = gotcha;
#endif
#ifndef NOCTX_ACTIVE
                    if (bucketstate[buckno] & ACTIVE)
                      ctx |= 8;
#endif
                    // code difference bit
                    if (zp.decoder( ctxStart[ctx] ))
                      {
                        cstate[i] |= NEW;
                        int halfthres = thres>>1;
                        int coeff = thres+halfthres-(halfthres>>2);
                        if (zp.IWdecoder())
                          pcoeff[i] = -coeff;
                        else
                          pcoeff[i] = coeff;
                      }
#ifndef NOCTX_EXPECT
                    if (cstate[i] & NEW)
                      gotcha = 0;
                    else if (gotcha > 0)
                      gotcha -= 1;
#endif
#ifdef TRACE
                    DjVuPrintMessage("    coeffstate[bit=%d,band=%d,buck=%d,c=%d] = %d\n", 
                           bit, band, buckno, i, cstate[i]);
#endif
                  }
              }
          }
    }
  
  // code mantissa bits
  if (bbstate & ACTIVE)
    {
      int thres = quant_hi[band];
      char *cstate = coeffstate;
      for (int buckno=0; buckno<nbucket; buckno++, cstate+=16)
        if (bucketstate[buckno] & ACTIVE)
          {
            short *pcoeff = (short*)blk.data(fbucket+buckno);
            for (int i=0; i<16; i++)
              if (cstate[i] & ACTIVE)
                {
                  int coeff = pcoeff[i];
                  if (coeff < 0)
                    coeff = -coeff;
                  // find lores threshold
                  if (band == 0)
                    thres = quant_lo[i];
                  // adjust coefficient
                  if (coeff <= 3*thres)
                    {
                      // second mantissa bit
                      coeff = coeff + (thres>>2);
                      if (zp.decoder(ctxMant))
                        coeff = coeff + (thres>>1);
                      else
                        coeff = coeff - thres + (thres>>1);
                    }
                  else
                    {
                      if (zp.IWdecoder())
                        coeff = coeff + (thres>>1);
                      else
                        coeff = coeff - thres + (thres>>1);
                    }
                  // store coefficient
                  if (pcoeff[i] > 0)
                    pcoeff[i] = coeff;
                  else
                    pcoeff[i] = -coeff;
                }
          }
    }
}


//////////////////////////////////////////////////////
// UTILITIES
//////////////////////////////////////////////////////


#ifdef min
#undef min
#endif
static inline int
min(const int x, const int y)
{
  return (x <= y) ? x : y;
}

#ifdef max
#undef max
#endif
static inline int
max(const int x, const int y)
{
  return (y <= x) ? x : y;
}


void 
IW44Image::PrimaryHeader::decode(GP<ByteStream> gbs)
{
  serial = gbs->read8();
  slices = gbs->read8();
}

void 
IW44Image::SecondaryHeader::decode(GP<ByteStream> gbs)
{
  major = gbs->read8();
  minor = gbs->read8();
}

void 
IW44Image::TertiaryHeader::decode(GP<ByteStream> gbs, int major, int minor)
{
  xhi = gbs->read8();
  xlo = gbs->read8();
  yhi = gbs->read8();
  ylo = gbs->read8();
  crcbdelay = 0;
  if (major== 1 && minor>=2)
    crcbdelay = gbs->read8();
}



//////////////////////////////////////////////////////
// CLASS IW44Image
//////////////////////////////////////////////////////

IW44Image::IW44Image(void)
  : db_frac(1.0),
    ymap(0), cbmap(0), crmap(0),
    cslice(0), cserial(0), cbytes(0)
{}

IW44Image::~IW44Image()
{
  delete ymap;
  delete cbmap;
  delete crmap;
}

GP<IW44Image>
IW44Image::create_decode(const ImageType itype)
{
  switch(itype)
  {
  case COLOR:
    return new IWPixmap();
  case GRAY:
    return new IWBitmap();
  default:
    return 0;
  }
}

int
IW44Image::encode_chunk(GP<ByteStream>, const IWEncoderParms &)
{
  G_THROW( ERR_MSG("IW44Image.codec_open2") );
  return 0;
}

void 
IW44Image::encode_iff(IFFByteStream &, int nchunks, const IWEncoderParms *)
{
  G_THROW( ERR_MSG("IW44Image.codec_open2") );
}

  
void 
IWBitmap::close_codec(void)
{
  delete ycodec;
  ycodec = 0;
  cslice = cbytes = cserial = 0;
}

void 
IWPixmap::close_codec(void)
{
  delete ycodec;
  delete cbcodec;
  delete crcodec;
  ycodec = crcodec = cbcodec = 0;
  cslice = cbytes = cserial = 0;
}

int 
IW44Image::get_width(void) const
{
  return (ymap)?(ymap->iw):0;
}

int 
IW44Image::get_height(void) const
{
  return (ymap)?(ymap->ih):0;
}


//////////////////////////////////////////////////////
// CLASS IWBITMAP
//////////////////////////////////////////////////////

IWBitmap::IWBitmap(void )
: IW44Image(), ycodec(0)
{}

IWBitmap::~IWBitmap()
{
  close_codec();
}

int
IWBitmap::get_percent_memory(void) const
{
  int buckets = 0;
  int maximum = 0;
  if (ymap) 
    {
      buckets += ymap->get_bucket_count();
      maximum += 64 * ymap->nb;
    }
  return 100*buckets/ (maximum ? maximum : 1);
}

unsigned int
IWBitmap::get_memory_usage(void) const
{
  unsigned int usage = sizeof(GBitmap);
  if (ymap)
    usage += ymap->get_memory_usage();
  return usage;
}


GP<GBitmap> 
IWBitmap::get_bitmap(void)
{
  // Check presence of data
  if (ymap == 0)
    return 0;
  // Perform wavelet reconstruction
  int w = ymap->iw;
  int h = ymap->ih;
  GP<GBitmap> pbm = GBitmap::create(h, w);
  ymap->image((signed char*)(*pbm)[0],pbm->rowsize());
  // Shift image data
  for (int i=0; i<h; i++)
    {
      unsigned char *urow = (*pbm)[i];
      signed char *srow = (signed char*)urow;
      for (int j=0; j<w; j++)
        urow[j] = (int)(srow[j]) + 128;
    }
  pbm->set_grays(256);
  return pbm;
}


GP<GBitmap>
IWBitmap::get_bitmap(int subsample, const GRect &rect)
{
  if (ymap == 0)
    return 0;
  // Allocate bitmap
  int w = rect.width();
  int h = rect.height();
  GP<GBitmap> pbm = GBitmap::create(h,w);
  ymap->image(subsample, rect, (signed char*)(*pbm)[0],pbm->rowsize());
  // Shift image data
  for (int i=0; i<h; i++)
    {
      unsigned char *urow = (*pbm)[i];
      signed char *srow = (signed char*)urow;
      for (int j=0; j<w; j++)
        urow[j] = (int)(srow[j]) + 128;
    }
  pbm->set_grays(256);
  return pbm;
}


int
IWBitmap::decode_chunk(GP<ByteStream> gbs)
{
  // Open
  if (! ycodec)
  {
    cslice = cserial = 0;
    delete ymap;
    ymap = 0;
  }
  // Read primary header
  struct IW44Image::PrimaryHeader primary;
  primary.decode(gbs);
  if (primary.serial != cserial)
    G_THROW( ERR_MSG("IW44Image.wrong_serial") );
  int nslices = cslice + primary.slices;
  // Read auxilliary headers
  if (cserial == 0)
    {
      struct IW44Image::SecondaryHeader secondary;
      secondary.decode(gbs);
      if ((secondary.major & 0x7f) != IWCODEC_MAJOR)
        G_THROW( ERR_MSG("IW44Image.incompat_codec") );
      if (secondary.minor > IWCODEC_MINOR)
        G_THROW( ERR_MSG("IW44Image.recent_codec") );
      // Read tertiary header
      struct IW44Image::TertiaryHeader tertiary;
      tertiary.decode(gbs, secondary.major & 0x7f, secondary.minor);
      if (! (secondary.major & 0x80))
        G_THROW( ERR_MSG("IW44Image.has_color") );
      // Create ymap and ycodec
      int w = (tertiary.xhi << 8) | tertiary.xlo;
      int h = (tertiary.yhi << 8) | tertiary.ylo;
      assert(! ymap);
      ymap = new Map(w, h);
      assert(! ycodec);
      ycodec = new Codec::Decode(*ymap);
    }
  // Read data
  assert(ymap);
  assert(ycodec);
  GP<ZPCodec> gzp=ZPCodec::create(gbs, false, true);
  ZPCodec &zp=*gzp;
  int flag = 1;
  while (flag && cslice<nslices)
    {
      flag = ycodec->code_slice(zp);
      cslice++;
    }
  // Return
  cserial += 1;
  return nslices;
}

void 
IWBitmap::parm_dbfrac(float frac)
{
  if (frac>0 && frac<=1)
    db_frac = frac;
  else
    G_THROW( ERR_MSG("IW44Image.param_range") );
}


int 
IWBitmap::get_serial(void)
{
  return cserial;
}

void 
IWBitmap::decode_iff(IFFByteStream &iff, int maxchunks)
{
  if (ycodec)
    G_THROW( ERR_MSG("IW44Image.left_open2") );
  GUTF8String chkid;
  iff.get_chunk(chkid);
  if (chkid != "FORM:BM44")
    G_THROW( ERR_MSG("IW44Image.corrupt_BM44") );
  while (--maxchunks>=0 && iff.get_chunk(chkid))
    {
      if (chkid == "BM44")
        decode_chunk(iff.get_bytestream());
      iff.close_chunk();
    }
  iff.close_chunk();
  close_codec();
}




//////////////////////////////////////////////////////
// CLASS IWENCODERPARMS
//////////////////////////////////////////////////////


IWEncoderParms::IWEncoderParms(void)
{
  // Zero represent default values
  memset((void*)this, 0, sizeof(IWEncoderParms));
}





//////////////////////////////////////////////////////
// CLASS IWPIXMAP
//////////////////////////////////////////////////////


IWPixmap::IWPixmap(void)
: IW44Image(), crcb_delay(10), crcb_half(0), ycodec(0), cbcodec(0), crcodec(0)
{}

IWPixmap::~IWPixmap()
{
  close_codec();
}

int
IWPixmap::get_percent_memory(void) const
{
  int buckets = 0;
  int maximum = 0;
  if (ymap)
    {
      buckets += ymap->get_bucket_count();
      maximum += 64*ymap->nb;
    }
  if (cbmap)
    {
      buckets += cbmap->get_bucket_count();
      maximum += 64*cbmap->nb;
    }
  if (crmap)
    {
      buckets += crmap->get_bucket_count();
      maximum += 64*crmap->nb;
    }
  return 100*buckets/ (maximum ? maximum : 1);
}

unsigned int
IWPixmap::get_memory_usage(void) const
{
  unsigned int usage = sizeof(GPixmap);
  if (ymap)
    usage += ymap->get_memory_usage();
  if (cbmap)
    usage += cbmap->get_memory_usage();
  if (crmap)
    usage += crmap->get_memory_usage();
  return usage;
}


GP<GPixmap> 
IWPixmap::get_pixmap(void)
{
  // Check presence of data
  if (ymap == 0)
    return 0;
  // Allocate pixmap
  int w = ymap->iw;
  int h = ymap->ih;
  GP<GPixmap> ppm = GPixmap::create(h, w);
  // Perform wavelet reconstruction
  signed char *ptr = (signed char*) (*ppm)[0];
  int rowsep = ppm->rowsize() * sizeof(GPixel);
  int pixsep = sizeof(GPixel);
  ymap->image(ptr, rowsep, pixsep);
  if (crmap && cbmap && crcb_delay >= 0)
  {
    cbmap->image(ptr+1, rowsep, pixsep, crcb_half);
    crmap->image(ptr+2, rowsep, pixsep, crcb_half);
  }
  // Convert image data to RGB
  if (crmap && cbmap && crcb_delay >= 0)
    {
      Transform::Decode::YCbCr_to_RGB((*ppm)[0], w, h, ppm->rowsize());
    }
  else
    {
      for (int i=0; i<h; i++)
        {
          GPixel *pixrow = (*ppm)[i];
          for (int j=0; j<w; j++, pixrow++)
            pixrow->b = pixrow->g = pixrow->r 
              = 127 - (int)(((signed char*)pixrow)[0]);
        }
    }
  // Return
  return ppm;
}



GP<GPixmap>
IWPixmap::get_pixmap(int subsample, const GRect &rect)
{
  if (ymap == 0)
    return 0;
  // Allocate
  int w = rect.width();
  int h = rect.height();
  GP<GPixmap> ppm = GPixmap::create(h,w);
  // Perform wavelet reconstruction
  signed char *ptr = (signed char*) (*ppm)[0];
  int rowsep = ppm->rowsize() * sizeof(GPixel);
  int pixsep = sizeof(GPixel);
  ymap->image(subsample, rect, ptr, rowsep, pixsep);
  if (crmap && cbmap && crcb_delay >= 0)
  {
    cbmap->image(subsample, rect, ptr+1, rowsep, pixsep, crcb_half);
    crmap->image(subsample, rect, ptr+2, rowsep, pixsep, crcb_half);
  }
  // Convert image data to RGB
  if (crmap && cbmap && crcb_delay >= 0)
    {
      Transform::Decode::YCbCr_to_RGB((*ppm)[0], w, h, ppm->rowsize());
    }
  else
    {
      for (int i=0; i<h; i++)
        {
          GPixel *pixrow = (*ppm)[i];
          for (int j=0; j<w; j++, pixrow++)
            pixrow->b = pixrow->g = pixrow->r 
              = 127 - (int)(((signed char*)pixrow)[0]);
        }
    }
  // Return
  return ppm;
}


int
IWPixmap::decode_chunk(GP<ByteStream> gbs)
{
  // Open
  if (! ycodec)
  {
      cslice = cserial = 0;
      delete ymap;
      ymap = 0;
  }

  // Read primary header
  struct IW44Image::PrimaryHeader primary;
  primary.decode(gbs);
  if (primary.serial != cserial)
    G_THROW( ERR_MSG("IW44Image.wrong_serial2") );
  int nslices = cslice + primary.slices;
  // Read secondary header
  if (cserial == 0)
    {
      struct IW44Image::SecondaryHeader secondary;
      secondary.decode(gbs);
      if ((secondary.major & 0x7f) != IWCODEC_MAJOR)
        G_THROW( ERR_MSG("IW44Image.incompat_codec2") );
      if (secondary.minor > IWCODEC_MINOR)
        G_THROW( ERR_MSG("IW44Image.recent_codec2") );
      // Read tertiary header
      struct IW44Image::TertiaryHeader tertiary;
      tertiary.decode(gbs, secondary.major & 0x7f, secondary.minor);
      // Handle header information
      int w = (tertiary.xhi << 8) | tertiary.xlo;
      int h = (tertiary.yhi << 8) | tertiary.ylo;
      crcb_delay = 0;
      crcb_half = 0;
      if (secondary.minor>=2)
        crcb_delay = tertiary.crcbdelay & 0x7f;
      if (secondary.minor>=2)
        crcb_half = (tertiary.crcbdelay & 0x80 ? 0 : 1);
      if (secondary.major & 0x80)
        crcb_delay = -1;
      // Create ymap and ycodec    
      assert(! ymap);
      assert(! ycodec);
      ymap = new Map(w, h);
      ycodec = new Codec::Decode(*ymap);
      if (crcb_delay >= 0)
        {
          cbmap = new Map(w, h);
          crmap = new Map(w, h);
          cbcodec = new Codec::Decode(*cbmap);
          crcodec = new Codec::Decode(*crmap);
        }
    }
  // Read data
  assert(ymap);
  assert(ycodec);
  GP<ZPCodec> gzp=ZPCodec::create(gbs, false, true);
  ZPCodec &zp=*gzp;
  int flag = 1;
  while (flag && cslice<nslices)
    {
      flag = ycodec->code_slice(zp);
      if (crcodec && cbcodec && crcb_delay<=cslice)
        {
          flag |= cbcodec->code_slice(zp);
          flag |= crcodec->code_slice(zp);
        }
      cslice++;
    }
  // Return
  cserial += 1;
  return nslices;
}


int 
IWPixmap::parm_crcbdelay(const int parm)
{
  if (parm >= 0)
    crcb_delay = parm;
  return crcb_delay;
}

void 
IWPixmap::parm_dbfrac(float frac)
{
  if (frac>0 && frac<=1)
    db_frac = frac;
  else
    G_THROW( ERR_MSG("IW44Image.param_range2") );
}

int 
IWPixmap::get_serial(void)
{
  return cserial;
}


void 
IWPixmap::decode_iff(IFFByteStream &iff, int maxchunks)
{
  if (ycodec)
    G_THROW( ERR_MSG("IW44Image.left_open4") );
  GUTF8String chkid;
  iff.get_chunk(chkid);
  if (chkid!="FORM:PM44" && chkid!="FORM:BM44")
    G_THROW( ERR_MSG("IW44Image.corrupt_BM44_2") );
  while (--maxchunks>=0 && iff.get_chunk(chkid))
    {
      if (chkid=="PM44" || chkid=="BM44")
        decode_chunk(iff.get_bytestream());
      iff.close_chunk();
    }
  iff.close_chunk();
  close_codec();
}

//////////////////////////////////////////////////////
// NEW FILTERS
//////////////////////////////////////////////////////

void
IW44Image::Transform::filter_begin(int w, int h)
{
  if (MMXControl::mmxflag < 0)  
    MMXControl::enable_mmx();
}


void
IW44Image::Transform::filter_end(void)
{
#ifdef MMX
  if (MMXControl::mmxflag > 0)
    MMXemms;
#endif
}


//////////////////////////////////////////////////////
// WAVELET TRANSFORM 
//////////////////////////////////////////////////////


//----------------------------------------------------
// Function for applying bidimensional IW44 between 
// scale intervals begin(inclusive) and end(exclusive)

void
IW44Image::Transform::Decode::backward(short *p, int w, int h, int rowsize, int begin, int end)
{ 
  // PREPARATION
  filter_begin(w,h);
  // LOOP ON SCALES
  for (int scale=begin>>1; scale>=end; scale>>=1)
    {
#ifdef IWTRANSFORM_TIMER
      int tv,th;
      th = tv = GOS::ticks();
#endif
      filter_bv(p, w, h, rowsize, scale);
#ifdef IWTRANSFORM_TIMER
      th = GOS::ticks();
      tv = th - tv;
#endif
      filter_bh(p, w, h, rowsize, scale);
#ifdef IWTRANSFORM_TIMER
      th = GOS::ticks()-th;
      DjVuPrintErrorUTF8("back%d\tv=%dms h=%dms\n", scale,tv,th);
#endif
    }
  // TERMINATE
  filter_end();
}
  



//////////////////////////////////////////////////////
// COLOR TRANSFORM 
//////////////////////////////////////////////////////

/* Converts YCbCr to RGB. */
void 
IW44Image::Transform::Decode::YCbCr_to_RGB(GPixel *p, int w, int h, int rowsize)
{
  for (int i=0; i<h; i++,p+=rowsize)
    {
      GPixel *q = p;
      for (int j=0; j<w; j++,q++)
        {
          signed char y = ((signed char*)q)[0];
          signed char b = ((signed char*)q)[1];
          signed char r = ((signed char*)q)[2];
          // This is the Pigeon transform
          int t1 = b >> 2 ; 
          int t2 = r + (r >> 1);
          int t3 = y + 128 - t1;
          int tr = y + 128 + t2;
          int tg = t3 - (t2 >> 1);
          int tb = t3 + (b << 1);
          q->r = max(0,min(255,tr));
          q->g = max(0,min(255,tg));
          q->b = max(0,min(255,tb));
        }
    }
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
