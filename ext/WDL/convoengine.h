/*
  WDL - convoengine.h
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
  

  This file provides an interface to the WDL fast convolution engine. This engine can convolve audio streams using
  either brute force (for small impulses), or a partitioned FFT scheme (for larger impulses). 

  Note that this library needs to have lookahead ability in order to process samples. Calling Add(somevalue) may produce Avail() < somevalue.

*/


#ifndef _WDL_CONVOENGINE_H_
#define _WDL_CONVOENGINE_H_

#include "queue.h"
#include "fastqueue.h"
#include "fft.h"

#define WDL_CONVO_MAX_IMPULSE_NCH 2
#define WDL_CONVO_MAX_PROC_NCH 2

//#define WDL_CONVO_WANT_FULLPRECISION_IMPULSE_STORAGE // define this for slowerness with -138dB error difference in resulting output (+-1 LSB at 24 bit)

#ifdef WDL_CONVO_WANT_FULLPRECISION_IMPULSE_STORAGE 

typedef WDL_FFT_REAL WDL_CONVO_IMPULSEBUFf;
typedef WDL_FFT_COMPLEX WDL_CONVO_IMPULSEBUFCPLXf;

#else
typedef float WDL_CONVO_IMPULSEBUFf;
typedef struct
{
  WDL_CONVO_IMPULSEBUFf re, im;
}
WDL_CONVO_IMPULSEBUFCPLXf;
#endif

class WDL_ImpulseBuffer
{
public:
  WDL_ImpulseBuffer() { samplerate=44100.0; m_nch=1; }
  ~WDL_ImpulseBuffer() { }

  int GetLength() { return impulses[0].GetSize(); }
  int SetLength(int samples); // resizes/clears all channels accordingly, returns actual size set (can be 0 if error)
  void SetNumChannels(int usench); // handles allocating/converting/etc
  int GetNumChannels() { return m_nch; }


  double samplerate;
  WDL_TypedBuf<WDL_FFT_REAL> impulses[WDL_CONVO_MAX_IMPULSE_NCH];

private:
  int m_nch;

};

class WDL_ConvolutionEngine
{
public:
  WDL_ConvolutionEngine();
  ~WDL_ConvolutionEngine();

  int SetImpulse(WDL_ImpulseBuffer *impulse, int fft_size=-1, int impulse_sample_offset=0, int max_imp_size=0, bool forceBrute=false);
 
  int GetFFTSize() { return m_fft_size; }
  int GetLatency() { return m_fft_size/2; }
  
  void Reset(); // clears out any latent samples

  void Add(WDL_FFT_REAL **bufs, int len, int nch);

  int Avail(int wantSamples);
  WDL_FFT_REAL **Get(); // returns length valid
  void Advance(int len);

private:
  WDL_TypedBuf<WDL_CONVO_IMPULSEBUFf> m_impulse[WDL_CONVO_MAX_IMPULSE_NCH]; // FFT'd data blocks per channel
  WDL_TypedBuf<char> m_impulse_zflag[WDL_CONVO_MAX_IMPULSE_NCH]; // FFT'd data blocks per channel

  int m_impulse_nch;
  int m_fft_size;
  int m_impulse_len;
  int m_proc_nch;

  WDL_Queue m_samplesout[WDL_CONVO_MAX_PROC_NCH];
  WDL_Queue m_samplesin2[WDL_CONVO_MAX_PROC_NCH];
  WDL_FastQueue m_samplesin[WDL_CONVO_MAX_PROC_NCH];

  int m_hist_pos[WDL_CONVO_MAX_PROC_NCH];

  WDL_TypedBuf<WDL_FFT_REAL> m_samplehist[WDL_CONVO_MAX_PROC_NCH]; // FFT'd sample blocks per channel
  WDL_TypedBuf<char> m_samplehist_zflag[WDL_CONVO_MAX_IMPULSE_NCH];
  WDL_TypedBuf<WDL_FFT_REAL> m_overlaphist[WDL_CONVO_MAX_PROC_NCH]; 
  WDL_TypedBuf<WDL_FFT_REAL> m_combinebuf;

  WDL_FFT_REAL *m_get_tmpptrs[WDL_CONVO_MAX_PROC_NCH];

public:

  // _div stuff
  int m_zl_delaypos;
  int m_zl_dumpage;

//#define WDLCONVO_ZL_ACCOUNTING
#ifdef WDLCONVO_ZL_ACCOUNTING
  int m_zl_fftcnt;//removeme (testing of benchmarks)
#endif
  void AddSilenceToOutput(int len, int nch);

} WDL_FIXALIGN;

// low latency version
class WDL_ConvolutionEngine_Div
{
public:
  WDL_ConvolutionEngine_Div();
  ~WDL_ConvolutionEngine_Div();

  int SetImpulse(WDL_ImpulseBuffer *impulse, int maxfft_size=0, int known_blocksize=0, int max_imp_size=0, int impulse_offset=0, int latency_allowed=0);

  int GetLatency();
  void Reset();

  void Add(WDL_FFT_REAL **bufs, int len, int nch);

  int Avail(int wantSamples);
  WDL_FFT_REAL **Get(); // returns length valid
  void Advance(int len);

private:
  WDL_PtrList<WDL_ConvolutionEngine> m_engines;

  WDL_Queue m_samplesout[WDL_CONVO_MAX_PROC_NCH];
  WDL_FFT_REAL *m_get_tmpptrs[WDL_CONVO_MAX_PROC_NCH];

  int m_proc_nch;
  bool m_need_feedsilence;

} WDL_FIXALIGN;


#endif