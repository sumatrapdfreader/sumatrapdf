/*
    WDL - mp3write.h
    Copyright (C) 2005 Cockos Incorporated
  
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

  This file provides a simple class for writing MP3 files (using lameencdec.h)
 
*/


#ifndef _MP3WRITE_H_
#define _MP3WRITE_H_


#include <stdio.h>
#include "lameencdec.h"

class mp3Writer
{
  public:
    // appending doesnt check sample types
    mp3Writer()
    {
      m_enc=0;
      m_fp=0;
      m_srate=0;
      m_nch=0;
    }

    mp3Writer(char *filename, int nch, int srate, int bitrate, int allow_append=1) 
    {
      m_enc=0;
      m_fp=0;
      m_srate=0;
      m_nch=0;
      Open(filename,nch,srate,bitrate,allow_append);

    }

    int Open(char *filename, int nch, int srate, int bitrate, int allow_append=1)
    {
      m_fp=0;
      if (allow_append)
      {
        m_fp=fopen(filename,"r+b");
        if (m_fp)
        {
          fseek(m_fp,0,SEEK_END);
        }
      }
      if (!m_fp)
      {
        m_fp=fopen(filename,"wb");
      }
      m_nch=nch>1?2:1;
      m_srate=srate;
      m_enc = new LameEncoder(srate,nch,bitrate);
      if (m_enc->Status())
      {
        delete m_enc;
        m_enc=0;
      }
      return m_fp && m_enc;
    }

    ~mp3Writer()
    {
      if (m_fp)
      {
        if (m_enc)
        {
          m_enc->Encode(NULL,0);
          if (m_enc->outqueue.Available())
          {
            fwrite(m_enc->outqueue.Get(),1,m_enc->outqueue.GetSize(),m_fp);
            fflush(m_fp);
            m_enc->outqueue.Advance(m_enc->outqueue.GetSize());
            m_enc->outqueue.Compact();
          }
        }

        fclose(m_fp);
        m_fp=0;
      }
      if (m_enc)
      {
        delete m_enc;
        m_enc=0;
      }
    }

    int Status() { return m_enc && m_fp; }

    void WriteFloats(float *samples, int nsamples)
    {
      if (!m_fp || !m_enc) return;

      m_enc->Encode(samples,nsamples/m_nch);
      if (m_enc->outqueue.Available())
      {
        fwrite(m_enc->outqueue.Get(),1,m_enc->outqueue.GetSize(),m_fp);
        fflush(m_fp);
        m_enc->outqueue.Advance(m_enc->outqueue.GetSize());
        m_enc->outqueue.Compact();
      }

    }

    int get_nch() { return m_nch; } 
    int get_srate() { return m_srate; }

  private:
    FILE *m_fp;
    int m_nch,m_srate;
    LameEncoder *m_enc;
};


#endif//_MP3WRITE_H_