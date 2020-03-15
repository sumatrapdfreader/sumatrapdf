/*
  WDL - fileread.h
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
  

  This file provides the WDL_FileRead object, which can be used to read files.
  On windows systems it supports reading synchronous, asynchronous, memory mapped, and asynchronous unbuffered.
  On non-windows systems it acts as a wrapper for fopen()/etc.


*/


#ifndef _WDL_FILEREAD_H_
#define _WDL_FILEREAD_H_




#include "ptrlist.h"



#if defined(_WIN32) && !defined(WDL_NO_WIN32_FILEREAD)
  #ifndef WDL_WIN32_NATIVE_READ
    #define WDL_WIN32_NATIVE_READ
  #endif
#else
  #ifdef WDL_WIN32_NATIVE_READ
    #undef WDL_WIN32_NATIVE_READ
  #endif
  
  #if !defined(WDL_NO_POSIX_FILEREAD)
  #define WDL_POSIX_NATIVE_READ 
   #include <sys/fcntl.h>
   #include <sys/file.h>
   #include <sys/stat.h>
   #include <sys/errno.h>
   #include <sys/mman.h>
   #ifdef __APPLE__
      #include <sys/param.h>
      #include <sys/mount.h>
   #endif
  #endif
  
#endif



#ifdef _MSC_VER
#define WDL_FILEREAD_POSTYPE __int64
#else
#define WDL_FILEREAD_POSTYPE long long
#endif
class WDL_FileRead
{

#ifdef WDL_WIN32_NATIVE_READ


class WDL_FileRead__ReadEnt
{
public:
  WDL_FileRead__ReadEnt(int sz, char *buf)
  {
    m_size=0;
    memset(&m_ol,0,sizeof(m_ol));
    m_ol.hEvent=CreateEvent(NULL,TRUE,TRUE,NULL);
    m_buf=buf;
  }
  ~WDL_FileRead__ReadEnt()
  {
    CloseHandle(m_ol.hEvent);
  }

  OVERLAPPED m_ol;
  DWORD m_size;
  LPVOID m_buf;
};

#endif

#if defined(_WIN32) && !defined(WDL_NO_SUPPORT_UTF8)
  BOOL HasUTF8(const char *_str)
  {
    const unsigned char *str = (const unsigned char *)_str;
    if (!str) return FALSE;
    while (*str) 
    {
      unsigned char c = *str;
      if (c >= 0xC2)
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

public:
  // allow_async=1 for unbuffered async, 2 for buffered async, =-1 for unbuffered sync
  // async aspect is unused on OS X, but the buffered mode affects F_NOCACHE
  WDL_FileRead(const char *filename, int allow_async=1, int bufsize=8192, int nbufs=4, unsigned int mmap_minsize=0, unsigned int mmap_maxsize=0) : m_bufspace(4096 WDL_HEAPBUF_TRACEPARM("WDL_FileRead"))
  {
    m_async_hashaderr=false;
    m_sync_bufmode_used=m_sync_bufmode_pos=0;
    m_async_readpos=m_file_position=0;
    m_fsize=0;
    m_fsize_maychange=false;
    m_syncrd_firstbuf=true;
    m_mmap_view=0;
    m_mmap_totalbufmode=0;

#define WDL_UNBUF_ALIGN 8192
    if (bufsize&(WDL_UNBUF_ALIGN-1)) bufsize=(bufsize&~(WDL_UNBUF_ALIGN-1))+WDL_UNBUF_ALIGN; // ensure bufsize is multiple of 4kb
    
#ifdef WDL_WIN32_NATIVE_READ

    m_mmap_fmap=0;

    #ifdef WDL_SUPPORT_WIN9X
    const bool isNT = GetVersion()<0x80000000;
    #else
    const bool isNT = true;
    #endif
    m_async = isNT ? allow_async : 0;

    int flags=FILE_ATTRIBUTE_NORMAL;
    if (m_async>0)
    {
      flags|=FILE_FLAG_OVERLAPPED;
      if (m_async==1) flags|=FILE_FLAG_NO_BUFFERING;
    }
    else if (nbufs*bufsize>=WDL_UNBUF_ALIGN && !mmap_maxsize && m_async==-1) 
      flags|=FILE_FLAG_NO_BUFFERING; // non-async mode unbuffered if we do our own buffering

#ifndef WDL_NO_SUPPORT_UTF8
    m_fh = INVALID_HANDLE_VALUE;
    if (isNT && HasUTF8(filename)) // only convert to wide if there are UTF-8 chars
    {
      int szreq=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,filename,-1,NULL,0);
      if (szreq > 1000)
      {
        WDL_TypedBuf<WCHAR> wfilename;
        wfilename.Resize(szreq+10);

        if (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,filename,-1,wfilename.Get(),wfilename.GetSize()))
        {
          m_fh = CreateFileW(wfilename.Get(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,flags,NULL);
          if (m_fh == INVALID_HANDLE_VALUE && GetLastError()==ERROR_SHARING_VIOLATION)
          {
            m_fh = CreateFileW(wfilename.Get(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,flags,NULL);
            m_fsize_maychange=true;
          }
        }
      }
      else
      {
        WCHAR wfilename[1024];

        if (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,filename,-1,wfilename,1024))
        {
          m_fh = CreateFileW(wfilename,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,flags,NULL);
          if (m_fh == INVALID_HANDLE_VALUE && GetLastError()==ERROR_SHARING_VIOLATION)
          {
            m_fh = CreateFileW(wfilename,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,flags,NULL);
            m_fsize_maychange=true;
          }
        }
      }
    }
    if (m_fh == INVALID_HANDLE_VALUE)
#endif
    {
      m_fh = CreateFileA(filename,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,flags,NULL);
      if (m_fh == INVALID_HANDLE_VALUE && GetLastError()==ERROR_SHARING_VIOLATION)
      {
        m_fh = CreateFileA(filename,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,flags,NULL);
        m_fsize_maychange=true;
      }
    }

    if (m_fh != INVALID_HANDLE_VALUE)
    {
      DWORD h=0;
      DWORD l=GetFileSize(m_fh,&h);
      m_fsize=(((WDL_FILEREAD_POSTYPE)h)<<32)|l;
      if (m_fsize<0 || (l == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)) m_fsize=0;

      if (!h && l < mmap_maxsize && m_async<=0)
      {
        if (l >= mmap_minsize)
        {
          m_mmap_fmap=CreateFileMapping(m_fh,NULL,PAGE_READONLY,NULL,0,NULL);
          if (m_mmap_fmap) 
          {
            m_mmap_view=MapViewOfFile(m_mmap_fmap,FILE_MAP_READ,0,0,(int)m_fsize);
            if (!m_mmap_view) 
            {
              CloseHandle(m_mmap_fmap);
              m_mmap_fmap=0;
            }
            else m_fsize_maychange=false;
          }
        }
        else if (l>0)
        {
          m_mmap_totalbufmode = malloc(l);
          if (m_mmap_totalbufmode)
          {
            DWORD sz;
            ReadFile(m_fh,m_mmap_totalbufmode,l,&sz,NULL);
          }
          m_fsize_maychange=false;
        }
      }

      if (m_async>0)
      {
        m_async_bufsize=bufsize;
        int x;
        char *bptr=(char *)m_bufspace.Resize(nbufs*bufsize + (WDL_UNBUF_ALIGN-1));
        int a=((int)(INT_PTR)bptr)&(WDL_UNBUF_ALIGN-1);
        if (a) bptr += WDL_UNBUF_ALIGN-a;
        for (x = 0; x < nbufs; x ++)
        {
          WDL_FileRead__ReadEnt *t=new WDL_FileRead__ReadEnt(m_async_bufsize,bptr);
          m_empties.Add(t);
          bptr+=m_async_bufsize;
        }
      }
      else if (!m_mmap_view && !m_mmap_totalbufmode && nbufs*bufsize>=WDL_UNBUF_ALIGN)
      {
        m_bufspace.Resize(nbufs*bufsize+(WDL_UNBUF_ALIGN-1));
      }
    }

#elif defined(WDL_POSIX_NATIVE_READ)
    m_filedes_locked=false;
    m_filedes_rdpos=0;
    m_filedes=open(filename,O_RDONLY 
        // todo: use fcntl() for platforms when O_CLOEXEC is not available (if we ever need to support them)
        // (currently the only platform that meets this criteria is macOS w/ old SDK, but we don't use execve()
        // there
#ifdef O_CLOEXEC
        | O_CLOEXEC
#endif
        );
    if (m_filedes>=0)
    {
      if (flock(m_filedes,LOCK_SH|LOCK_NB)>=0) // get shared lock
        m_filedes_locked=true;
      else
        m_fsize_maychange=true; // if couldnt get shared lock, then it may change

#ifdef __APPLE__
      if (allow_async==1 || allow_async==-1) 
      {
        struct statfs sfs;
        if (fstatfs(m_filedes,&sfs)||(sfs.f_flags&MNT_LOCAL)) // don't use F_NOCACHE on nfs/smb/afp mounts, we need caching there!
          fcntl(m_filedes,F_NOCACHE,1);
      }
#endif
      m_fsize=lseek(m_filedes,0,SEEK_END);
      lseek(m_filedes,0,SEEK_SET);
      if (m_fsize<0) m_fsize=0;

      if (m_fsize < mmap_maxsize)
      {
        if (m_fsize >= mmap_minsize)
        {
          m_mmap_view = mmap(NULL,(size_t)m_fsize,PROT_READ,MAP_SHARED,m_filedes,0);
          if (m_mmap_view == MAP_FAILED) m_mmap_view = 0;
          else m_fsize_maychange=false;
        }
        else
        {
          m_mmap_totalbufmode = malloc((size_t)m_fsize);
          if (m_mmap_totalbufmode)
            m_fsize = pread(m_filedes,m_mmap_totalbufmode,(size_t)m_fsize,0);
          m_fsize_maychange=false;
        }
      }
    }
    if (!m_mmap_view && !m_mmap_totalbufmode && m_filedes>=0 && nbufs*bufsize>=WDL_UNBUF_ALIGN)
      m_bufspace.Resize(nbufs*bufsize+(WDL_UNBUF_ALIGN-1));

#else
    m_fp=fopen(filename,"rb");
    if(m_fp)
    {
      fseek(m_fp,0,SEEK_END);
      m_fsize=ftell(m_fp);
      fseek(m_fp,0,SEEK_SET);
    }
    if (m_fp && nbufs*bufsize>=WDL_UNBUF_ALIGN)
      m_bufspace.Resize(nbufs*bufsize+(WDL_UNBUF_ALIGN-1));
#endif
  }

  ~WDL_FileRead()
  {
    free(m_mmap_totalbufmode);
    m_mmap_totalbufmode=0;

#ifdef WDL_WIN32_NATIVE_READ
    int x;
    for (x = 0; x < m_empties.GetSize();x ++) delete m_empties.Get(x);
    m_empties.Empty();
    for (x = 0; x < m_full.GetSize();x ++) delete m_full.Get(x);
    m_full.Empty();
    for (x = 0; x < m_pending.GetSize();x ++) 
    {
      WaitForSingleObject(m_pending.Get(x)->m_ol.hEvent,INFINITE);
      delete m_pending.Get(x);
    }
    m_pending.Empty();

    if (m_mmap_view) UnmapViewOfFile(m_mmap_view);
    m_mmap_view=0;

    if (m_mmap_fmap) CloseHandle(m_mmap_fmap);
    m_mmap_fmap=0;

    if (m_fh != INVALID_HANDLE_VALUE) CloseHandle(m_fh);
    m_fh=INVALID_HANDLE_VALUE;
#elif defined(WDL_POSIX_NATIVE_READ)
    if (m_mmap_view) munmap(m_mmap_view,(size_t)m_fsize);
    m_mmap_view=0;
    if (m_filedes>=0) 
    {
      if (m_filedes_locked) flock(m_filedes,LOCK_UN); // release shared lock
      close(m_filedes);
    }
    m_filedes=-1;
#else
    if (m_fp) fclose(m_fp);
    m_fp=0;
#endif

  }

  bool IsOpen()
  {
#ifdef WDL_WIN32_NATIVE_READ
    return (m_fh != INVALID_HANDLE_VALUE);
#elif defined(WDL_POSIX_NATIVE_READ)
    return m_filedes >= 0;
#else
    return m_fp != NULL;
#endif
  }

  void CloseHandlesIfFullyInMemory()
  {
    if (!m_mmap_totalbufmode) return;
#ifdef WDL_WIN32_NATIVE_READ
    if (m_fh != INVALID_HANDLE_VALUE) 
    {
      CloseHandle(m_fh);
      m_fh=INVALID_HANDLE_VALUE;
    }
#elif defined(WDL_POSIX_NATIVE_READ)
    if (m_filedes>=0) 
    {
      if (m_filedes_locked) flock(m_filedes,LOCK_UN); // release shared lock
      close(m_filedes);
      m_filedes=-1;
    }
#else
    if (m_fp) 
    {
      fclose(m_fp);
      m_fp=NULL;
    }
#endif
  }

#ifdef WDL_WIN32_NATIVE_READ

  int RunReads()
  {
    while (m_pending.GetSize())
    {
      WDL_FileRead__ReadEnt *ent=m_pending.Get(0);
      DWORD s=0;

      if (!ent->m_size && !GetOverlappedResult(m_fh,&ent->m_ol,&s,FALSE)) break;
      m_pending.Delete(0);
      if (!ent->m_size) ent->m_size=s;
      m_full.Add(ent);
    }


    if (m_empties.GetSize()>0)
    {
      if (m_async_readpos < m_file_position)  m_async_readpos = m_file_position;

      if (m_async==1) m_async_readpos &= ~((WDL_FILEREAD_POSTYPE) WDL_UNBUF_ALIGN-1);

      if (m_async_readpos >= m_fsize) return 0;

      const int rdidx=m_empties.GetSize()-1;
      WDL_FileRead__ReadEnt *t=m_empties.Get(rdidx);

      ResetEvent(t->m_ol.hEvent);

      *(WDL_FILEREAD_POSTYPE *)&t->m_ol.Offset = m_async_readpos;

      m_async_readpos += m_async_bufsize;
      DWORD dw;
      if (ReadFile(m_fh,t->m_buf,m_async_bufsize,&dw,&t->m_ol))
      {
        if (!dw) return 1;
      }
      else
      {
        if (GetLastError() != ERROR_IO_PENDING) return 1;
        dw=0;
      }
      t->m_size=dw;
      m_empties.Delete(rdidx);
      m_pending.Add(t);
    }
    return 0;
  }

  int AsyncRead(char *buf, int maxlen)
  {
    char *obuf=buf;
    int lenout=0;
    if (m_file_position+maxlen > m_fsize)
    {
      maxlen=(int) (m_fsize-m_file_position);
    }
    if (maxlen<1) return 0;

    int errcnt=!!m_async_hashaderr;
    do
    {
      while (m_full.GetSize() > 0)
      {
        WDL_FileRead__ReadEnt *ti=m_full.Get(0);
        WDL_FILEREAD_POSTYPE tiofs=*(WDL_FILEREAD_POSTYPE *)&ti->m_ol.Offset;
        if (m_file_position >= tiofs && m_file_position < tiofs + ti->m_size)
        {
          if (maxlen < 1) break;

          int l=ti->m_size-(int) (m_file_position-tiofs);
          if (l > maxlen) l=maxlen;

          memcpy(buf,(char *)ti->m_buf+m_file_position - tiofs,l);
          buf += l;
          m_file_position += l;
          maxlen -= l;
          lenout += l;
        }
        else
        {
          m_empties.Add(ti);
          m_full.Delete(0);
        }  
      }
      
      if (maxlen > 0 && m_async_readpos != m_file_position)
      {
        int x;
        for (x = 0; x < m_pending.GetSize(); x ++)
        {
          WDL_FileRead__ReadEnt *ent=m_pending.Get(x);
          WDL_FILEREAD_POSTYPE tiofs=*(WDL_FILEREAD_POSTYPE *)&ent->m_ol.Offset;
          if (m_file_position >= tiofs && m_file_position < tiofs + m_async_bufsize) break;
        }
        if (x == m_pending.GetSize())
        {
          m_async_readpos=m_file_position;
        }
      }

      errcnt+=RunReads();
      
      if (maxlen > 0 && m_pending.GetSize() && !m_full.GetSize())
      {
        WDL_FileRead__ReadEnt *ent=m_pending.Get(0);
        m_pending.Delete(0);

        if (ent->m_size) m_full.Add(ent);
        else
        {
//          WaitForSingleObject(ent->m_ol.hEvent,INFINITE);

          DWORD s=0;
          if (GetOverlappedResult(m_fh,&ent->m_ol,&s,TRUE) && s)
          {
            ent->m_size=s;
            m_full.Add(ent);
          }
          else // failed read, set the error flag
          {
            errcnt++;
            ent->m_size=0;
            m_empties.Add(ent);
          }
        }
      }
    }
    while (maxlen > 0 && (m_pending.GetSize()||m_full.GetSize()) && !errcnt);
    if (!errcnt) RunReads();
    else m_async_hashaderr=true;

    return lenout;
  }

#endif

  void *GetMappedView(int offs, int *len)
  {
    if (!m_mmap_view && !m_mmap_totalbufmode) return 0;

    int maxl=(int) (m_fsize-(WDL_FILEREAD_POSTYPE)offs);
    if (*len > maxl) *len=maxl;
    if (m_mmap_view)
      return (char *)m_mmap_view + offs;
    else
      return (char *)m_mmap_totalbufmode + offs;
  }

  int Read(void *buf, int len)
  {
    if (m_mmap_view||m_mmap_totalbufmode)
    {
      int maxl=(int) (m_fsize-m_file_position);
      if (maxl > len) maxl=len;
      if (maxl < 0) maxl=0;
      if (maxl>0)
      {
        if (m_mmap_view)
          memcpy(buf,(char *)m_mmap_view + (int)m_file_position,maxl);
        else
          memcpy(buf,(char *)m_mmap_totalbufmode + (int)m_file_position,maxl);
          
      }
      m_file_position+=maxl;
      return maxl;     
    }

    if (m_fsize_maychange) GetSize(); // update m_fsize

#ifdef WDL_WIN32_NATIVE_READ
    if (m_fh == INVALID_HANDLE_VALUE||len<1) return 0;

    if (m_async>0)
    {
      return AsyncRead((char *)buf,len);
    }
#elif defined(WDL_POSIX_NATIVE_READ)
    if (m_filedes<0 || len<1) return 0;

#else
    if (!m_fp || len<1) return 0;

#endif

    if (m_bufspace.GetSize()>=WDL_UNBUF_ALIGN*2-1)
    {
      int rdout=0;
      int sz=m_bufspace.GetSize()-(WDL_UNBUF_ALIGN-1);
      char *srcbuf=(char *)m_bufspace.Get(); // read size
      if (((int)(INT_PTR)srcbuf)&(WDL_UNBUF_ALIGN-1)) srcbuf += WDL_UNBUF_ALIGN-(((int)(INT_PTR)srcbuf)&(WDL_UNBUF_ALIGN-1));
      while (len > rdout)
      {
        int a=m_sync_bufmode_used-m_sync_bufmode_pos;
        if (a>(len-rdout)) a=(len-rdout);
        if (a>0)
        {
          memcpy((char*)buf+rdout,srcbuf+m_sync_bufmode_pos,a);
          rdout+=a;
          m_sync_bufmode_pos+=a;
          m_file_position+=a;
        }

        if (len > rdout)
        {
          m_sync_bufmode_used=0;
          m_sync_bufmode_pos=0;
          
          int thissz=sz;
          if (m_syncrd_firstbuf) // this is a scheduling mechanism to avoid having reads on various files always happening at the same time -- not needed in async modes, only in sync with large buffers
          {
            m_syncrd_firstbuf=false;
            const int blocks = thissz/WDL_UNBUF_ALIGN;
            if (blocks > 1)
            {
              static int rrs; // may not be ideal on multithread, but having it incorrect isnt a big deal.
              if (blocks>7) thissz >>= (rrs++)&3;
              else thissz>>= (rrs++)&1;
            }
          }
          
          #ifdef WDL_WIN32_NATIVE_READ
            DWORD o;
            if (m_async==-1)
            {
              if (m_file_position&(WDL_UNBUF_ALIGN-1))
              {
                int offs = (int)(m_file_position&(WDL_UNBUF_ALIGN-1));
                LONG high=(LONG) ((m_file_position-offs)>>32);
                SetFilePointer(m_fh,(LONG)((m_file_position-offs)&((WDL_FILEREAD_POSTYPE)0xFFFFFFFF)),&high,FILE_BEGIN);
                m_sync_bufmode_pos=offs;
              }
            }
            if (!ReadFile(m_fh,srcbuf,thissz,&o,NULL) || o<1 || m_sync_bufmode_pos>=(int)o) 
            {
              break;
            }
          #elif defined(WDL_POSIX_NATIVE_READ)
            int o=(int)pread(m_filedes,srcbuf,thissz,m_filedes_rdpos);
            if (o>0) m_filedes_rdpos+=o;
            if (o<1 || m_sync_bufmode_pos>=o) break;                    
          
          #else
            int o=(int)fread(srcbuf,1,thissz,m_fp);
            if (o<1 || m_sync_bufmode_pos>=o) break;                    
          #endif
          m_sync_bufmode_used=o;
        }

      }
      return rdout;
    }
    else
    {
    #ifdef WDL_WIN32_NATIVE_READ
      DWORD dw=0;
      ReadFile(m_fh,buf,len,&dw,NULL);
      m_file_position+=dw;
      return dw;
    #elif defined(WDL_POSIX_NATIVE_READ)
    
      int ret=(int)pread(m_filedes,buf,len,m_filedes_rdpos);
      if (ret>0) m_filedes_rdpos+=ret;
      m_file_position+=ret;
      return ret;
    #else
      int ret=fread(buf,1,len,m_fp);
      m_file_position+=ret;
      return ret;
    #endif      
    }
    
  }

  WDL_FILEREAD_POSTYPE GetSize()
  {
    if (m_mmap_totalbufmode) return m_fsize;
#ifdef WDL_WIN32_NATIVE_READ
    if (m_fh == INVALID_HANDLE_VALUE) return 0;
#elif defined(WDL_POSIX_NATIVE_READ)
    if (m_filedes<0) return -1;

#else
    if (!m_fp) return -1;
#endif

    if (m_fsize_maychange)
    {
#ifdef WDL_WIN32_NATIVE_READ
      DWORD h=0;
      DWORD l=GetFileSize(m_fh,&h);
      m_fsize=(((WDL_FILEREAD_POSTYPE)h)<<32)|l;
#elif defined(WDL_POSIX_NATIVE_READ)
      struct stat64 st;
      if (!fstat64(m_filedes,&st))  m_fsize = st.st_size;

#endif
    }

    return m_fsize;
  }

  WDL_FILEREAD_POSTYPE GetPosition()
  {
    if (m_mmap_totalbufmode) return m_file_position;
#ifdef WDL_WIN32_NATIVE_READ
    if (m_fh == INVALID_HANDLE_VALUE) return -1;
#elif defined(WDL_POSIX_NATIVE_READ)
    if (m_filedes<0) return -1;
#else
    if (!m_fp) return -1;
#endif
    return m_file_position;
  }

  bool SetPosition(WDL_FILEREAD_POSTYPE pos) // returns 0 on success
  {
    m_async_hashaderr=false;

    if (!m_mmap_totalbufmode)
    {
      #ifdef WDL_WIN32_NATIVE_READ
        if (m_fh == INVALID_HANDLE_VALUE) return true;
      #elif defined(WDL_POSIX_NATIVE_READ)
        if (m_filedes<0) return true;
      #else
        if (!m_fp) return true;
      #endif
    }

    if (m_fsize_maychange) GetSize();

    if (pos < 0) pos=0;
    if (pos > m_fsize) pos=m_fsize;
    WDL_FILEREAD_POSTYPE oldpos=m_file_position;
    if (m_file_position!=pos) m_file_position=pos;
    else return false;

    if (m_mmap_view||m_mmap_totalbufmode) return false;
    
#ifdef WDL_WIN32_NATIVE_READ
    if (m_async>0)
    {
      WDL_FileRead__ReadEnt *ent;

      if (pos > m_async_readpos || !(ent=m_full.Get(0)) || pos < *(WDL_FILEREAD_POSTYPE *)&ent->m_ol.Offset)
      {
        m_async_readpos=pos;
      }

      return FALSE;
    }
#endif


    if (m_bufspace.GetSize()>=WDL_UNBUF_ALIGN*2-1)
    {
      if (pos >= oldpos-m_sync_bufmode_pos && pos < oldpos-m_sync_bufmode_pos + m_sync_bufmode_used)
      {
        int diff=(int) (pos-oldpos);
        m_sync_bufmode_pos+=diff;

        return 0;
      }
      m_sync_bufmode_pos=m_sync_bufmode_used=0;
    }

    m_syncrd_firstbuf=true;
#ifdef WDL_WIN32_NATIVE_READ
    LONG high=(LONG) (m_file_position>>32);
    return SetFilePointer(m_fh,(LONG)(m_file_position&((WDL_FILEREAD_POSTYPE)0xFFFFFFFF)),&high,FILE_BEGIN)==0xFFFFFFFF && GetLastError() != NO_ERROR;
#elif defined(WDL_POSIX_NATIVE_READ)
    m_filedes_rdpos = m_file_position;
    return false;
#else
    return !!fseek(m_fp,m_file_position,SEEK_SET);
#endif
  }
  
  WDL_HeapBuf m_bufspace;
  int m_sync_bufmode_used, m_sync_bufmode_pos;

  WDL_FILEREAD_POSTYPE m_file_position,m_async_readpos;
  WDL_FILEREAD_POSTYPE m_fsize;
  
  void *m_mmap_view;
  void *m_mmap_totalbufmode;

#ifdef WDL_WIN32_NATIVE_READ
  HANDLE GetHandle() { return m_fh; }
  HANDLE m_fh;
  HANDLE m_mmap_fmap;
  int m_mmap_size;
  int m_async; // 1=nobuf, 2=buffered async, -1=unbuffered sync

  int m_async_bufsize;
  WDL_PtrList<WDL_FileRead__ReadEnt> m_empties;
  WDL_PtrList<WDL_FileRead__ReadEnt> m_pending;
  WDL_PtrList<WDL_FileRead__ReadEnt> m_full;
  
#elif defined(WDL_POSIX_NATIVE_READ)
  WDL_FILEREAD_POSTYPE m_filedes_rdpos;
  int m_filedes;
  bool m_filedes_locked;

  int GetHandle() { return m_filedes; }
#else
  FILE *m_fp;
  
  int GetHandle() { return fileno(m_fp); }
#endif

  bool m_fsize_maychange;
  bool m_syncrd_firstbuf;
  bool m_async_hashaderr;

} WDL_FIXALIGN;






#endif
