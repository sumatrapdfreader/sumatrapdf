/*
  WDL - lameencdec.cpp
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
  


*/



#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wdlcstring.h"
#include "lameencdec.h"


#ifdef __APPLE__
  #include <Carbon/Carbon.h>
#endif
#ifndef _WIN32
  #include <dlfcn.h>
#endif

typedef enum MPEG_mode_e {
  STEREO = 0,
  JOINT_STEREO,
  DUAL_CHANNEL,   /* LAME doesn't supports this! */
  MONO,
  NOT_SET,
  MAX_INDICATOR   /* Don't use this! It's used for sanity checks. */
} MPEG_mode;

typedef void *lame_t;

static struct {
  int (*close)(lame_t);
  lame_t (*init)();
  int (*set_in_samplerate)(lame_t, int);
  int (*set_num_channels)(lame_t,int);
  int (*set_out_samplerate)(lame_t,int);
  int (*set_quality)(lame_t,int);
  int (*set_mode)(lame_t,MPEG_mode);
  int (*set_brate)(lame_t, int);
  int (*init_params)(lame_t);
  int (*get_framesize)(lame_t);

  int (*encode_buffer_float)(lame_t,
          const float     buffer_l [],       /* PCM data for left channel     */
          const float     buffer_r [],       /* PCM data for right channel    */
          const int           nsamples,      /* number of samples per channel */
          unsigned char*      mp3buf,        /* pointer to encoded MP3 stream */
          const int           mp3buf_size ); 
  int (*encode_flush)(lame_t,unsigned char*       mp3buf,  int                  size);

// these are optional
  int (*set_VBR)(lame_t, int);
  int (*set_VBR_q)(lame_t, int);
  int (*set_VBR_mean_bitrate_kbps)(lame_t, int);
  int (*set_VBR_min_bitrate_kbps)(lame_t, int);
  int (*set_VBR_max_bitrate_kbps)(lame_t, int);
  size_t (*get_lametag_frame)(lame_t, unsigned char *, size_t);
  const char *(*get_lame_version)();
  int (*set_findReplayGain)(lame_t, int);
} lame;

#if 1
#define LAME_DEBUG_LOADING(x) 
#else
#define LAME_DEBUG_LOADING(x) OutputDebugString(x)
#endif

static char s_last_dll_file[128];

static bool tryLoadDLL2(const char *name)
{
#ifdef _WIN32
  HINSTANCE dll = LoadLibrary(name);
#else
  void *dll=dlopen(name,RTLD_NOW|RTLD_LOCAL);
#endif

  if (!dll) return false;

  LAME_DEBUG_LOADING("trying to load");
  LAME_DEBUG_LOADING(name);
  int errcnt = 0;
  #ifdef _WIN32
    #define GETITEM(x) if (NULL == (*(void **)&lame.x = GetProcAddress((HINSTANCE)dll,"lame_" #x))) errcnt++;
    #define GETITEM_NP(x) if (NULL == (*(void **)&lame.x = GetProcAddress((HINSTANCE)dll, #x))) errcnt++;
  #else
    #define GETITEM(x) if (NULL == (*(void **)&lame.x = dlsym(dll,"lame_" #x))) errcnt++;
    #define GETITEM_NP(x) if (NULL == (*(void **)&lame.x = dlsym(dll,#x))) errcnt++;
  #endif
  GETITEM(close)
  GETITEM(init)
  GETITEM(set_in_samplerate)
  GETITEM(set_num_channels)
  GETITEM(set_out_samplerate)
  GETITEM(set_quality)
  GETITEM(set_mode)
  GETITEM(set_brate)
  GETITEM(init_params)
  GETITEM(get_framesize)
  GETITEM(encode_buffer_float)
  GETITEM(encode_flush)

  int errcnt2 = errcnt;
  GETITEM(set_VBR)
  GETITEM(set_VBR_q)
  GETITEM(set_VBR_mean_bitrate_kbps)
  GETITEM(set_VBR_min_bitrate_kbps)
  GETITEM(set_VBR_max_bitrate_kbps)
  GETITEM(get_lametag_frame)
  GETITEM(set_findReplayGain)
  GETITEM_NP(get_lame_version)
  #undef GETITEM   
  #undef GETITEM_NP
  if (errcnt2)
  {
    memset(&lame, 0, sizeof(lame));

#ifdef _WIN32
    FreeLibrary(dll);
#else
    dlclose(dll);
#endif
    return false;
  }

  LAME_DEBUG_LOADING("loaded normal mode");

  lstrcpyn_safe(s_last_dll_file, name, sizeof(s_last_dll_file));
#ifdef _WIN32
//  if (!strstr(name,"\\"))
  GetModuleFileName(dll,s_last_dll_file, (DWORD)sizeof(s_last_dll_file));
#else
//  if (!strstr(name,"/"))
  {
    Dl_info inf={0,};
    dladdr((void*)lame.init,&inf);
    if (inf.dli_fname)
      lstrcpyn_safe(s_last_dll_file,inf.dli_fname,sizeof(s_last_dll_file));
  }
#endif

  return true;
}



void LameEncoder::InitDLL(const char *extrapath, bool forceRetry)
{
  static int a;
  if (a<0) return;

  if (forceRetry) a=0;
  else if (a > 30) return; // give up

  a++;
  
  char me[1024];
#ifdef _WIN32
  const char *dll = "libmp3lame.dll";
#elif defined(__APPLE__)
  const char *dll = "libmp3lame.dylib";
#else
  const char *dll = "libmp3lame.so.0";
#endif

  if (extrapath)
  {
    snprintf(me,sizeof(me),"%s%c%s",extrapath,WDL_DIRCHAR,dll);
    if (tryLoadDLL2(me)) { a = -1; return; }
  }

  if (tryLoadDLL2(dll)) a=-1;

}

const char *LameEncoder::GetLibName() { return s_last_dll_file; }

const char *LameEncoder::GetInfo()
{
  static char buf[128];
  if (!CheckDLL()) return NULL;
  const char *p = lame.get_lame_version ? lame.get_lame_version() : NULL;
  if (p && *p)
  {
    snprintf(buf, sizeof(buf), "LAME %s", p);
    return buf;
  }
  return "LAME ?.??";
}

int LameEncoder::CheckDLL() // returns 1 for lame API, 2 for Blade, 0 for none
{
  InitDLL();
  if (!lame.close||
      !lame.init||
      !lame.set_in_samplerate||
      !lame.set_num_channels||
      !lame.set_out_samplerate||
      !lame.set_quality||
      !lame.set_mode||
      !lame.set_brate||
      !lame.init_params||
      !lame.get_framesize||
      !lame.encode_buffer_float||
      !lame.encode_flush) 
  {
    return 0;
  }

  return 1;

}

LameEncoder::LameEncoder(int srate, int nch, int bitrate, int stereomode, int quality, int vbrmethod, int vbrquality, int vbrmax, int abr, int rpgain)
{
  m_lamestate=0;
  if (!CheckDLL())
  {
    errorstat=1;
    return;
  }

  errorstat=0;
  m_nch=nch;
  m_encoder_nch = stereomode == 3 ? 1 : m_nch;


  m_lamestate=lame.init();
  if (!m_lamestate)
  {
    errorstat=1; 
    return;
  }

  lame.set_in_samplerate(m_lamestate, srate);
  lame.set_num_channels(m_lamestate,m_encoder_nch);
  int outrate=srate;

  int maxbr = (vbrmethod != -1 ? vbrmax : bitrate);
  if (outrate>=32000 && maxbr <= 32*m_encoder_nch) outrate/=2;

  lame.set_out_samplerate(m_lamestate,outrate);
  lame.set_quality(m_lamestate,(quality>9 ||quality<0) ? 0 : quality);
  if (m_encoder_nch == 1 || stereomode >= 0)
    lame.set_mode(m_lamestate,(MPEG_mode) (m_encoder_nch==1?3 :stereomode ));
  lame.set_brate(m_lamestate,bitrate);
  
  //int vbrmethod (-1 no vbr), int vbrquality (nVBRQuality), int vbrmax, int abr
  if (vbrmethod != -1 && lame.set_VBR)
  {
    int vm=4; // mtrh
    if (vbrmethod == 4) vm = 3; //ABR
    lame.set_VBR(m_lamestate,vm);
    
    if (lame.set_VBR_q) lame.set_VBR_q(m_lamestate,vbrquality);
    
    if (vbrmethod == 4&&lame.set_VBR_mean_bitrate_kbps)
    {
      lame.set_VBR_mean_bitrate_kbps(m_lamestate,abr);
    }
    if (lame.set_VBR_max_bitrate_kbps)
    {
      lame.set_VBR_max_bitrate_kbps(m_lamestate,vbrmax);
    }
    if (lame.set_VBR_min_bitrate_kbps)
    {
      lame.set_VBR_min_bitrate_kbps(m_lamestate,bitrate);
    }
  }
  if (rpgain>0 && lame.set_findReplayGain) lame.set_findReplayGain(m_lamestate,1);

  lame.init_params(m_lamestate);
  in_size_samples=lame.get_framesize(m_lamestate);

  outtmp.Resize(65536);
}

void LameEncoder::Encode(float *in, int in_spls, int spacing)
{
  if (errorstat) return;

  if (in_spls > 0)
  {
    if (m_nch > 1 && m_encoder_nch==1)
    {
      // downmix
      int x;
      int pos=0;
      int adv=2*spacing;
      for (x = 0; x < in_spls; x ++)
      {
        float f=in[pos]+in[pos+1];
        f*=16384.0f;
        spltmp[0].Add(&f,sizeof(float));
        pos+=adv;
      }
    }
    else if (m_encoder_nch > 1) // deinterleave
    {
      int x;
      int pos=0;
      int adv=2*spacing;
      for (x = 0; x < in_spls; x ++)
      {
        float f=in[pos];
        f*=32768.0f;
        spltmp[0].Add(&f,sizeof(float));

        f=in[pos+1];
        f*=32768.0f;
        spltmp[1].Add(&f,sizeof(float));

        pos+=adv;
      }
    }
    else 
    {
      int x;
      int pos=0;
      for (x = 0; x < in_spls; x ++)
      {
        float f=in[pos];
        f*=32768.0f;
        spltmp[0].Add(&f,sizeof(float));

        pos+=spacing;
      }
    }
  }
  for (;;)
  {
    int a = spltmp[0].Available()/sizeof(float);
    if (a >= in_size_samples) a = in_size_samples;
    else if (a<1 || in_spls>0) break; // not enough samples available, and not flushing

    int dwo=lame.encode_buffer_float(m_lamestate,(float *)spltmp[0].Get(),(float*)spltmp[m_encoder_nch>1].Get(), a,(unsigned char *)outtmp.Get(),outtmp.GetSize());
    outqueue.Add(outtmp.Get(),dwo);
    spltmp[0].Advance(a*sizeof(float));
    if (m_encoder_nch > 1) spltmp[1].Advance(a*sizeof(float));
  }

  if (in_spls<1)
  {
    int a=lame.encode_flush(m_lamestate,(unsigned char *)outtmp.Get(),outtmp.GetSize());
    if (a>0) outqueue.Add(outtmp.Get(),a);
  }



  spltmp[0].Compact();
  spltmp[1].Compact();

}

#ifdef _WIN32

static BOOL HasUTF8(const char *_str)
{
  const unsigned char *str = (const unsigned char *)_str;
  if (!str) return FALSE;
  while (*str) 
  {
    unsigned char c = *str;
    if (c >= 0xC2) // discard overlongs
    {
      if (c <= 0xDF && str[1] >=0x80 && str[1] <= 0xBF) return TRUE;
      else if (c <= 0xEF && str[1] >=0x80 && str[1] <= 0xBF && str[2] >=0x80 && str[2] <= 0xBF) return TRUE;
      else if (c <= 0xF4 && str[1] >=0x80 && str[1] <= 0xBF && str[2] >=0x80 && str[2] <= 0xBF) return TRUE;
    }
    str++;
  }
  return FALSE;
}
#endif

LameEncoder::~LameEncoder()
{
  if (m_lamestate)
  {
    if (m_vbrfile.Get()[0] && lame.get_lametag_frame)
    {
      unsigned char buf[16384];
      size_t a=lame.get_lametag_frame(m_lamestate,buf,sizeof(buf));
      if (a>0 && a<=sizeof(buf))
      {
        FILE *fp=NULL;
#ifdef _WIN32
        if (HasUTF8(m_vbrfile.Get()) && GetVersion()<0x80000000)
        {
          WCHAR wf[2048];
          if (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,m_vbrfile.Get(),-1,wf,2048))
          {
            fp = _wfopen(wf,L"r+b");
          }
        }
#endif
        if (!fp) fp = fopen(m_vbrfile.Get(),"r+b");
        if (fp)
        {
          fseek(fp,0,SEEK_SET);
          fwrite(buf,1,a,fp);
          fclose(fp);
        }
      }
    }
    lame.close(m_lamestate);
    m_lamestate=0;
  }
}

