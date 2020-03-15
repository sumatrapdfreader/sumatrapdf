/*
  WDL - fft.h
  Copyright (C) 2006 and later Cockos Incorporated

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
  


  This file defines the interface to the WDL FFT library. These routines are based on the 
  DJBFFT library, which are   Copyright 1999 D. J. Bernstein, djb@pobox.com

  The DJB FFT web page is:  http://cr.yp.to/djbfft.html

*/

#ifndef _WDL_FFT_H_
#define _WDL_FFT_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WDL_FFT_REALSIZE
#define WDL_FFT_REALSIZE 4
#endif

#if WDL_FFT_REALSIZE == 4
typedef float WDL_FFT_REAL;
#elif WDL_FFT_REALSIZE == 8
typedef double WDL_FFT_REAL;
#else
#error invalid FFT item size
#endif

typedef struct {
  WDL_FFT_REAL re;
  WDL_FFT_REAL im;
} WDL_FFT_COMPLEX;

extern void WDL_fft_init();

extern void WDL_fft_complexmul(WDL_FFT_COMPLEX *dest, WDL_FFT_COMPLEX *src, int len);
extern void WDL_fft_complexmul2(WDL_FFT_COMPLEX *dest, WDL_FFT_COMPLEX *src, WDL_FFT_COMPLEX *src2, int len);
extern void WDL_fft_complexmul3(WDL_FFT_COMPLEX *destAdd, WDL_FFT_COMPLEX *src, WDL_FFT_COMPLEX *src2, int len);

/* Expects WDL_FFT_COMPLEX input[0..len-1] scaled by 1.0/len, returns
WDL_FFT_COMPLEX output[0..len-1] order by WDL_fft_permute(len). */
extern void WDL_fft(WDL_FFT_COMPLEX *, int len, int isInverse);

/* Expects WDL_FFT_REAL input[0..len-1] scaled by 0.5/len, returns
WDL_FFT_COMPLEX output[0..len/2-1], for len >= 4 order by
WDL_fft_permute(len/2). Note that output[len/2].re is stored in
output[0].im. */
extern void WDL_real_fft(WDL_FFT_REAL *, int len, int isInverse);

extern int WDL_fft_permute(int fftsize, int idx);
extern int *WDL_fft_permute_tab(int fftsize);

#ifdef __cplusplus
};
#endif

#endif