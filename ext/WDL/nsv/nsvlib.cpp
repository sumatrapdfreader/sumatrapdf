/*
  LICENSE
  -------
Copyright 2005 Nullsoft, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer. 

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution. 

  * Neither the name of Nullsoft nor the names of its contributors may be used to 
    endorse or promote products derived from this software without specific prior written permission. 
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR 
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT 
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
/*
** nsvlib.cpp - NSV file/bitstream reading/writing code
** 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nsvlib.h"

#define NSV_HDR_DWORD (NSV_MAKETYPE('N','S','V','f'))

#define NSV_SYNC_HEADERLEN_BITS 192
#define NSV_SYNC_DWORD (NSV_MAKETYPE('N','S','V','s'))
#define NSV_NONSYNC_HEADERLEN_BITS 56
#define NSV_NONSYNC_WORD 0xBEEF

#define NSV_INVALID_SYNC_OFFSET 0x80000000

/*
  NSV sync packet header
  32 bits: NSV_SYNC_DWORD
  32 bits: video format
  32 bits: audio format
  16 bits: width
  16 bits: height
  8  bits: framerate (see getfrate/setfrate)


  16 bits: audio/video sync offset

or

  NSV nonsync packet header
  16 bits: NSV_NONSYNC_WORD

then 

4  bits: # aux data channels present (max 15)
20 bits: video data + aux channels length
16 bits: audio data length

--------------------------------
sync:
  192 bit header, 
  136 bits are invariant
nonsync:
  56 bit header
  16 bits are invariant

*/




static int is_type_char_valid(int c)
{
  c&=0xff;
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') ||
         c == ' ' || c == '-' || 
         c == '.' || c == '_';
}

static int is_type_valid(unsigned int t)
{
  return (t&0xff) != ' ' &&
          is_type_char_valid(t>>24) &&
          is_type_char_valid(t>>16) &&
          is_type_char_valid(t>>8) &&
          is_type_char_valid(t);
}


void nsv_type_to_string(unsigned int t, char *out)
{
  if (is_type_valid(t))
  {
    out[0]=(t)&0xff;
    out[1]=(t>>8)&0xff;
    out[2]=(t>>16)&0xff;
    out[3]=(t>>24)&0xff;
    out[4]=0;
    int x=3;
    while (out[x]==' ' && x > 0) out[x--]=0;
  }
  else *out=0;
}

unsigned int nsv_string_to_type(char *in)
{
  int n;
  unsigned int ret=*in;
  if (*in == ' ' || !is_type_char_valid(*in)) return 0;
  in++;
  for (n = 0; n < 3; n ++)
  {
    if (!is_type_char_valid(*in)) break;
    ret|=(*in<<(8+8*n));
    in++;
  }
  if (*in) return 0;
  return ret;
}

// frate is specified
// XYYYYYZZ
// if !X, framerate is YYYYYZZ (1-127)
// otherwise:
//   ZZ indexes base
//   YYYYY is scale (0-32). 
//     if YYYYY < 16, then scale = 1/(YYYY+1)
//     otherwise scale = YYYYY-15


static double frate2double(unsigned char fr)
{
  static double fratetab[]=
  {
    30.0,
    30.0*1000.0/1001.0,
    25.0,
    24.0*1000.0/1001.0,
  };
  if (!(fr&0x80)) return (double)fr;

  double sc;
  int d=(fr&0x7f)>>2;
  if (d < 16) sc=1.0/(double)(d+1);
  else sc=d-15;

  return fratetab[fr&3]*sc; 
}



static unsigned char double2frate(double fr)
{
  int best=0;
  double best_v=1000000.0;
  int x;
  for (x = 0; x < 256; x ++)
  {
    double this_v=(fr-frate2double(x));

    if (this_v<0) this_v=-this_v;
    if (this_v < best_v) 
    {
      best_v=this_v;
      best=x;
    }
  }
  return (unsigned char) best;
}



nsv_Packeter::nsv_Packeter()
{
  vidfmt=audfmt=0;
  width=height=0;
  framerate_idx=0;
  framerate=0.0;
  syncoffset_cur=0;
  video=NULL;
  audio=NULL;
  video_len=0;
  audio_len=0;
  aux_used=0;
}

void nsv_Packeter::setVidFmt(unsigned int vfmt, unsigned int w, unsigned int h, double frt)
{
  vidfmt=vfmt;
  width=w;
  height=h;
  framerate=frt;
  framerate_idx=double2frate(frt);
}

nsv_Packeter::~nsv_Packeter()
{
}

int nsv_Packeter::packet(nsv_OutBS &bs)
{
  int total_auxlen=0;
  int x;
  if (width >= (1<<16) || height >= (1<<16) || 
      !framerate_idx ||
      !is_type_valid(audfmt) || 
      !is_type_valid(vidfmt) ||
      video_len > NSV_MAX_VIDEO_LEN || 
      audio_len > NSV_MAX_AUDIO_LEN ||
      aux_used > NSV_MAX_AUXSTREAMS || 
      aux_used < 0    
    ) return -1;

  for (x = 0; x < aux_used; x ++)
  {
    if (aux_len[x] > NSV_MAX_AUX_LEN) return -1;
    total_auxlen+=aux_len[x]+6;
  }
  
  if (is_sync_frame)
  {
    bs.putbits(32,NSV_SYNC_DWORD);
    bs.putbits(32,vidfmt);
    bs.putbits(32,audfmt);
    bs.putbits(16,width);
    bs.putbits(16,height);
    bs.putbits(8 ,framerate_idx);
    bs.putbits(16,syncoffset_cur);
  }
  else
  {
    bs.putbits(16,NSV_NONSYNC_WORD);
  }
  
  bs.putbits(4,aux_used); // no aux data channels for our streams yet
  bs.putbits(20,video_len+total_auxlen);
  bs.putbits(16,audio_len);

  for (x = 0; x < aux_used; x ++)
  {
    bs.putbits(16,aux_len[x]); // length of 0 for aux channels
    bs.putbits(32,aux_types[x]);
    if (aux_len[x]) bs.putdata(aux_len[x]*8,aux[x]);
  }

  if (video_len) bs.putdata(video_len*8,video);
  if (audio_len) bs.putdata(audio_len*8,audio);

  return 0;
}


void nsv_Unpacketer::reset(int full)
{
  synched=0;
  is_sync_frame=0;
  syncoffset_cur=0;
  syncoffset=NSV_INVALID_SYNC_OFFSET;

  if (full)
  {
    m_auxbs=NULL;
    m_audiobs=NULL;
    m_videobs=NULL;
    m_eof=0;
    vidfmt=0;
    audfmt=0;
    valid=0;
    width=0;
    height=0;
    framerate=0.0;
    framerate_idx=0;
  }
}


// returns 0 on success, >0 on needs (at least X bytes) more data, 
//   -1 on error (no header found in block)
int nsv_Unpacketer::unpacket(nsv_InBS &bs)
{
  int gotframe=0;
  int num_aux=0;
  int vl=0;
  int al=0;
   
  while (bs.avail()>=NSV_NONSYNC_HEADERLEN_BITS)
  {
    if (valid && synched)
    {
      if (bs.avail() < NSV_NONSYNC_HEADERLEN_BITS)
        return m_eof?-1:(NSV_NONSYNC_HEADERLEN_BITS-bs.avail())/8;

      unsigned int d=bs.getbits(16);
      if (d == NSV_NONSYNC_WORD)
      {
        num_aux=bs.getbits(4);
        vl=bs.getbits(20);
        al=bs.getbits(16);
        if (al >= NSV_MAX_AUDIO_LEN || 
            vl >= (NSV_MAX_VIDEO_LEN+num_aux*(NSV_MAX_AUX_LEN+6)))
        {
          bs.seek(-NSV_NONSYNC_HEADERLEN_BITS);
        }
        else
        {
          if ((unsigned int)bs.avail() < 8*(vl+al)+(m_eof?0:32))
          {
            int l=(al+vl+32/8)-bs.avail()/8;
            bs.seek(-NSV_NONSYNC_HEADERLEN_BITS);
            return m_eof?-1:l;
          }

          if ((unsigned int)bs.avail() >= 8*(vl+al)+32)
          {
            bs.seek(8*(vl+al));
            unsigned int a32=bs.getbits(32);
            bs.seek(-32);
            unsigned int a16=bs.getbits(16);
            bs.seek(-16);
            bs.seek(-8*(vl+al));
            if (a16 != NSV_NONSYNC_WORD && a32 != NSV_SYNC_DWORD)
            {
              bs.seek(-NSV_NONSYNC_HEADERLEN_BITS);
            }
            else gotframe=NSV_NONSYNC_HEADERLEN_BITS;
          }
          else gotframe=NSV_NONSYNC_HEADERLEN_BITS;
        }
      }
      else bs.seek(-16);
    } // inf.valid && inf.synched

    // gotframe is set if we successfully got a nonsync frame, otherwise
    // let's see if we can't interpret this as a sync frame

    if (!gotframe)
    {
      if (bs.avail() < NSV_SYNC_HEADERLEN_BITS)
        return m_eof?-1:(NSV_SYNC_HEADERLEN_BITS-bs.avail())/8;
      unsigned int d=bs.getbits(32);
      if (d != NSV_SYNC_DWORD)
      {
        bs.seek(8-32); // seek back 3 bytes
        synched=0;
        continue;
      }
      unsigned int vfmt=bs.getbits(32);
      unsigned int afmt=bs.getbits(32);
      unsigned int w=bs.getbits(16);
      unsigned int h=bs.getbits(16);
      unsigned char frt=bs.getbits(8);
      unsigned int so=bs.getbits(16);

      num_aux=bs.getbits(4);
      vl=bs.getbits(20);
      al=bs.getbits(16);

      if (al >= NSV_MAX_AUDIO_LEN || 
          vl >= (NSV_MAX_VIDEO_LEN+num_aux*(NSV_MAX_AUX_LEN+6)) ||
          !frt || !is_type_valid(vfmt) || !is_type_valid(afmt) ||
          (valid &&
           (width != w || height != h ||
            vidfmt != vfmt || audfmt != afmt || framerate_idx != frt)))
      { // frame is definately not valid
        bs.seek(8-NSV_SYNC_HEADERLEN_BITS); // seek back what we just read
        synched=0;
        continue;
      }
      
      if ((unsigned int)bs.avail() < (al+vl)*8+((m_eof||(valid&&synched))?0:32))
      {
        int l=(al+vl)*8+NSV_SYNC_HEADERLEN_BITS-bs.avail();
        bs.seek(-NSV_SYNC_HEADERLEN_BITS);
        return m_eof?-1:(l/8);
      }

      if (valid && synched) 
      {
        gotframe=NSV_SYNC_HEADERLEN_BITS;
      }
      else // we need to do more robust sync
      {
        int sk=(al+vl)*8;
        bs.seek(sk);
        unsigned int a16=bs.getbits(16);
        bs.seek(-16);
        unsigned int a32=bs.getbits(32);
        bs.seek(-32);
        if (a16 == NSV_NONSYNC_WORD)
        {
          sk+=16+4+20+16;
          bs.seek(16+4);  //skip hdr + aux bits
          unsigned int _vl=bs.getbits(20);
          unsigned int _al=bs.getbits(16);
          if ((unsigned int)bs.avail() < (_vl+_al)*8 + 32)
          {
            int l=(_al+_vl+32)-bs.avail()/8;
            bs.seek(-NSV_SYNC_HEADERLEN_BITS-sk);
            return m_eof?-1:l;
          }
          bs.seek((_vl+_al)*8);
          sk+=(_vl+_al)*8;
          unsigned int a16=bs.getbits(16);
          bs.seek(-16);
          unsigned int a32=bs.getbits(32);
          bs.seek(-32);
          bs.seek(-sk);
          if (a16 == NSV_NONSYNC_WORD || a32 == NSV_SYNC_DWORD)
            gotframe=NSV_SYNC_HEADERLEN_BITS;
        }
        else if (a32 == NSV_SYNC_DWORD)
        {
			
			    sk+=32+32+32+16+16+8;
          
          bs.seek(32);
          unsigned int _vfmt=bs.getbits(32);
          unsigned int _afmt=bs.getbits(32);
          unsigned int _w=bs.getbits(16);
          unsigned int _h=bs.getbits(16);
          unsigned char _frt=bs.getbits(8);
          bs.seek(-sk);

          if (_vfmt==vfmt && _afmt==afmt && _w==w && _h==h && _frt==frt) // matches
          {
            gotframe=NSV_SYNC_HEADERLEN_BITS;
          }
        }
      }
      if (!gotframe)
      {
        synched=0;
        bs.seek(8-NSV_SYNC_HEADERLEN_BITS);
      }
      else
      {
        if (so & 0x8000) so|=0xFFFF0000;       
        syncoffset_cur=so;
        if (!valid || (unsigned int)syncoffset == NSV_INVALID_SYNC_OFFSET) syncoffset=so;
        if (!valid) framerate=frate2double(frt);
        framerate_idx=frt;
        width=w;
        height=h;
        audfmt=afmt;
        vidfmt=vfmt;
        valid=1;
        synched=1;
      }
    }

    if (gotframe)
    {
      is_sync_frame = (gotframe == NSV_SYNC_HEADERLEN_BITS);
      // read aux channels
      int rd=gotframe;
      unsigned int x;
      for (x = 0; x < num_aux; x ++)
      {
        unsigned int l=bs.getbits(16);
        unsigned int fmt=bs.getbits(32);
        vl -= 4+2;
        rd += 16+32;

        if (l > NSV_MAX_AUX_LEN) break;

        if (m_auxbs)
        {
          m_auxbs->addint(l);
          m_auxbs->addint(fmt);
          m_auxbs->add(bs.getcurbyteptr(),l);
        }
        bs.seek(l*8); // toss aux

        vl-=l;
        rd+=l*8;

        if (vl<0) break; // invalid frame (aux channels add up to more than video)
      }
      if (x < num_aux) // oh shit, invalid frame
      {
        synched=0;
        bs.seek(8-rd);
        gotframe=0;
        continue;
      }
    
      if (m_videobs)
      {
        m_videobs->addint(vl);
        m_videobs->add(bs.getcurbyteptr(),vl);
      }
      bs.seek(vl*8);

      if (m_audiobs)
      {
        m_audiobs->addint(al);
        m_audiobs->add(bs.getcurbyteptr(),al);
      }
      bs.seek(al*8);

      return 0;
    }
  } // while
  return m_eof?-1:(NSV_NONSYNC_HEADERLEN_BITS-bs.avail())/8;
}









/* NSV file header
4: NSV_HDR_DWORD
4: length of header in bytes
    -- may not be 0 or 0xFFFFFFFF. :)
4: length of file, in bytes (including header - if this is 0 we are invalid)
    -- can be 0xFFFFFFFF which means unknown length
4: length of file, in milliseconds (max file length, 24 days or so)
    -- can be 0xFFFFFFFF which means unknown length
4: metadata length
4: number of TOC entries allocated
4: number of TOC entries used
mdlen: metadata
TOC_alloc*4:offset in file at time t.
*/

void nsv_writeheader(nsv_OutBS &bs, nsv_fileHeader *hdr, unsigned int padto)
{
  if (hdr->toc_alloc < hdr->toc_size)
    hdr->toc_alloc=hdr->toc_size;

  if (hdr->toc_ex && hdr->toc_alloc <= hdr->toc_size*2)
    hdr->toc_alloc=hdr->toc_size*2+1;

  hdr->header_size = 4+4+4+4+4+hdr->metadata_len+4+4+4*hdr->toc_alloc;

  bs.putbits(32,NSV_HDR_DWORD);
  bs.putbits(32,hdr->header_size>padto?hdr->header_size:padto);
  if (hdr->file_lenbytes == 0xFFFFFFFF) bs.putbits(32,hdr->file_lenbytes);
  else bs.putbits(32,hdr->file_lenbytes+(hdr->header_size>padto?hdr->header_size:padto));
  bs.putbits(32,hdr->file_lenms);
  bs.putbits(32,hdr->metadata_len);
  bs.putbits(32,hdr->toc_alloc);
  bs.putbits(32,hdr->toc_size);
  bs.putdata(hdr->metadata_len*8,hdr->metadata);

  unsigned int numtoc=hdr->toc_alloc;
  unsigned int numtocused=hdr->toc_size;
  unsigned int *toc=hdr->toc;
  unsigned int *toc_ex=hdr->toc_ex;
  unsigned int numtocused2=(toc_ex && hdr->toc_alloc > hdr->toc_size*2) ? (hdr->toc_size + 1): 0;

  while (numtoc--)
  {
    if (!numtocused) 
    {
      if (numtocused2)
      {
        if (--numtocused2 == hdr->toc_size) // signal extended TOC :)
          bs.putbits(32,NSV_MAKETYPE('T','O','C','2'));
        else
          bs.putbits(32,*toc_ex++);
      }
      else // extra (unused by this implementation but could be used someday so we fill it with 0xFF) space
        bs.putbits(32,~0);
    }
    else if (toc) 
    {
      bs.putbits(32,*toc++);
      numtocused--;
    }
    else bs.putbits(32,0);
  }
  
  unsigned int x;
  for (x = hdr->header_size; x < padto; x ++) bs.putbits(8,0);
}


int nsv_readheader(nsv_InBS &bs, nsv_fileHeader *hdr)
{
  int s=0;
  hdr->metadata=(void*)NULL;
  hdr->toc=(unsigned int *)NULL;
  hdr->toc_ex=(unsigned int *)NULL;
  hdr->header_size=0;
  hdr->file_lenbytes=~0;
  hdr->file_lenms=~0;
  hdr->toc_alloc=0;
  hdr->toc_size=0;
  hdr->metadata_len=0;
  
  if (bs.avail()<64) {
	  return 8-bs.avail()/8;
  }
  s+=32;
  if (bs.getbits(32) != NSV_HDR_DWORD) 
  {
    bs.seek(-s);
    return -1;
  }
  s+=32;
  unsigned int headersize=bs.getbits(32);

  if ((unsigned int)bs.avail() < (headersize-4)*8)
  {
    int l=headersize-4-bs.avail()/8;
    bs.seek(-s);
    return l;
  }

  s+=32;
  unsigned int lenbytes=bs.getbits(32);
  s+=32;
  unsigned int lenms=bs.getbits(32);
  s+=32;
  unsigned int metadatalen=bs.getbits(32);
  s+=32;
  unsigned int tocalloc=bs.getbits(32);
  s+=32;
  unsigned int tocsize=bs.getbits(32);

  if (tocalloc < tocsize || lenbytes < headersize || tocalloc + metadatalen + s/8 > headersize)
  {
    bs.seek(-s);
    return -1;
  }

  void *metadata=NULL;

  if (metadatalen)
  {
    metadata=malloc(metadatalen+1);
    if (!metadata) 
    {
      bs.seek(-s);
      return -1;
    }
    s+=metadatalen*8;
    bs.getdata(metadatalen*8,metadata);
    ((char*)metadata)[metadatalen]=0;
  }

  unsigned int *toc=NULL;
  unsigned int *toc_ex=NULL;

  if (tocalloc)
  {
    toc=(unsigned int *)malloc(tocsize * 4 * 2);
    if (!toc) 
    {
      free(metadata);
      bs.seek(-s);
      return -1;
    }
    unsigned int x;
    int bitsread=0;
    for (x = 0; x < tocsize; x ++) { toc[x] = bs.getbits(32); bitsread += 32; }

    if (tocalloc > tocsize*2)
    {
      bitsread += 32;
      if (bs.getbits(32) == NSV_MAKETYPE('T','O','C','2'))
      {
        toc_ex=toc + tocsize;
        for (x = 0; x < tocsize; x ++) { toc_ex[x] = bs.getbits(32); bitsread += 32; }
      }
    }
    bs.seek((tocalloc-tocsize)*32 - bitsread);
    s+=tocalloc*32;
  }

  hdr->header_size=headersize;
  if (lenbytes == 0xFFFFFFFF) hdr->file_lenbytes=lenbytes;
  else hdr->file_lenbytes=lenbytes-headersize;
  hdr->file_lenms=lenms;
  hdr->metadata=metadata;
  hdr->metadata_len=metadatalen;
  hdr->toc=toc;
  hdr->toc_ex=toc_ex;
  hdr->toc_alloc=tocalloc;
  hdr->toc_size=tocsize;
  
  return 0;
}

char *nsv_getmetadata(void *metadata, char *name)
{
  if (!metadata) return NULL;
  char *p=(char*)metadata;
  int ln=strlen(name);
  for (;;)
  {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (!*p) break;
    if (!strnicmp(p,name,ln) && p[ln]=='=' && p[ln+1] && p[ln+2])
    {
      int cnt=0;
      char *np=p+ln+1;
      char c=*np++;
      while (np[cnt] && np[cnt] != c) cnt++;

      char *s=(char*)malloc(cnt+1);
      if (!s) return NULL;
      memcpy(s,np,cnt);
      s[cnt]=0;
      return s;
    }

    // advance to next item
    while (*p && *p != '=') p++;
    if (!*p++) break;
    if (!*p) break;
    char c=*p++;
    while (*p && *p != c) p++;
    if (*p) p++;
  }
  return NULL;
}
