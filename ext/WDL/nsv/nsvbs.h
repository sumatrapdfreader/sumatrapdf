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
** nsvbs.h - NSV basic inline bitstream classes
**
**
** Note: these bitstream classes encode/decode everything in LSB.
** bits are stored from the lowest bit to the highest bit.
**  putbits(4,0xE) will result in getbits(1)=0, getbits(1)=1, 
**                                getbits(1)=1, getbits(1)=1
**    or of course, getbits(4) == 0xE :)
*/

#ifndef _NSVBS_H_
#define _NSVBS_H_

#include <stdlib.h>
#include <memory.h>

class nsv_GrowBuf
{
  public:
    nsv_GrowBuf() { m_alloc=m_used=0; m_s=NULL; }
    ~nsv_GrowBuf() { if (m_s) free(m_s); m_s=0; }

    int add(void *data, int len) 
    { 
      if (len<=0) return 0;
      resize(m_used+len); 
      if (m_s) memcpy((char*)m_s+m_used-len,data,len);
      return m_used-len;
    }

    void set(void *data, int len)
    {
      resize(len);
      if (m_s) memcpy((char*)m_s,data,len);
    }

    void resize(int newlen)
    {
      m_used=newlen;
      if (newlen > m_alloc)
      {
        void *ne;
        m_alloc = newlen*2;
        if (m_alloc < 1024) m_alloc =1024;
        ne = realloc(m_s, m_alloc);
        if (!ne)
        {
          ne=malloc(m_alloc);
          if (ne) memcpy(ne,m_s,m_used);
          free(m_s);
        }
        m_s=ne;
      }
    }

    int getlen() { return m_used; }
    void *get() { return m_s; }

  private:
    void *m_s;
    int m_alloc;
    int m_used;

};


class nsv_OutBS
{
public:
  nsv_OutBS() { m_used = 0; m_curb=0; }
  ~nsv_OutBS() { }

  void putbits(int nbits, unsigned int value)
  {
    while (nbits-- > 0)
    {
      m_curb|=(value&1)<<(m_used&7);
      if (!((++m_used)&7))
      {
        m_bits.add(&m_curb,1);
        m_curb=0;
      }
      value>>=1;
    }
  }

    // lets you put in any amount of data, but does not preserve endianness.
  void putdata(int nbits, void *data) 
  {
    unsigned char *c=(unsigned char *)data;
    if (!(m_used&7) && nbits >= 8)
    {
      m_bits.add(c,nbits/8);
      c+=nbits/8;
      m_used+=nbits&~7;
      nbits&=7;
    }
    while (nbits > 0)
    {
      int tb=nbits;
      if (tb > 8) tb=8;
      putbits(tb,*c++);
      nbits-=tb;
    }
  }

  int getlen() { return m_used; } // in bits

  void *get(int *len) // len is in bytes, forces to byte aligned.
  {
    if (m_used&7)
    {
      m_bits.add(&m_curb,1);
      m_used=(m_used+7)&~7;
      m_curb=0;
    }
    *len=m_used/8;
    return m_bits.get();
  }

  void clear()
  {
    m_used=0;
    m_curb=0;
    m_bits.resize(0);
  }

private:
  nsv_GrowBuf m_bits;
  int m_used; // bits
  unsigned char m_curb;
};

class nsv_InBS {
public:
  nsv_InBS() { m_bitpos=0; m_eof=0; }
  ~nsv_InBS() { }

  void clear() 
  {
    m_eof=0;
    m_bitpos=0;
    m_bits.resize(0);
  }

  void add(void *data, int len)
  {
    m_bits.add(data,len);
    m_eof=0;
  }

  void addbyte(unsigned char byte)
  {
    add(&byte,1);
  }

  void addint(unsigned int dword)
  {
    addbyte(dword&0xff);
    addbyte((dword>>8)&0xff);
    addbyte((dword>>16)&0xff);
    addbyte((dword>>24)&0xff);
  }
  
  void compact()
  {
    int bytepos=m_bitpos/8;
    if (bytepos)
    {
      unsigned char *t=(unsigned char *)m_bits.get();
      int l=m_bits.getlen()-bytepos;
      if (t) memcpy(t,t+bytepos,l);
      m_bits.resize(l);
      m_bitpos&=7;
    }
    m_eof=0;
  }


  void seek(int nbits) { m_bitpos+=nbits; if (m_bitpos < 0) m_bitpos=0; m_eof=m_bits.getlen()*8 < m_bitpos; }
  void rewind() { m_bitpos=0; m_eof=0; }
  int eof() { return m_eof; }
  int avail() { if (m_eof) return 0; return m_bits.getlen()*8 - m_bitpos; }

  unsigned int getbits(int nbits)
  {
    unsigned int ret=0;
    if (nbits <= 0) return ret;
    unsigned char *t=(unsigned char *)m_bits.get();

    if (!t || m_bits.getlen()*8 < m_bitpos+nbits) m_eof=1;
    else
    {
      int sh=0;
      t+=m_bitpos/8;
      for (sh = 0; sh < nbits; sh ++)
      {
        ret|=((*t>>(m_bitpos&7))&1) << sh;
        if (!((++m_bitpos)&7)) t++;
      }
    }
    return ret;
  }

  int getdata(int nbits, void *data)
  {
    unsigned char *t=(unsigned char *)data;
    if (m_bits.getlen()*8 < m_bitpos+nbits) return 1;
    if (!(m_bitpos&7) && nbits >= 8)
    {
      char *bitptr=(char*)m_bits.get();
      bitptr+=(m_bitpos/8);
      if (bitptr) memcpy(t,bitptr,nbits/8);
      m_bitpos+=nbits&~7;

      t+=nbits/8;
      nbits&=7;
    }
    while (nbits > 0)
    {
      int nb=nbits;
      if (nb > 8) nb=8;
      *t++=getbits(nb);
      nbits-=nb;
    }
    return 0;
  }

  void *getcurbyteptr()
  {
    char *t=(char*)m_bits.get();
    if (t) return (void *)(t+(m_bitpos/8));
    return 0;
  }

private:
  nsv_GrowBuf m_bits;
  int m_bitpos;
  int m_eof;
};

#endif//_NSVBS_H_
