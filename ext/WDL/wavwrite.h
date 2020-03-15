/*
    WDL - wavwrite.h
    Copyright (C) 2005 and later, Cockos Incorporated

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

/*

  This file provides a simple class for writing basic 16 or 24 bit PCM WAV files.
 
*/


#ifndef _WAVWRITE_H_
#define _WAVWRITE_H_


#include <stdio.h>
#include "pcmfmtcvt.h"
#include "wdlstring.h"

class WaveWriter
{
  public:
    // appending doesnt check sample types
    WaveWriter()
    {
      m_fp=0;
      m_bps=0;
      m_srate=0;
      m_nch=0;
    }

    WaveWriter(const char *filename, int bps, int nch, int srate, int allow_append=1) 
    {
      m_fp=0;
      m_bps=0;
      m_srate=0;
      m_nch=0;
      Open(filename,bps,nch,srate,allow_append);

    }

    int Open(const char *filename, int bps, int nch, int srate, int allow_append=1)
    {
      m_fn.Set(filename);
      m_fp=0;
      if (allow_append)
      {
        m_fp=fopen(filename,"r+b");
        if (m_fp)
        {
          fseek(m_fp,0,SEEK_END);
          int pos=ftell(m_fp);
          if (pos < 44)
          {
            char buf[44]={0,};
            fwrite(buf,1,44-pos,m_fp);
          }
        }
      }
      if (!m_fp)
      {
        m_fp=fopen(filename,"wb");
        if (!m_fp) return 0;

        char tbuf[44];
        fwrite(tbuf,1,44,m_fp); // room for header
      }
      m_bps=bps;
      m_nch=nch>1?2:1;
      m_srate=srate;

      return !!m_fp;
    }

    ~WaveWriter()
    {
      if (m_fp)
      {
        int bytelen=ftell(m_fp)-44;
        fseek(m_fp,0,SEEK_SET);

        // write header
        fwrite("RIFF",1,4,m_fp);
        int riff_size=bytelen+44-8;
        int x;
        for (x = 0; x < 32; x += 8)
        {
          unsigned char c=(riff_size>>x)&255;
          fwrite(&c,1,1,m_fp);
        }
        fwrite("WAVEfmt \x10\0\0\0",1,12,m_fp);
  			fwrite("\1\0",1,2,m_fp); // PCM

        for (x = 0; x < 16; x += 8) // nch
        {
          char c=(m_nch>>x)&255;
          fwrite(&c,1,1,m_fp);
        }
        for (x = 0; x < 32; x += 8) // srate
        {
          char c=(m_srate>>x)&255;
          fwrite(&c,1,1,m_fp);
        }
        for (x = 0; x < 32; x += 8) // bytes_per_sec
        {
          char c=((m_nch * (m_bps/8) * m_srate)>>x)&255;
          fwrite(&c,1,1,m_fp);
        }
        int blockalign=m_nch * (m_bps/8);
        for (x = 0; x < 16; x += 8) // block alignment
        {
          char c=(blockalign>>x)&255;
          fwrite(&c,1,1,m_fp);
        }
        for (x = 0; x < 16; x += 8) // bits/sample
        {
          char c=((m_bps&~7)>>x)&255;
          fwrite(&c,1,1,m_fp);
        }
        fwrite("data",1,4,m_fp);
        for (x = 0; x < 32; x += 8) // size
        {
          char c=((bytelen)>>x)&255;
          fwrite(&c,1,1,m_fp);
        }                

        fclose(m_fp);
        m_fp=0;
      }
    }

    const char *GetFileName() { return m_fn.Get(); }

    int Status() { return !!m_fp; }

    int BytesWritten()
    {
      if (m_fp) return ftell(m_fp)-44;
      return 0;
    }

    void WriteRaw(void *buf, int len)
    {
      if (m_fp) fwrite(buf,1,len,m_fp);
    }

    void WriteFloats(float *samples, int nsamples)
    {
      if (!m_fp) return;

      if (m_bps == 16)
      {
        while (nsamples-->0)
        {
          short a;
          float_TO_INT16(a,*samples);
          unsigned char c=a&0xff;
          fwrite(&c,1,1,m_fp);
          c=a>>8;
          fwrite(&c,1,1,m_fp);
          samples++;
        }
      }
      else if (m_bps == 24)
      {
        while (nsamples-->0)
        {
          unsigned char a[3];
          float_to_i24(samples,a);
          fwrite(a,1,3,m_fp);
          samples++;
        }
      }
    }

    void WriteDoubles(double *samples, int nsamples)
    {
      if (!m_fp) return;

      if (m_bps == 16)
      {
        while (nsamples-->0)
        {
          short a;
          double_TO_INT16(a,*samples);
          unsigned char c=a&0xff;
          fwrite(&c,1,1,m_fp);
          c=a>>8;
          fwrite(&c,1,1,m_fp);
          samples++;
        }
      }
      else if (m_bps == 24)
      {
        while (nsamples-->0)
        {
          unsigned char a[3];
          double_to_i24(samples,a);
          fwrite(a,1,3,m_fp);
          samples++;
        }
      }
    }

    void WriteFloatsNI(float **samples, int offs, int nsamples, int nchsrc=0)
    {
      if (!m_fp) return;

      if (nchsrc < 1) nchsrc=m_nch;

      float *tmpptrs[2]={samples[0]+offs,m_nch>1?(nchsrc>1?samples[1]+offs:samples[0]+offs):NULL};

      if (m_bps == 16)
      {
        while (nsamples-->0)
        {          
          int ch;
          for (ch = 0; ch < m_nch; ch ++)
          {
            short a;
            float_TO_INT16(a,tmpptrs[ch][0]);
            unsigned char c=a&0xff;
            fwrite(&c,1,1,m_fp);
            c=a>>8;
            fwrite(&c,1,1,m_fp);
            tmpptrs[ch]++;
          }
        }
      }
      else if (m_bps == 24)
      {
        while (nsamples-->0)
        {
          int ch;
          for (ch = 0; ch < m_nch; ch ++)
          {
            unsigned char a[3];
            float_to_i24(tmpptrs[ch],a);
            fwrite(a,1,3,m_fp);
            tmpptrs[ch]++;
          }
        }
      }
    }

    void WriteDoublesNI(double **samples, int offs, int nsamples, int nchsrc=0)
    {
      if (!m_fp) return;

      if (nchsrc < 1) nchsrc=m_nch;

      double *tmpptrs[2]={samples[0]+offs,m_nch>1?(nchsrc>1?samples[1]+offs:samples[0]+offs):NULL};

      if (m_bps == 16)
      {
        while (nsamples-->0)
        {          
          int ch;
          for (ch = 0; ch < m_nch; ch ++)
          {
            short a;
            double_TO_INT16(a,tmpptrs[ch][0]);
            unsigned char c=a&0xff;
            fwrite(&c,1,1,m_fp);
            c=a>>8;
            fwrite(&c,1,1,m_fp);
            tmpptrs[ch]++;
          }
        }
      }
      else if (m_bps == 24)
      {
        while (nsamples-->0)
        {
          int ch;
          for (ch = 0; ch < m_nch; ch ++)
          {
            unsigned char a[3];
            double_to_i24(tmpptrs[ch],a);
            fwrite(a,1,3,m_fp);
            tmpptrs[ch]++;
          }
        }
      }
    }


    int get_nch() { return m_nch; } 
    int get_srate() { return m_srate; }
    int get_bps() { return m_bps; }

  private:
    WDL_String m_fn;
    FILE *m_fp;
    int m_bps,m_nch,m_srate;
};


#endif//_WAVWRITE_H_