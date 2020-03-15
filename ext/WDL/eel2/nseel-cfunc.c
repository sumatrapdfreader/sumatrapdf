/*
  Expression Evaluator Library (NS-EEL) v2
  Copyright (C) 2004-2013 Cockos Incorporated
  Copyright (C) 1999-2003 Nullsoft, Inc.
  
  nseel-cfunc.c: assembly/C implementation of operator/function templates
  This file should be ideally compiled with optimizations towards "minimize size"

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


#include "ns-eel-int.h"
#include <math.h>
#include <stdio.h>



// these are used by our assembly code


#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */

static unsigned int genrand_int32(void)
{

    unsigned int y;
    static unsigned int mag01[2]={0x0UL, MATRIX_A};
    /* mag01[x] = x * MATRIX_A  for x=0,1 */

    static unsigned int mt[N]; /* the array for the state vector  */
    static unsigned int __idx;

    unsigned int mti = __idx;

    if (!mti)
    { 
      unsigned int s=0x4141f00d;
      mt[0]= s & 0xffffffffUL;
      for (mti=1; mti<N; mti++) 
      {
          mt[mti] = 
	      (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti); 
          /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
          /* In the previous versions, MSBs of the seed affect   */
          /* only MSBs of the array mt[].                        */
          /* 2002/01/09 modified by Makoto Matsumoto             */
          mt[mti] &= 0xffffffffUL;
          /* for >32 bit machines */
      }
      __idx = N; // mti = N (from loop)
    }

    if (mti >= N) { /* generate N words at one time */
        int kk;
        __idx = 1;

        for (kk=0;kk<N-M;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        for (;kk<N-1;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
        mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];

        mti = 0;
    }
    else
      __idx++;
  
    y = mt[mti];

    /* Tempering */
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
}



//---------------------------------------------------------------------------------------------------------------
EEL_F NSEEL_CGEN_CALL nseel_int_rand(EEL_F f)
{
  EEL_F x=floor(f);
  if (x < 1.0) x=1.0;
 
#ifdef NSEEL_EEL1_COMPAT_MODE 
  return (EEL_F)(genrand_int32()%(int)x);
#else
  return (EEL_F) (genrand_int32()*(1.0/(double)0xFFFFFFFF)*x);
#endif
}

//---------------------------------------------------------------------------------------------------------------


#ifndef EEL_TARGET_PORTABLE

#ifdef __ppc__
#include "asm-nseel-ppc-gcc.c"
#elif defined(__aarch64__)
#include "asm-nseel-aarch64-gcc.c"
#elif defined(__arm__)
#include "asm-nseel-arm-gcc.c"
#elif defined (_M_ARM) && _M_ARM  == 7
  // vc on ARM, tbd
#else
  #ifdef _MSC_VER
    #ifdef _WIN64
      //nasm
    #else
      #include "asm-nseel-x86-msvc.c"

      void eel_setfp_round() 
      { 
        short oldsw;
        __asm
        {
          fnstcw [oldsw]
          mov ax, [oldsw]
          and ax, 0xF3FF // round to nearest
          mov [oldsw], ax
          fldcw [oldsw]
        }
      }
      void eel_setfp_trunc() 
      { 
        short oldsw;
        __asm
        {
          fnstcw [oldsw]
          mov ax, [oldsw]
          or ax, 0xC00 // truncate
          mov [oldsw], ax
          fldcw [oldsw]
        }
      }
    #endif
  #elif !defined(__LP64__)
    #define FUNCTION_MARKER "\n.byte 0x89,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90\n"
    #include "asm-nseel-x86-gcc.c"
    void eel_setfp_round()
    {
	__asm__(
		"subl $16, %esp\n"
		"fnstcw (%esp)\n"
		"mov (%esp), %ax\n"
		"and $0xF3FF, %ax\n" // set round to nearest
		"mov %ax, 4(%esp)\n"
		"fldcw 4(%esp)\n"
		"addl $16, %esp\n"
	);
    }
    void eel_setfp_trunc()
    {
	__asm__(
		"subl $16, %esp\n"
		"fnstcw (%esp)\n"
		"mov (%esp), %ax\n"
		"or $0xC00, %ax\n" // set to truncate
		"mov %ax, 4(%esp)\n"
		"fldcw 4(%esp)\n"
		"addl $16, %esp\n"
	);
   }
  #endif
#endif

#endif
