/* Cockos SWELL (Simple/Small Win32 Emulation Layer for Linux/OSX)
   Copyright (C) 2006 and later, Cockos, Inc.

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

  This file implements basic win32 GetPrivateProfileString / etc support.
  It works by caching reads, but writing through on every write that is required (to ensure 
  that updates take, especially when being updated from multiple modules who have their own 
  cache of the .ini file).

  It is threadsafe, but in theory if two processes are trying to access the same ini, 
  results may go a bit unpredictable (but in general the file should NOT get corrupted,
  we hope).

*/

#ifndef SWELL_PROVIDED_BY_APP


#include "swell.h"
#include "../assocarray.h"
#include "../wdlcstring.h"
#include "../mutex.h"
#include "../queue.h"
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>

static void deleteStringKeyedArray(WDL_StringKeyedArray<char *> *p) {   delete p; }

struct iniFileContext
{
  iniFileContext() : m_sections(false,deleteStringKeyedArray) 
  { 
    m_curfn=NULL;
    m_lastaccesscnt=0;
    m_curfn_time=0;
    m_curfn_sz=0;
  }
  ~iniFileContext() { }
  
  WDL_UINT64 m_lastaccesscnt;
  time_t m_curfn_time;
  int m_curfn_sz;
  char *m_curfn;

  WDL_StringKeyedArray< WDL_StringKeyedArray<char *> * > m_sections;

};

#define NUM_OPEN_CONTEXTS 32
static iniFileContext s_ctxs[NUM_OPEN_CONTEXTS];
static WDL_Mutex m_mutex;

static time_t getfileupdtimesize(const char *fn, int *szOut)
{
  struct stat st;
  *szOut = 0;
  if (!fn || !fn[0] || stat(fn,&st)) return 0;
  *szOut = (int)st.st_size;
  return st.st_mtime;
}

static bool fgets_to_typedbuf(WDL_TypedBuf<char> *buf, FILE *fp) 
{
  int rdpos=0;
  while (rdpos < 1024*1024*32)
  {
    if (buf->GetSize()<rdpos+8192) buf->Resize(rdpos+8192);
    if (buf->GetSize()<rdpos+4) break; // malloc fail, erg
    char *p = buf->Get()+rdpos;
    *p=0;
    if (!fgets(p,buf->GetSize()-rdpos,fp) || !*p) break;
    while (*p) p++;
    if (p[-1] == '\r' || p[-1] == '\n') break;

    rdpos = (int) (p - buf->Get());
  }
  return buf->GetSize()>0 && buf->Get()[0];
}

// return true on success
static iniFileContext *GetFileContext(const char *name)
{
  static WDL_UINT64 acc_cnt;
  int best_z = 0;
  char fntemp[512];
  if (!name || !strstr(name,"/"))
  {
    extern char *g_swell_defini;
    if (g_swell_defini)
    {
      lstrcpyn_safe(fntemp,g_swell_defini,sizeof(fntemp));
    }
    else
    {
      const char *p = getenv("HOME");
      snprintf(fntemp,sizeof(fntemp),"%s/.libSwell.ini",
        p && *p ? p : "/tmp");
    }
    if (name && *name)
    {
      WDL_remove_fileext(fntemp);
      snprintf_append(fntemp,sizeof(fntemp),"_%s%s",name,
        stricmp(WDL_get_fileext(name),".ini")?".ini":"");
    }
    name = fntemp;
  }

  {
    int w;
    WDL_UINT64 bestcnt = 0; 
    bestcnt--;

    for (w=0;w<NUM_OPEN_CONTEXTS;w++)
    {
      if (!s_ctxs[w].m_curfn || !stricmp(s_ctxs[w].m_curfn,name)) 
      {
        // we never clear m_curfn, so we'll always find an item in cache before an unused cache entry
        best_z=w;
        break;
      }

      if (s_ctxs[w].m_lastaccesscnt < bestcnt) { best_z = w; bestcnt = s_ctxs[w].m_lastaccesscnt; }
    }
  }
    
  iniFileContext *ctx = &s_ctxs[best_z];
  ctx->m_lastaccesscnt=++acc_cnt;
  
  int sz=0;
  if (!ctx->m_curfn || stricmp(ctx->m_curfn,name) || ctx->m_curfn_time != getfileupdtimesize(ctx->m_curfn,&sz) || sz != ctx->m_curfn_sz)
  {
    ctx->m_sections.DeleteAll();
//    printf("reinitting to %s\n",name);
    if (!ctx->m_curfn || stricmp(ctx->m_curfn,name))
    {
      free(ctx->m_curfn);
      ctx->m_curfn=strdup(name);
    }
    FILE *fp = WDL_fopenA(name,"r");
    
    if (!fp)
    {
      ctx->m_curfn_time=0;
      ctx->m_curfn_sz=0;
      return ctx; // allow to proceed (empty file)
    }

    flock(fileno(fp),LOCK_SH);
    
    // parse .ini file
    WDL_StringKeyedArray<char *> *cursec=NULL;

    int lcnt=0;
    for (;;)
    {
      static WDL_TypedBuf<char> _buf;
      if (!fgets_to_typedbuf(&_buf,fp)) break;

      char *buf = _buf.Get();
      if (!ctx->m_sections.GetSize()) 
      {
        lcnt += strlen(buf);
        if (lcnt > 256*1024) break; // dont bother reading more than 256kb if no section encountered
      }
      char *p=buf;

      while (*p) p++;

      if (p>buf)
      {
        p--;
        while (p >= buf && (*p==' ' || *p == '\r' || *p == '\n' || *p == '\t')) p--;
        p[1]=0;
      }
      p=buf;
      while (*p == ' ' || *p == '\t') p++;
      if (p[0] == '[')
      {
        char *p2=p;
        while (*p2 && *p2 != ']') p2++;
        if (*p2)
        {
          *p2=0;
          if (cursec) cursec->Resort();
          
          if (p[1])
          {
            cursec = ctx->m_sections.Get(p+1);
            if (!cursec)
            {
              cursec = new WDL_StringKeyedArray<char *>(false,WDL_StringKeyedArray<char *>::freecharptr);
              ctx->m_sections.Insert(p+1,cursec);
            }
            else cursec->DeleteAll();
          }
          else cursec=0;
        }
      }
      else if (cursec)
      {
        char *t=strstr(p,"=");
        if (t)
        {
          *t++=0;
          if (*p) 
            cursec->AddUnsorted(p,strdup(t));
        }
      }
    }
    ctx->m_curfn_time = getfileupdtimesize(name,&ctx->m_curfn_sz);
    flock(fileno(fp),LOCK_UN);    
    fclose(fp);

    if (cursec) cursec->Resort();
  }
  return ctx;
}

static void WriteBackFile(iniFileContext *ctx)
{
  if (!ctx||!ctx->m_curfn) return;
  char newfn[1024];
  lstrcpyn_safe(newfn,ctx->m_curfn,sizeof(newfn)-8);
  {
    char *p=newfn;
    while (*p) p++;
    while (p>newfn && p[-1] != '/') p--;
    char lc = '.';
    while (*p)
    {
      char c = *p;
      *p++ = lc;
      lc = c;
    }
    *p++ = lc;
    strcpy(p,".new");
  }

  FILE *fp = WDL_fopenA(newfn,"w");
  if (!fp) return;
  
  flock(fileno(fp),LOCK_EX);
  
  int x;
  for (x = 0; ; x ++)
  {
    const char *secname=NULL;
    WDL_StringKeyedArray<char *> * cursec = ctx->m_sections.Enumerate(x,&secname);
    if (!cursec || !secname) break;
    
    fprintf(fp,"[%s]\n",secname);
    int y;
    for (y=0;;y++)
    {
      const char *keyname = NULL;
      const char *keyvalue = cursec->Enumerate(y,&keyname);
      if (!keyvalue || !keyname) break;
      if (*keyname) fprintf(fp,"%s=%s\n",keyname,keyvalue);
    }
    fprintf(fp,"\n");
  }  
  
  fflush(fp);
  flock(fileno(fp),LOCK_UN);
  fclose(fp);

  if (!rename(newfn,ctx->m_curfn))
  {
    ctx->m_curfn_time = getfileupdtimesize(ctx->m_curfn,&ctx->m_curfn_sz);
  }
  else
  {
    // error updating, hmm how to handle this?
  }
}

BOOL WritePrivateProfileSection(const char *appname, const char *strings, const char *fn)
{
  if (!appname) return FALSE;
  WDL_MutexLock lock(&m_mutex);
  iniFileContext *ctx = GetFileContext(fn);
  if (!ctx) return FALSE;

  WDL_StringKeyedArray<char *> * cursec = ctx->m_sections.Get(appname);
  if (!cursec)
  {
    if (!*strings) return TRUE;
    
    cursec = new WDL_StringKeyedArray<char *>(false,WDL_StringKeyedArray<char *>::freecharptr);   
    ctx->m_sections.Insert(appname,cursec);
  }
  else cursec->DeleteAll();
  
  if (*strings)
  {
    while (*strings)
    {
      char buf[8192];
      lstrcpyn_safe(buf,strings,sizeof(buf));
      char *p = buf;
      while (*p && *p != '=') p++;
      if (*p)
      {
        *p++=0;
        cursec->Insert(buf,strdup(strings + (p-buf)));
      }
      
      strings += strlen(strings)+1;
    }
  }
  WriteBackFile(ctx);
  
  return TRUE;
}


BOOL WritePrivateProfileString(const char *appname, const char *keyname, const char *val, const char *fn)
{
  if (!appname || (keyname && !*keyname)) return FALSE;
//  printf("writing %s %s %s %s\n",appname,keyname,val,fn);
  WDL_MutexLock lock(&m_mutex);
  
  iniFileContext *ctx = GetFileContext(fn);
  if (!ctx) return FALSE;
    
  if (!keyname)
  {
    if (ctx->m_sections.Get(appname))
    {
      ctx->m_sections.Delete(appname);
      WriteBackFile(ctx);
    }
  }
  else 
  {
    WDL_StringKeyedArray<char *> * cursec = ctx->m_sections.Get(appname);
    if (!val)
    {
      if (cursec && cursec->Get(keyname))
      {
        cursec->Delete(keyname);
        WriteBackFile(ctx);
      }
    }
    else
    {
      const char *p;
      if (!cursec || !(p=cursec->Get(keyname)) || strcmp(p,val))
      {
        if (!cursec) 
        {
          cursec = new WDL_StringKeyedArray<char *>(false,WDL_StringKeyedArray<char *>::freecharptr);   
          ctx->m_sections.Insert(appname,cursec);
        }
        cursec->Insert(keyname,strdup(val));
        WriteBackFile(ctx);
      }
    }

  }

  return TRUE;
}

static void lstrcpyn_trimmed(char* dest, const char* src, int len)
{
  if (len<1) return;
  // Mimic Win32 behavior of stripping quotes and whitespace
  while (*src==' ' || *src=='\t') ++src; // Strip beginning whitespace

  const char *end = src;
  if (*end) while (end[1]) end++;

  while (end >= src && (*end==' ' || *end=='\t')) --end; // Strip end whitespace

  if (end > src && ((*src=='\"' && *end=='\"') || (*src=='\'' && *end=='\'')))
  {	
    // Strip initial set of "" or ''
    ++src; 
    --end;
  }

  int newlen = (int) (end-src+2);
  if (newlen < 1) newlen = 1;
  else if (newlen > len) newlen = len;

  lstrcpyn_safe(dest, src, newlen);
}	

DWORD GetPrivateProfileSection(const char *appname, char *strout, DWORD strout_len, const char *fn)
{
  WDL_MutexLock lock(&m_mutex);
  
  if (!strout || strout_len<2) 
  {
    if (strout && strout_len==1) *strout=0;
    return 0;
  }
  iniFileContext *ctx= GetFileContext(fn);
  int szOut=0;
  WDL_StringKeyedArray<char *> *cursec = ctx ? ctx->m_sections.Get(appname) : NULL;

  if (ctx && cursec) 
  {
    int x;
    for(x=0;x<cursec->GetSize();x++)
    {
      const char *kv = NULL;
      const char *val = cursec->Enumerate(x,&kv);
      if (val && kv)
      {        
        int l;
       
#define WRSTR(v) \
        l = (int)strlen(v); \
        if (l > (int)strout_len - szOut - 2) l = (int)strout_len - 2 - szOut; \
        if (l>0) { memcpy(strout+szOut,v,l); szOut+=l; }
        
        WRSTR(kv)
        WRSTR("=")
#undef WRSTR

        lstrcpyn_trimmed(strout+szOut, val, (int)strout_len - szOut - 2);
        szOut += strlen(strout+szOut);

        l=1;
        if (l > (int)strout_len - szOut - 1) l = (int)strout_len - 1 - szOut;
        if (l>0) { memset(strout+szOut,0,l); szOut+=l; }
        if (szOut >= (int)strout_len-1)
        {
          strout[strout_len-1]=0;
          return strout_len-2;
        }
      }
    }
  }
  strout[szOut]=0;
  if (!szOut) strout[1]=0;
  return szOut;
}

DWORD GetPrivateProfileString(const char *appname, const char *keyname, const char *def, char *ret, int retsize, const char *fn)
{
  WDL_MutexLock lock(&m_mutex);
  
//  printf("getprivateprofilestring: %s\n",fn);
  iniFileContext *ctx= GetFileContext(fn);
  
  if (ctx)
  {
    if (!appname||!keyname)
    {
      WDL_Queue tmpbuf;
      if (!appname)
      {
        int x;
        for (x = 0;; x ++)
        {
          const char *secname=NULL;
          if (!ctx->m_sections.Enumerate(x,&secname) || !secname) break;
          if (*secname) tmpbuf.Add(secname,(int)strlen(secname)+1);
        }
      }
      else
      {
        WDL_StringKeyedArray<char *> *cursec = ctx->m_sections.Get(appname);
        if (cursec)
        {
          int y;
          for (y = 0; ; y ++)
          {            
            const char *k=NULL;
            if (!cursec->Enumerate(y,&k)||!k) break;
            if (*k) tmpbuf.Add(k,(int)strlen(k)+1);
          }
        }
      }
      
      int sz=tmpbuf.GetSize()-1;
      if (sz<0)
      {
        ret[0]=ret[1]=0;
        return 0;
      }
      if (sz > retsize-2) sz=retsize-2;
      memcpy(ret,tmpbuf.Get(),sz);
      ret[sz]=ret[sz+1]=0;
        
      return (DWORD)sz;
    }
    
    WDL_StringKeyedArray<char *> *cursec = ctx->m_sections.Get(appname);
    if (cursec)
    {
      const char *val = cursec->Get(keyname);
      if (val)
      {
        lstrcpyn_trimmed(ret,val,retsize);
        return (DWORD)strlen(ret);
      }
    }
  }
//  printf("def %s %s %s %s\n",appname,keyname,def,fn);
  lstrcpyn_safe(ret,def?def:"",retsize);
  return (DWORD)strlen(ret);
}

int GetPrivateProfileInt(const char *appname, const char *keyname, int def, const char *fn)
{
  char buf[512];
  GetPrivateProfileString(appname,keyname,"",buf,sizeof(buf),fn);
  if (buf[0])
  {
    int a=atoi(buf);
    if (a||buf[0]=='0') return a;
  }
  return def;
}

static bool __readbyte(char *src, unsigned char *out)
{
  unsigned char cv=0;
  int s=4;
  while(s>=0)
  {
    if (*src >= '0' && *src <= '9') cv += (*src-'0')<<s;
    else if (*src >= 'a' && *src <= 'f') cv += (*src-'a' + 10)<<s;
    else if (*src >= 'A' && *src <= 'F') cv += (*src-'A' + 10)<<s;
    else return false;
    src++;
    s-=4;
  }
  
  *out=cv;
  return true;
}

BOOL GetPrivateProfileStruct(const char *appname, const char *keyname, void *buf, int bufsz, const char *fn)
{
  if (!appname || !keyname || bufsz<0) return 0;
  char *tmp=(char *)malloc((bufsz+1)*2+16); 
  if (!tmp) return 0;

  BOOL ret=0;
  GetPrivateProfileString(appname,keyname,"",tmp,(bufsz+1)*2+15,fn);
  if (strlen(tmp) == (size_t) (bufsz+1)*2)
  {
    unsigned char sum=0;
    unsigned char *bufout=(unsigned char *)buf;
    char *src=tmp;
    unsigned char cv;
    while (bufsz-->0)
    {
      if (!__readbyte(src,&cv)) break;
      *bufout++ = cv;
      sum += cv;
      src+=2;
    }
    ret = bufsz<0 && __readbyte(src,&cv) && cv==sum;
  }
  free(tmp);
  //printf("getprivateprofilestruct returning %d\n",ret);
  return ret;
}

BOOL WritePrivateProfileStruct(const char *appname, const char *keyname, const void *buf, int bufsz, const char *fn)
{
  if (!keyname || !buf) return WritePrivateProfileString(appname,keyname,(const char *)buf,fn);
  char *tmp=(char *)malloc((bufsz+1)*2+1);
  if (!tmp) return 0;
  char *p = tmp;
  unsigned char sum=0;
  unsigned char *src=(unsigned char *)buf;
  while (bufsz-- > 0)
  {
    sprintf(p,"%02X",*src);
    sum+=*src++;
    p+=2;
  }
  sprintf(p,"%02X",sum);

  BOOL ret=WritePrivateProfileString(appname,keyname,tmp,fn);
  free(tmp);
  return ret;
}

#endif
