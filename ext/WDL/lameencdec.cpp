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
#include "win32_utf8.h"

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

LameEncoder::LameEncoder(int srate, int nch, int bitrate, int stereomode, int quality,
  int vbrmethod, int vbrquality, int vbrmax, int abr, int rpgain,
  WDL_StringKeyedArray<char*> *metadata)
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
  m_id3_len=0;

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

  if (metadata && metadata->GetSize())
  {
    SetMetadata(metadata);
  }

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
        FILE *fp = fopenUTF8(m_vbrfile.Get(),"r+b");
        if (fp)
        {
          fseek(fp, m_id3_len, SEEK_SET);
          fwrite(buf,1,a,fp);
          fclose(fp);
        }
      }
    }
    lame.close(m_lamestate);
    m_lamestate=0;
  }
}


#define _AddSyncSafeInt32(i) \
  *p++=(((i)>>21)&0x7F); \
  *p++=(((i)>>14)&0x7F); \
  *p++=(((i)>>7)&0x7F); \
  *p++=((i)&0x7F);

#define _AddInt32(i) \
  *p++=(((i)>>24)&0xFF); \
  *p++=(((i)>>16)&0xFF); \
  *p++=(((i)>>8)&0xFF); \
  *p++=((i)&0xFF);

#define CTOC_NAME "TOC" // arbitrary name of table of contents element

unsigned char *PackID3Chunk(WDL_StringKeyedArray<char*> *metadata, int *buflen)
{
  if (!metadata || !buflen) return NULL;

  unsigned char *buf=NULL;
  *buflen=0;

  int id3len=0;
  int i;
  for (i=0; i < metadata->GetSize(); ++i)
  {
    const char *id;
    const char *val=metadata->Enumerate(i, &id);
    if (strlen(id) < 8 || strncmp(id, "ID3:", 4) || !val) continue;
    id += 4;
    if (strlen(id) == 4 && id[0] == 'T')
    {
      id3len += 10+1+strlen(val);
      if (!strcmp(id, "TXXX"))
      {
        // tag form is "desc=val", default to "USER" if desc is not supplied
        const char *sep=strchr(val, '=');
        if (!sep) id3len += 5;
        else if (sep == val) id3len += 4;
      }
    }
    else if (!strcmp(id, "COMM"))
    {
      id3len += 10+5+strlen(val);
    }
  }

  WDL_HeapBuf apic_hdr;
  int apic_datalen=0;
  const char *apic_fn=metadata->Get("ID3:APIC_FILE");
  if (apic_fn && apic_fn[0])
  {
    const char *mime=NULL;
    const char *ext=WDL_get_fileext(apic_fn);
    if (ext && (!stricmp(ext, ".jpg") || !stricmp(ext, ".jpeg"))) mime="image/jpeg";
    else if (ext && !stricmp(ext, ".png")) mime="image/png";
    if (mime)
    {
      FILE *fp=fopenUTF8(apic_fn, "rb"); // could stat but let's make sure we can open the file
      if (fp)
      {
        fseek(fp, 0, SEEK_END);
        apic_datalen=ftell(fp);
        fclose(fp);
      }
    }
    if (apic_datalen)
    {
      const char *t=metadata->Get("ID3:APIC_TYPE");
      int type=-1;
      if (t && t[0] >= '0' && t[0] <= '9') type=atoi(t);
      if (type < 0 || type >= 16) type=3; // default "Cover (front)"

      const char *desc=metadata->Get("ID3:APIC_DESC");
      if (!desc) desc="";
      int desclen=wdl_min(strlen(desc), 63);

      int apic_hdrlen=1+strlen(mime)+1+1+desclen+1;
      char *p=(char*)apic_hdr.Resize(apic_hdrlen);
      if (p)
      {
        *p++=3; // UTF-8
        memcpy(p, mime, strlen(mime)+1);
        p += strlen(mime)+1;
        *p++=type;
        memcpy(p, desc, desclen);
        p += desclen;
        *p++=0;
        id3len += 10+apic_hdrlen+apic_datalen;
      }
    }
  }

  int chapcnt=0, toclen=0;
  char idbuf[512];
  snprintf(idbuf,sizeof(idbuf), "ID3:CHAP%d", chapcnt+1);
  const char *val=metadata->Get(idbuf);
  while (val)
  {
    const char *c1=strchr(val, ':');
    const char *c2=(c1 ? strchr(c1+1, ':') : NULL);
    if (WDL_NOT_NORMALLY(!c1 || !c2)) break;

    const char *id=idbuf+4;
    int idlen=strlen(id);
    int namelen=strlen(c2+1);
    toclen += idlen+1;
    id3len += 10+idlen+1+4*4+10+1+namelen+1;

    if (++chapcnt == 255) break;
    snprintf(idbuf,sizeof(idbuf), "ID3:CHAP%d", chapcnt+1);
    val=metadata->Get(idbuf);
  }
  if (toclen)
  {
    toclen += strlen(CTOC_NAME)+1+2;
    id3len += 10+toclen;
  }

  if (id3len)
  {
    id3len += 10;
    buf=(unsigned char*)malloc(id3len);
    unsigned char *p=buf;
    memcpy(p,"ID3\x04\x00\x00", 6);
    p += 6;
    _AddSyncSafeInt32(id3len-10);
    for (i=0; i < metadata->GetSize(); ++i)
    {
      const char *id;
      const char *val=metadata->Enumerate(i, &id);
      if (strlen(id) < 8 || strncmp(id, "ID3:", 4) || !val) continue;
      id += 4;
      if (strlen(id) == 4 && !strcmp(id, "TXXX"))
      {
        memcpy(p, id, 4);
        p += 4;
        const char *sep=strchr(val, '=');
        if (sep == val)
        {
          ++val;
          sep=NULL;
        }
        int len=strlen(val);
        int tlen=len+(!sep ? 5 : 0);
        _AddSyncSafeInt32(1+tlen);
        memcpy(p, "\x00\x00\x03", 3); // UTF-8
        p += 3;
        if (sep)
        {
          memcpy(p, val, len);
          p[sep-val]=0;
        }
        else
        {
          memcpy(p, "USER\x00", 5);
          p += 5;
          memcpy(p, val, len);
        }
        p += len;
      }
      else if (strlen(id) == 4 && id[0] == 'T')
      {
        memcpy(p, id, 4);
        p += 4;
        int len=strlen(val);
        _AddSyncSafeInt32(1+len);
        memcpy(p, "\x00\x00\x03", 3); // UTF-8
        p += 3;
        memcpy(p, val, len);
        p += len;
      }
      else if (!strcmp(id, "COMM"))
      {
        // http://www.loc.gov/standards/iso639-2/php/code_list.php
        // most apps ignore this, itunes wants "eng" or something locale-specific
        const char *lang=metadata->Get("ID3:COMM_LANG");

        memcpy(p, id, 4);
        p += 4;
        int len=strlen(val);
        _AddSyncSafeInt32(5+len);
        memcpy(p, "\x00\x00\x03", 3); // UTF-8
        p += 3;
        if (lang && strlen(lang) >= 3 &&
          tolower(*lang) >= 'a' && tolower(*lang) <= 'z')
        {
          *p++=tolower(*lang++);
          *p++=tolower(*lang++);
          *p++=tolower(*lang++);
          *p++=0;
        }
        else
        {
          // some apps write "XXX" for "no particular language"
          memcpy(p, "XXX\x00", 4);
          p += 4;
        }
        memcpy(p, val, len);
        p += len;
      }
    }

    if (toclen)
    {
      memcpy(p, "CTOC", 4);
      p += 4;
      _AddSyncSafeInt32(toclen);
      memcpy(p, "\x00\x00", 2);
      p += 2;
      memcpy(p, CTOC_NAME, strlen(CTOC_NAME)+1);
      p += strlen(CTOC_NAME)+1;
      *p++=3; // CTOC flags: &1=top level, &2=ordered
      *p++=(chapcnt&0xFF);

      for (i=0; i < chapcnt; ++i)
      {
        snprintf(idbuf,sizeof(idbuf), "ID3:CHAP%d", i+1);
        const char *val=metadata->Get(idbuf);
        if (WDL_NOT_NORMALLY(!val)) break;
        const char *c1=strchr(val, ':');
        const char *c2=(c1 ? strchr(c1+1, ':') : NULL);
        if (WDL_NOT_NORMALLY(!c1 || !c2)) break;

        const char *id=idbuf+4;
        int idlen=strlen(id);
        memcpy(p, id, idlen+1);
        p += idlen+1;
      }

      for (i=0; i < chapcnt; ++i)
      {
        snprintf(idbuf,sizeof(idbuf), "ID3:CHAP%d", i+1);
        const char *val=metadata->Get(idbuf);
        if (WDL_NOT_NORMALLY(!val)) break;
        const char *c1=strchr(val, ':');
        const char *c2=(c1 ? strchr(c1+1, ':') : NULL);
        if (WDL_NOT_NORMALLY(!c1 || !c2)) break;

        const char *id=idbuf+4;
        int idlen=strlen(id);
        int st=atoi(val);
        int et=atoi(c1+1);
        const char *name=c2+1;
        int namelen=strlen(name);

        memcpy(p, "CHAP", 4);
        p += 4;
        _AddSyncSafeInt32(idlen+1+4*4+10+1+namelen+1);
        memcpy(p, "\x00\x00", 2);
        p += 2;
        memcpy(p, id, idlen+1);
        p += idlen+1;
        _AddInt32(st);
        _AddInt32(et);
        p += 8;
        memcpy(p, "TIT2", 4);
        p += 4;
        _AddSyncSafeInt32(1+namelen+1);
        memcpy(p, "\x00\x00\x03", 3); // UTF-8
        p += 3;
        memcpy(p, name, namelen+1);
        p += namelen+1;
      }
    }

    if (apic_hdr.GetSize() && apic_datalen)
    {
      memcpy(p, "APIC", 4);
      p += 4;
      int len=apic_hdr.GetSize()+apic_datalen;
      _AddSyncSafeInt32(len);
      memcpy(p, "\x00\x00", 2);
      p += 2;
      memcpy(p, apic_hdr.Get(), apic_hdr.GetSize());
      p += apic_hdr.GetSize();
      FILE *fp=fopenUTF8(apic_fn, "rb");
      if (WDL_NORMALLY(fp))
      {
        fread(p, 1, apic_datalen, fp);
        fclose(fp);
      }
      else // uh oh
      {
        memset(p, 0, apic_datalen);
      }
      p += apic_datalen;
    }

    if (WDL_NOT_NORMALLY(p-buf != id3len))
    {
      free(buf);
      buf=NULL;
    }
  }

  if (buf) *buflen=id3len;
  return buf;
}


void LameEncoder::SetMetadata(WDL_StringKeyedArray<char*> *metadata)
{
  int buflen=0;
  unsigned char *buf=PackID3Chunk(metadata, &buflen);
  if (buf && buflen)
  {
    outqueue.Add(buf, buflen);
    m_id3_len=buflen;
  }
}
