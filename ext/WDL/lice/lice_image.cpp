#include "lice.h"



LICE_IBitmap* LICE_LoadImage(const char* filename, LICE_IBitmap* bmp, bool tryIgnoreExtension)
{
  _LICE_ImageLoader_rec *hdr = LICE_ImageLoader_list;
  while (hdr)
  {
    LICE_IBitmap *ret = hdr->loadfunc(filename,true,bmp);
    if (ret) return ret;
    hdr=hdr->_next;
  }
  if (tryIgnoreExtension)
  {
    hdr = LICE_ImageLoader_list;
    while (hdr)
    {
      LICE_IBitmap *ret = hdr->loadfunc(filename,false,bmp);
      if (ret) return ret;
      hdr=hdr->_next;
    }
  }

  return 0;
}


static bool grow_buf(char **buf, int *bufsz, int *wrpos, const char *rd, int len)
{
  if (*wrpos + len > *bufsz)
  {
    char *nbuf=(char*)realloc(*buf,*bufsz = *wrpos+len+4096);
    if (!nbuf) return true; // ugh
    *buf = nbuf;
  }
  memcpy(*buf+*wrpos,rd,len);
  *wrpos+=len;
  return false;
}

char *LICE_GetImageExtensionList(bool wantAllSup, bool wantAllFiles)
{
  _LICE_ImageLoader_rec *hdr = LICE_ImageLoader_list;
  int bufsz=4096;
  int wrpos=0;
  char *buf=(char *)malloc(bufsz);
  buf[0]=buf[1]=buf[2]=0;

  if (wantAllSup)
  {
    static const char af[]="All supported images";
    if (grow_buf(&buf,&bufsz,&wrpos,af,sizeof(af))) { free(buf); return NULL; } // fail

    int cnt=0;
    while (hdr)
    {
      const char *rd = hdr->get_extlist();
      if (rd && *rd)
      {
        bool st=false;
        const char *p=rd;
        while (*p)
        {
          if (st)
          {
            if (cnt++)
              if (grow_buf(&buf,&bufsz,&wrpos,";",1))  { free(buf); return NULL; } // fail
            if (grow_buf(&buf,&bufsz,&wrpos,p,(int)strlen(p)+1))  { free(buf); return NULL; } // fail
            wrpos--;
          }
          while (*p) p++;
          p++;
          st=!st;
        }
      }
      hdr=hdr->_next;
    }

    if (!cnt) 
    {
      wrpos=0; // reset if nothing meaningful added
      buf[0]=buf[1]=buf[2]=0;
    }
    else wrpos++;

    hdr = LICE_ImageLoader_list;
  }

  while (hdr)
  {
    const char *rd = hdr->get_extlist();
    if (rd && *rd)
    {
      int len = 0;
      while (rd[len] || rd[len+1]) len++; // doublenull terminated list

      if (len)
      {
        if (grow_buf(&buf, &bufsz, &wrpos, rd, len+2)) return buf; // fail
        wrpos--; // remove doublenull on next round
      }
    }
    hdr=hdr->_next;
  }
  if (wantAllFiles)
  {
    static const char af[]="All files (*.*)\0*.*\0";
    if (grow_buf(&buf,&bufsz,&wrpos,af,sizeof(af))) return buf; // fail
    wrpos--;
  }

  return buf;
}

bool LICE_ImageIsSupported(const char *filename)
{
  const char *extension = filename;
  while (*extension)extension++;
  while (extension>=filename && *extension != '/' && *extension != '.' && *extension != '\\') extension--;
  if (extension<filename || *extension != '.') return false;

  const size_t extlen = strlen(extension);

  _LICE_ImageLoader_rec *hdr = LICE_ImageLoader_list;
  while (hdr)
  {
    const char *el = hdr->get_extlist();
    if (el)
    {
      while (*el) el++; // skip desc
      ++el;

      // scan rest of buffer
      while (*el)
      {
        if (!strnicmp(el,extension,extlen))
        {
          if (el[extlen] == 0|| el[extlen] == ';') return true;
        }
        el++;
      }
    }
    hdr=hdr->_next;
  }
  return false;
}
