/*
  WDL - filewrite.h
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
  

  This file provides the WDL_FileWrite object, which can be used to create/write files.
  On windows systems it supports writing synchronously, asynchronously, and asynchronously without buffering.
  On windows systems it supports files larger than 4gb.
  On non-windows systems it acts as a wrapper for fopen()/etc.


*/


#ifndef _WDL_FILEWRITE_H_
#define _WDL_FILEWRITE_H_




#include "ptrlist.h"



#if defined(_WIN32) && !defined(WDL_NO_WIN32_FILEWRITE)
  #ifndef WDL_WIN32_NATIVE_WRITE
    #define WDL_WIN32_NATIVE_WRITE
  #endif
#else
  #ifdef WDL_WIN32_NATIVE_WRITE
    #undef WDL_WIN32_NATIVE_WRITE
  #endif
  #if !defined(WDL_NO_POSIX_FILEWRITE)
    #include <sys/fcntl.h>
    #include <sys/file.h>
    #include <sys/stat.h>
    #include <sys/errno.h>
    #define WDL_POSIX_NATIVE_WRITE
  #endif
#endif



#ifdef _MSC_VER
#define WDL_FILEWRITE_POSTYPE __int64
#else
#define WDL_FILEWRITE_POSTYPE long long
#endif

//#define WIN32_ASYNC_NOBUF_WRITE // this doesnt seem to give much perf increase (writethrough with buffering is fine, since ultimately writes get deferred anyway)

class WDL_FileWrite
{
#ifdef WDL_WIN32_NATIVE_WRITE

class WDL_FileWrite__WriteEnt
{
public:
  WDL_FileWrite__WriteEnt(int sz)
  {
    m_last_writepos=0;
    m_bufused=0;
    m_bufsz=sz;
    m_bufptr = (char *)__buf.Resize(sz+4095);
    int a=((int)(INT_PTR)m_bufptr)&4095;
    if (a) m_bufptr += 4096-a;

    memset(&m_ol,0,sizeof(m_ol));
    m_ol.hEvent=CreateEvent(NULL,TRUE,TRUE,NULL);
  }
  ~WDL_FileWrite__WriteEnt()
  {
    CloseHandle(m_ol.hEvent);
  }

  WDL_FILEWRITE_POSTYPE m_last_writepos;

  int m_bufused,m_bufsz;
  OVERLAPPED m_ol;
  char *m_bufptr;
  WDL_TypedBuf<char> __buf;
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
  WDL_FileWrite(const char *filename, int allow_async=1, int bufsize=8192, int minbufs=16, int maxbufs=16, bool wantAppendTo=false, bool noFileLocking=false) // async==2 is unbuffered
  {
    m_file_position=0;
    m_file_max_position=0;
    if(!filename)
    {
#ifdef WDL_WIN32_NATIVE_WRITE
      m_fh = INVALID_HANDLE_VALUE;
      m_async = 0;
#elif defined(WDL_POSIX_NATIVE_WRITE)
      m_filedes_locked=false;
      m_filedes=-1;
      m_bufspace_used=0;
#else
      m_fp = NULL;
#endif
      return;
    }

#ifdef WDL_WIN32_NATIVE_WRITE
    #ifdef WDL_SUPPORT_WIN9X
    const bool isNT = (GetVersion()<0x80000000);
    #else
    const bool isNT = true;
    #endif
    m_async = allow_async && isNT;
#ifdef WIN32_ASYNC_NOBUF_WRITE
    bufsize = (bufsize+4095)&~4095;
    if (bufsize<4096) bufsize=4096;
#endif

    int rwflag = GENERIC_WRITE;
    int createFlag= wantAppendTo?OPEN_ALWAYS:CREATE_ALWAYS;
    int shareFlag = noFileLocking ? (FILE_SHARE_READ|FILE_SHARE_WRITE) : FILE_SHARE_READ;
    int flag = FILE_ATTRIBUTE_NORMAL;

    if (m_async)
    {
      rwflag |= GENERIC_READ;
#ifdef WIN32_ASYNC_NOBUF_WRITE
      flag |= FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING|FILE_FLAG_WRITE_THROUGH;
#else
      flag |= FILE_FLAG_OVERLAPPED|(allow_async>1 ? FILE_FLAG_WRITE_THROUGH: 0);
#endif
    }

    {
#ifndef WDL_NO_SUPPORT_UTF8
      m_fh=INVALID_HANDLE_VALUE;
      if (isNT && HasUTF8(filename))
      {
        int szreq=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,filename,-1,NULL,0);
        if (szreq > 1000)
        {
          WDL_TypedBuf<WCHAR> wfilename;
          wfilename.Resize(szreq+10);
          if (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,filename,-1,wfilename.Get(),wfilename.GetSize()))
            m_fh = CreateFileW(wfilename.Get(),rwflag,shareFlag,NULL,createFlag,flag,NULL);
        }
        else
        {
          WCHAR wfilename[1024];
          if (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,filename,-1,wfilename,1024))
            m_fh = CreateFileW(wfilename,rwflag,shareFlag,NULL,createFlag,flag,NULL);
        }
      }
      
      if (m_fh == INVALID_HANDLE_VALUE)
#endif
        m_fh = CreateFileA(filename,rwflag,shareFlag,NULL,createFlag,flag,NULL);
    }

    if (m_async && m_fh != INVALID_HANDLE_VALUE)
    {
      m_async_bufsize=bufsize;
      m_async_maxbufs=maxbufs;
      m_async_minbufs=minbufs;
      int x;
      for (x = 0; x < m_async_minbufs; x ++)
      {
        WDL_FileWrite__WriteEnt *t=new WDL_FileWrite__WriteEnt(m_async_bufsize);
        m_empties.Add(t);
      }
    }

    if (m_fh != INVALID_HANDLE_VALUE && wantAppendTo)
      SetPosition(GetSize());
 
#elif defined(WDL_POSIX_NATIVE_WRITE)
    m_bufspace_used=0;
    m_filedes_locked=false;
    m_filedes=open(filename,O_WRONLY|O_CREAT
        // todo: use fcntl() for platforms when O_CLOEXEC is not available (if we ever need to support them)
        // (currently the only platform that meets this criteria is macOS w/ old SDK, but we don't use execve()
        // there
#ifdef O_CLOEXEC
        | O_CLOEXEC
#endif
        ,0644);
    if (m_filedes>=0)
    {

      if (!noFileLocking)
      {
        m_filedes_locked = !flock(m_filedes,LOCK_EX|LOCK_NB);
        if (!m_filedes_locked)
        {
          // this check might not be necessary, it might be sufficient to just fail and close if no exclusive lock possible
          if (errno == EWOULDBLOCK)  
          {
            // FAILED exclusive locking because someone else has a lock
            close(m_filedes); 
            m_filedes=-1;
          }
          else  // failed for some other reason, try to keep a shared lock at least
          {
            m_filedes_locked = !flock(m_filedes,LOCK_SH|LOCK_NB);
          }
        }
      }

      if (m_filedes>=0)
      {
        if (!wantAppendTo)
        {
          if (ftruncate(m_filedes,0) < 0)
          {
            WDL_ASSERT( false /* ftruncate() failed in WDL_FileWrite */ );
          }
        }
        else
        {
          struct stat64 st;
          if (!fstat64(m_filedes,&st))  SetPosition(st.st_size);
        }
      }

      
#ifdef __APPLE__
      if (m_filedes >= 0 && allow_async>1) fcntl(m_filedes,F_NOCACHE,1);
#endif
    }
    if (minbufs * bufsize >= 16384) m_bufspace.Resize((minbufs*bufsize+4095)&~4095);
#else
    m_fp=fopen(filename,wantAppendTo ? "a+b" : "wb");
    if (wantAppendTo && m_fp) 
      fseek(m_fp,0,SEEK_END);
#endif
  }

  ~WDL_FileWrite()
  {
#ifdef WDL_WIN32_NATIVE_WRITE
    // todo, async close stuff?
    if (m_fh != INVALID_HANDLE_VALUE && m_async)
    {
      SyncOutput(true);
    }

    m_empties.Empty(true);
    m_pending.Empty(true);

    if (m_fh != INVALID_HANDLE_VALUE) CloseHandle(m_fh);
    m_fh=INVALID_HANDLE_VALUE;
#elif defined(WDL_POSIX_NATIVE_WRITE)
   if (m_filedes >= 0)
   {
     if (m_bufspace.GetSize() > 0 && m_bufspace_used>0)
     {
       int v=(int)pwrite(m_filedes,m_bufspace.Get(),m_bufspace_used,m_file_position);
       if (v>0) m_file_position+=v;
       if (m_file_position > m_file_max_position) m_file_max_position=m_file_position;
       m_bufspace_used=0;
     }
     if (m_filedes_locked) flock(m_filedes,LOCK_UN);
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
#ifdef WDL_WIN32_NATIVE_WRITE
    return (m_fh != INVALID_HANDLE_VALUE);
#elif defined(WDL_POSIX_NATIVE_WRITE)
    return m_filedes >= 0;
#else
    return m_fp != NULL;
#endif
  }


  int Write(const void *buf, int len)
  {
#ifdef WDL_WIN32_NATIVE_WRITE
    if (m_fh == INVALID_HANDLE_VALUE) return 0;

    if (m_async)
    {
      int rdpos = 0;
      while (len > 0)
      {
        if (!m_empties.GetSize()) 
        {
          WDL_FileWrite__WriteEnt *ent=m_pending.Get(0);
          DWORD s=0;
          if (ent)
          {
            bool wasabort=false;
            if (GetOverlappedResult(m_fh,&ent->m_ol,&s,FALSE)||
                (wasabort=(GetLastError()==ERROR_OPERATION_ABORTED))) 
            {
              m_pending.Delete(0);

              if (wasabort) 
              {
                if (!RunAsyncWrite(ent,false)) m_empties.Add(ent);
              }
              else
              {
                m_empties.Add(ent);
                ent->m_bufused=0;
              }
            }
          }
        }


        WDL_FileWrite__WriteEnt *ent=m_empties.Get(0);
        if (!ent) 
        {
          if (m_pending.GetSize()>=m_async_maxbufs)
          {
            SyncOutput(false);
          }

          if (!(ent=m_empties.Get(0))) 
            m_empties.Add(ent = new WDL_FileWrite__WriteEnt(m_async_bufsize)); // new buffer

          
        }

        int ml=ent->m_bufsz-ent->m_bufused;
        if (ml>len) ml=len;
        memcpy(ent->m_bufptr+ent->m_bufused,(const char *)buf + rdpos,ml);

        ent->m_bufused+=ml;
        len-=ml;
        rdpos+=ml;

        if (ent->m_bufused >= ent->m_bufsz)
        {
          if (RunAsyncWrite(ent,true)) m_empties.Delete(0); // if queued remove from list
        }
      }
      return rdpos; 
    }
    else
    {
      DWORD dw=0;
      WriteFile(m_fh,buf,len,&dw,NULL);
      m_file_position+=dw;
      if (m_file_position>m_file_max_position) m_file_max_position=m_file_position;
      return dw;
    }
#elif defined(WDL_POSIX_NATIVE_WRITE)
   if (m_bufspace.GetSize()>0)
   {
     char *rdptr = (char *)buf;
     int rdlen = len;
     while (rdlen>0)
     {
       int amt = m_bufspace.GetSize() - m_bufspace_used;
       if (amt>0)
       {
         if (amt>rdlen) amt=rdlen;
         memcpy((char *)m_bufspace.Get()+m_bufspace_used,rdptr,amt);
         m_bufspace_used += amt;
         rdptr+=amt;
         rdlen -= amt;

         if (m_file_position+m_bufspace_used > m_file_max_position) m_file_max_position=m_file_position + m_bufspace_used;
       }
       if (m_bufspace_used >= m_bufspace.GetSize())
       {
         int v=(int)pwrite(m_filedes,m_bufspace.Get(),m_bufspace_used,m_file_position);
         if (v>0) m_file_position+=v;
         m_bufspace_used=0;
       }
     }    
     return len;
   }
   else
   {
     int v=(int)pwrite(m_filedes,buf,len,m_file_position);
     if (v>0) m_file_position+=v;
     if (m_file_position > m_file_max_position) m_file_max_position=m_file_position;
     return v;
   }
#else
    return fwrite(buf,1,len,m_fp);
#endif

    
  }

  WDL_FILEWRITE_POSTYPE GetSize()
  {
#ifdef WDL_WIN32_NATIVE_WRITE
    if (m_fh == INVALID_HANDLE_VALUE) return 0;
    DWORD h=0;
    DWORD l=GetFileSize(m_fh,&h);
    WDL_FILEWRITE_POSTYPE tmp=(((WDL_FILEWRITE_POSTYPE)h)<<32)|l;
    WDL_FILEWRITE_POSTYPE tmp2=GetPosition();
    if (tmp<m_file_max_position) return m_file_max_position;
    if (tmp<tmp2) return tmp2;
    
    return tmp;
#elif defined(WDL_POSIX_NATIVE_WRITE)
    if (m_filedes < 0) return -1;
    return m_file_max_position;
#else
    if (!m_fp) return -1;
    int opos=ftell(m_fp);
    fseek(m_fp,0,SEEK_END);
    int a=ftell(m_fp);
    fseek(m_fp,opos,SEEK_SET);
    return a;

#endif
  }

  WDL_FILEWRITE_POSTYPE GetPosition()
  {
#ifdef WDL_WIN32_NATIVE_WRITE
    if (m_fh == INVALID_HANDLE_VALUE) return -1;

    WDL_FILEWRITE_POSTYPE pos=m_file_position;
    if (m_async)
    {
      WDL_FileWrite__WriteEnt *ent=m_empties.Get(0);
      if (ent) pos+=ent->m_bufused;
    }
    return pos;
#elif defined(WDL_POSIX_NATIVE_WRITE)
    if (m_filedes < 0) return -1;
    return m_file_position + m_bufspace_used;
#else
    if (!m_fp) return -1;
    return ftell(m_fp);

#endif
  }

#ifdef WDL_WIN32_NATIVE_WRITE

  bool RunAsyncWrite(WDL_FileWrite__WriteEnt *ent, bool updatePosition) // returns true if ent is added to pending
  {
    if (ent && ent->m_bufused>0) 
    {
      if (updatePosition) 
      {
        ent->m_last_writepos = m_file_position;
        m_file_position += ent->m_bufused;
        if (m_file_position>m_file_max_position) m_file_max_position=m_file_position;
      }

#ifdef WIN32_ASYNC_NOBUF_WRITE
      if (ent->m_bufused&4095)
      {
        int offs=(ent->m_bufused&4095);
        char tmp[4096];
        memset(tmp,0,4096);

        *(WDL_FILEWRITE_POSTYPE *)&ent->m_ol.Offset = ent->m_last_writepos + ent->m_bufused - offs;
        ResetEvent(ent->m_ol.hEvent);

        DWORD dw=0;
        if (!ReadFile(m_fh,tmp,4096,&dw,&ent->m_ol))
        {
          if (GetLastError() == ERROR_IO_PENDING) 
            WaitForSingleObject(ent->m_ol.hEvent,INFINITE);
        }
        memcpy(ent->m_bufptr+ent->m_bufused,tmp+offs,4096-offs);

        ent->m_bufused += 4096-offs;
      }
#endif
      DWORD d=0;

      *(WDL_FILEWRITE_POSTYPE *)&ent->m_ol.Offset = ent->m_last_writepos;

      ResetEvent(ent->m_ol.hEvent);

      if (!WriteFile(m_fh,ent->m_bufptr,ent->m_bufused,&d,&ent->m_ol))
      {
        if (GetLastError()==ERROR_IO_PENDING)
        {
          m_pending.Add(ent);
          return true;
        }
      }
      ent->m_bufused=0;
    }
    return false;
  }

  void SyncOutput(bool syncall)
  {
    if (syncall)
    {
      if (RunAsyncWrite(m_empties.Get(0),true)) m_empties.Delete(0);
    }
    for (;;)
    {
      WDL_FileWrite__WriteEnt *ent=m_pending.Get(0);
      if (!ent) break;
      DWORD s=0;
      m_pending.Delete(0);
      if (!GetOverlappedResult(m_fh,&ent->m_ol,&s,TRUE) && GetLastError()==ERROR_OPERATION_ABORTED)
      {
        // rewrite this one
        if (!RunAsyncWrite(ent,false)) m_empties.Add(ent);
      }
      else
      {
        m_empties.Add(ent);
        ent->m_bufused=0;
        if (!syncall) break;
      }
    }
  }

#endif


  bool SetPosition(WDL_FILEWRITE_POSTYPE pos) // returns 0 on success
  {
#ifdef WDL_WIN32_NATIVE_WRITE
    if (m_fh == INVALID_HANDLE_VALUE) return true;
    if (m_async)
    {
      SyncOutput(true);
      m_file_position=pos;
      if (m_file_position>m_file_max_position) m_file_max_position=m_file_position;

#ifdef WIN32_ASYNC_NOBUF_WRITE
      if (m_file_position&4095)
      {
        WDL_FileWrite__WriteEnt *ent=m_empties.Get(0);
        if (ent)
        {
          int psz=(int) (m_file_position&4095);

          m_file_position -= psz;
          *(WDL_FILEWRITE_POSTYPE *)&ent->m_ol.Offset = m_file_position;
          ResetEvent(ent->m_ol.hEvent);

          DWORD dwo=0;
          if (!ReadFile(m_fh,ent->m_bufptr,4096,&dwo,&ent->m_ol))
          {
            if (GetLastError() == ERROR_IO_PENDING) 
              WaitForSingleObject(ent->m_ol.hEvent,INFINITE);
          }
          ent->m_bufused=(int)psz;
        }
      }
#endif
      return false;
    }

    m_file_position=pos;
    if (m_file_position>m_file_max_position) m_file_max_position=m_file_position;

    LONG high=(LONG) (m_file_position>>32);
    return SetFilePointer(m_fh,(LONG)(m_file_position&((WDL_FILEWRITE_POSTYPE)0xFFFFFFFF)),&high,FILE_BEGIN)==0xFFFFFFFF && GetLastError() != NO_ERROR;
#elif defined(WDL_POSIX_NATIVE_WRITE)

    if (m_filedes < 0) return true;
    if (m_bufspace.GetSize() > 0 && m_bufspace_used>0)
    {
      int v=(int)pwrite(m_filedes,m_bufspace.Get(),m_bufspace_used,m_file_position);
      if (v>0) m_file_position+=v;
      if (m_file_position > m_file_max_position) m_file_max_position=m_file_position;
      m_bufspace_used=0;
    }

    m_file_position = pos; // seek!
    if (m_file_position>m_file_max_position) m_file_max_position=m_file_position;
    return false;
#else
    if (!m_fp) return true;
    return !!fseek(m_fp,pos,SEEK_SET);
#endif
  }

  WDL_FILEWRITE_POSTYPE m_file_position, m_file_max_position;

#ifdef WDL_WIN32_NATIVE_WRITE
  HANDLE GetHandle() { return m_fh; }
  HANDLE m_fh;
  bool m_async;

  int m_async_bufsize, m_async_minbufs, m_async_maxbufs;

  WDL_PtrList<WDL_FileWrite__WriteEnt> m_empties;
  WDL_PtrList<WDL_FileWrite__WriteEnt> m_pending;

#elif defined(WDL_POSIX_NATIVE_WRITE)
  int GetHandle() { return m_filedes; }

  WDL_HeapBuf m_bufspace;
  int m_bufspace_used;
  int m_filedes;

  bool m_filedes_locked;

#else
  int GetHandle() { return fileno(m_fp); }
 
  FILE *m_fp;
#endif
} WDL_FIXALIGN;






#endif
