/*
  WDL - lameencdec.h
  Copyright (C) 2005 and later Cockos Incorporated

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
  

  This file provides a simple interface for using lame_enc/libmp3lame MP3 encoding

*/


#ifndef _LAMEENCDEC_H_
#define _LAMEENCDEC_H_

#include "queue.h"
#include "wdlstring.h"
#include "assocarray.h"

class LameEncoder
{
  public:

    LameEncoder(int srate, int nch, int bitrate, int stereomode=1, int quality=2,
      int vbrmethod=-1, int vbrquality=2, int vbrmax=320, int abr=128, int rpgain=0,
      WDL_StringKeyedArray<char*> *metadata=NULL);
    ~LameEncoder();

    int Status() { return errorstat; } // 1=no dll, 2=error

    void Encode(float *in, int in_spls, int spacing=1);

    WDL_Queue outqueue;

    void reinit() 
    { 
      spltmp[0].Advance(spltmp[0].Available());  
      spltmp[0].Compact();
      spltmp[1].Advance(spltmp[1].Available());  
      spltmp[1].Compact();
    }

    static const char *GetInfo();
    static const char *GetLibName();
    static int CheckDLL(); // returns >0 if DLL present, 1 for lame, 2 for old bladeenc
    static void InitDLL(const char *extrapath=NULL, bool forceRetry=false); // call with extrapath != NULL if you want to try loading from another path

    void SetVBRFilename(const char *fn)
    {
      m_vbrfile.Set(fn);
    }

    int GetNumChannels() { return m_encoder_nch; }
    
  private:

    void SetMetadata(WDL_StringKeyedArray<char*> *metadata);
    int m_id3_len;

    void *m_lamestate;
    WDL_Queue spltmp[2];
    WDL_HeapBuf outtmp;
    WDL_String m_vbrfile;
    int errorstat;
    int in_size_samples;
    int m_nch,m_encoder_nch;
};

#endif
