/*
  WDL - scsrc.h
  Copyright (C) 2007, Cockos Incorporated

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
  

  This file provides for an object to source a SHOUTcast (www.shoutcast.com) stream.
  It uses the lameencdec.h interface (and lame_enc.dll) to encode, and JNetLib to send the data.


  This object will not auto-reconnect on disconnect. If GetStatus() returns error, the callee needs to 
  delete the object and create a new one.


*/


#ifndef _WDL_SCSRC_H_
#define _WDL_SCSRC_H_

#include <time.h>
#include "jnetlib/connection.h"
#include "jnetlib/httpget.h"
#include "lameencdec.h"
#include "wdlstring.h"
#include "fastqueue.h"
#include "queue.h"
#include "mutex.h"

#include "resample.h"


class WDL_ShoutcastSource
{
public:
  WDL_ShoutcastSource(const char *host, const char *pass, const char *name, bool pub=false, 
                      const char *genre=NULL, const char *url=NULL,
                      
                      int nch=2, int srate=44100, int kbps=128,
                      const char *ircchan=NULL
                      );
  ~WDL_ShoutcastSource();

  int GetStatus(); // returns 0 if connected/connecting, >0 if disconnected, -1 if failed connect (or other error) from the start
  void GetStatusText(char *buf, int bufsz); // gets status text

  void SetCurTitle(const char *title);

  int GetSampleRate() { return m_srate; }
  void OnSamples(float **samples, int nch, int chspread, int frames, double srate); 
  int RunStuff(); // returns nonzero if work done
  

  void *userData;
  int totalBitrate; // 0 for normal, otherwise if using NSV (below) set to kbps of total stream
  // allows hooking to say, I dunno, package in some other format such as NSV?
  void (*sendProcessor)(void *userData, WDL_Queue *dataout, WDL_Queue *data); 

  int GetAudioBitrate() { return m_br*1000; }

private:

  WDL_Queue m_procdata;

  LameEncoder *m_encoder;
  int m_encoder_splsin;

  WDL_String m_host,m_pass,m_url,m_genre,m_name,m_ircchan;
  int m_br;
  bool m_pub;

  time_t m_titlecon_start,m_sendcon_start;

  unsigned int m_bytesout;
  int m_state;
  int m_nch,m_srate;

  WDL_Resampler m_rs;
  WDL_FastQueue m_samplequeue; // interleaved samples (float)

  JNL_HTTPGet *m_titlecon;
  JNL_Connection *m_sendcon;

  WDL_TypedBuf<float> m_workbuf;
  WDL_Mutex m_samplemutex;
  WDL_Mutex m_titlemutex;
  char m_title[512];
  bool m_needtitle;


  bool m_is_postmode;
  unsigned int m_postmode_session;
  int m_post_bytesleft;
  int m_post_postsleft;

  void PostModeConnect();

};

#endif // _WDL_SCSRC_H_