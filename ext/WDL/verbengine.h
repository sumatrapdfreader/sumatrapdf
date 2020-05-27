#ifndef _VERBENGINE_H_
#define _VERBENGINE_H_


/*
    WDL - verbengine.h
    Copyright (C) 2007 and later Cockos Incorporated

    This is based on the public domain FreeVerb source:
      by Jezar at Dreampoint, June 2000
      http://www.dreampoint.co.uk

    Filter tweaks and general guidance thanks to Thomas Scott Stillwell.

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


#include "heapbuf.h"


#include "denormal.h"

class WDL_ReverbAllpass
{
public:
  WDL_ReverbAllpass() { feedback=0.5; setsize(1); }
  ~WDL_ReverbAllpass() { }

  void setsize(int size)
  {
    if (size<1)size=1;
    if (buffer.GetSize()!=size)
    {
      bufidx=0;
      buffer.Resize(size);
      Reset();
    }
  }

	double process(double inp)
  {
    double *bptr=buffer.Get()+bufidx;

	  double bufout = *bptr;
	  
	  double output = bufout - inp;
	  *bptr = denormal_filter_double(inp + (bufout*feedback));

	  if(++bufidx>=buffer.GetSize()) bufidx = 0;

	  return output;
  }
  void Reset() { memset(buffer.Get(),0,buffer.GetSize()*sizeof(double)); }
  void setfeedback(double val) { feedback=val; }

private:
	double	feedback;
	WDL_TypedBuf<double> buffer;
	int		bufidx;
public:
  int __pad;

} WDL_FIXALIGN;

  
class WDL_ReverbComb
{
public:
  WDL_ReverbComb() { feedback=0.5; damp=0.5; filterstore=0; setsize(1); }
  ~WDL_ReverbComb() { }

  void setsize(int size)
  {
    if (size<1)size=1;
    if (buffer.GetSize()!=size)
    {
      bufidx=0;
      buffer.Resize(size);
      Reset();
    }
  }

	double process(double inp)
  {
    double *bptr=buffer.Get()+bufidx;
	  double output = *bptr;
	  filterstore = denormal_filter_double((output*(1-damp)) + (filterstore*damp));

	  *bptr = inp + (filterstore*feedback);

	  if(++bufidx>=buffer.GetSize()) bufidx = 0;

	  return output;
  }
  void Reset() { filterstore=0; memset(buffer.Get(),0,buffer.GetSize()*sizeof(double)); }
  void setdamp(double val) { damp=val;  }
  void setfeedback(double val) { feedback=val; }

private:

	double	feedback;
	double	filterstore;
	double	damp;
	WDL_TypedBuf<double> buffer;
	int		bufidx;
public:
  int __pad;
} WDL_FIXALIGN;

  // these represent lengths in samples at 44.1khz but are scaled accordingly
const int wdl_verb__stereospread=23;
const short wdl_verb__combtunings[]={1116,1188,1277,1356,1422,1491,1557,1617,1685,1748};
const short wdl_verb__allpasstunings[]={556,441,341,225,180,153};


class WDL_ReverbEngine
{
public:
  WDL_ReverbEngine()
  {
    m_srate=44100.0;
    m_roomsize=0.5;
    m_damp=0.5;
    SetWidth(1.0);
    Reset(false);
  }
  ~WDL_ReverbEngine()
  {
  }
  void SetSampleRate(double srate)
  {
    if (m_srate!=srate)
    {
      m_srate=srate;
      Reset(true);
    }
  }

  void ProcessSampleBlock(double *spl0, double *spl1, double *outp0, double *outp1, int ns)
  {
    int x;
    memset(outp0,0,ns*sizeof(double));
    memset(outp1,0,ns*sizeof(double));

    for (x = 0; x < sizeof(wdl_verb__combtunings)/sizeof(wdl_verb__combtunings[0]); x += 2)
    {
      int i=ns;
      double *p0=outp0,*p1=outp1,*i0=spl0,*i1=spl1;
      while (i--)
      {        
        double a=*i0++,b=*i1++;
        *p0+=m_combs[x][0].process(a); 
        *p1+=m_combs[x][1].process(b);
        *p0+++=m_combs[x+1][0].process(a); 
        *p1+++=m_combs[x+1][1].process(b);
      }
    }
    for (x = 0; x < sizeof(wdl_verb__allpasstunings)/sizeof(wdl_verb__allpasstunings[0])-2; x += 2)
    {
      int i=ns;
      double *p0=outp0,*p1=outp1;
      while (i--)
      {        
        double tmp=m_allpasses[x][0].process(*p0);
        double tmp2=m_allpasses[x][1].process(*p1);
        *p0++=m_allpasses[x+1][0].process(tmp);
        *p1++=m_allpasses[x+1][1].process(tmp2);
      }
    }
    int i=ns;
    double *p0=outp0,*p1=outp1;
    while (i--)
    {        
      double a=m_allpasses[x+1][0].process(m_allpasses[x][0].process(*p0))*0.015;
      double b=m_allpasses[x+1][1].process(m_allpasses[x][1].process(*p1))*0.015;

      if (m_wid<0)
      {
        double m=-m_wid;
        *p0 = b*m + a*(1.0-m);
        *p1 = a*m + b*(1.0-m);
      }
      else
      {
        double m=m_wid;
        *p0 = a*m + b*(1.0-m);
        *p1 = b*m + a*(1.0-m);
      }
      p0++;
      p1++;
    }
    
  }

  void ProcessSample(double *spl0, double *spl1)
  {
    int x;
    double in0=*spl0 * 0.015;
    double in1=*spl1 * 0.015;

    double out0=0.0;
    double out1=0.0;
    for (x = 0; x < sizeof(wdl_verb__combtunings)/sizeof(wdl_verb__combtunings[0]); x ++)
    {
      out0+=m_combs[x][0].process(in0);
      out1+=m_combs[x][1].process(in1);
    }
    for (x = 0; x < sizeof(wdl_verb__allpasstunings)/sizeof(wdl_verb__allpasstunings[0]); x ++)
    {
      out0=m_allpasses[x][0].process(out0);
      out1=m_allpasses[x][1].process(out1);
    }

    if (m_wid<0)
    {
      double m=-m_wid;
      *spl0 = out1*m + out0*(1.0-m);
      *spl1 = out0*m + out1*(1.0-m);
    }
    else
    {
      double m=m_wid;
      *spl0 = out0*m + out1*(1.0-m);
      *spl1 = out1*m + out0*(1.0-m);
    }
  }

  void Reset(bool doclear=false) // call this after changing roomsize or dampening
  {
    int x;
    double sc=m_srate / 44100.0;
    for (x = 0; x < sizeof(wdl_verb__allpasstunings)/sizeof(wdl_verb__allpasstunings[0]); x ++)
    {
      m_allpasses[x][0].setsize((int) (wdl_verb__allpasstunings[x] * sc));
      m_allpasses[x][1].setsize((int) ((wdl_verb__allpasstunings[x]+wdl_verb__stereospread) * sc));
      m_allpasses[x][0].setfeedback(0.5);
      m_allpasses[x][1].setfeedback(0.5);
      if (doclear)
      {
        m_allpasses[x][0].Reset();
        m_allpasses[x][1].Reset();
      }
    }
    for (x = 0; x < sizeof(wdl_verb__combtunings)/sizeof(wdl_verb__combtunings[0]); x ++)
    {
      m_combs[x][0].setsize((int) (wdl_verb__combtunings[x] * sc));
      m_combs[x][1].setsize((int) ((wdl_verb__combtunings[x]+wdl_verb__stereospread) * sc));
      m_combs[x][0].setfeedback(m_roomsize);
      m_combs[x][1].setfeedback(m_roomsize);
      m_combs[x][0].setdamp(m_damp*0.4);
      m_combs[x][1].setdamp(m_damp*0.4);
      if (doclear)
      {
        m_combs[x][0].Reset();
        m_combs[x][1].Reset();
      }
    }

  }

  void SetRoomSize(double sz) { m_roomsize=sz;; } // 0.3..0.99 or so
  void SetDampening(double dmp) { m_damp=dmp; } // 0..1
  void SetWidth(double wid) 
  {  
    if (wid<-1) wid=-1; 
    else if (wid>1) wid=1; 
    wid*=0.5;
    if (wid>=0.0) wid+=0.5;
    else wid-=0.5;
    m_wid=wid;
  } // -1..1

private:
  double m_wid;
  double m_roomsize;
  double m_damp;
  double m_srate;
  WDL_ReverbAllpass m_allpasses[sizeof(wdl_verb__allpasstunings)/sizeof(wdl_verb__allpasstunings[0])][2];
  WDL_ReverbComb m_combs[sizeof(wdl_verb__combtunings)/sizeof(wdl_verb__combtunings[0])][2];

};


#endif
