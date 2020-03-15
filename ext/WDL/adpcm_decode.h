#ifndef _WDL_ADPCM_DECODE_H_
#define _WDL_ADPCM_DECODE_H_

#include "queue.h"

#define MSADPCM_TYPE 2
#define IMAADPCM_TYPE 0x11
#define CADPCM2_TYPE 0xac0c

class WDL_adpcm_decoder
{
  typedef struct
  {
    int cf1,cf2,deltas,spl1,spl2;
  } WDL_adpcm_decode_chanctx;

public:
  enum { MSADPCM_PREAMBLELEN=7, IMA_PREAMBLELEN=4 };
  static INT64 sampleLengthFromBytes(INT64 nbytes, int blockalign, int nch, int type, int bps)
  {
    if (!bps||type!=CADPCM2_TYPE) bps=4;
    // remove overhead of headers
    INT64 nblocks=((nbytes+blockalign-1)/blockalign);
       
    // remove preambles
    if (type==IMAADPCM_TYPE||type==CADPCM2_TYPE) nbytes -= nblocks*IMA_PREAMBLELEN*nch;
    else nbytes -= nblocks*MSADPCM_PREAMBLELEN*nch;

    // scale from bytes to samples
    nbytes = (nbytes*8)/(nch*bps);

    if (type==IMAADPCM_TYPE||type==CADPCM2_TYPE) nbytes++; // IMA has just one initial sample
    else nbytes+=2; // msadpcm has 2 initial sample values

    return nbytes;
  }

  WDL_adpcm_decoder(int blockalign,int nch, int type, int bps)
  {
    m_bps=0;
    m_type=0;
    m_nch=0;
    m_blockalign=0;
    m_srcbuf=0;
    m_chans=0;
    setParameters(blockalign,nch,type,bps);
  }
  ~WDL_adpcm_decoder() 
  {
    free(m_srcbuf);
    free(m_chans);
  }


  void resetState()
  {
    if (m_chans) memset(m_chans,0,m_nch*sizeof(WDL_adpcm_decode_chanctx));
    m_srcbuf_valid=0;
    samplesOut.Clear();
    samplesOut.Compact();
  }

  void setParameters(int ba, int nch, int type, int bps) 
  {
    if (m_blockalign != ba||nch != m_nch||type!=m_type||bps != m_bps)
    {
      free(m_srcbuf);
      free(m_chans);
      m_bps=bps;
      m_blockalign=ba;
      m_nch=nch;
      m_srcbuf_valid=0;
      m_srcbuf=(unsigned char*)malloc(ba);
      m_chans=(WDL_adpcm_decode_chanctx*)malloc(sizeof(WDL_adpcm_decode_chanctx)*nch);
      m_type=type;
      resetState();
    }
  }

  int blockAlign() { return m_blockalign; }

  int samplesPerBlock()
  {
    if (m_type==IMAADPCM_TYPE||m_type==CADPCM2_TYPE) 
    {
      if (m_bps == 2) return (m_blockalign/m_nch - IMA_PREAMBLELEN)*4 + 1;
      return (m_blockalign/m_nch - IMA_PREAMBLELEN)*2 + 1; //4 bit
    }
    return (m_blockalign/m_nch - MSADPCM_PREAMBLELEN)*2 + 2; // 4 bit
  }

  INT64 samplesToSourceBytes(INT64 outlen_samples) // length in samplepairs
  {
    outlen_samples -= samplesOut.Available()/m_nch;
    if (outlen_samples<1) return 0; // no data required

    int spls_block = samplesPerBlock();
    if (spls_block<1) return 0;
    INT64 nblocks = (outlen_samples+spls_block-1)/spls_block;
    INT64 v=nblocks * m_blockalign;
    
    v -= m_srcbuf_valid;

    return wdl_max(v,0);
  }

  void AddInput(void *buf, int len, short *parm_cotab=NULL)
  {
    unsigned char *rdbuf = (unsigned char *)buf;
    if (m_srcbuf_valid)
    {
      int v=m_blockalign-m_srcbuf_valid;
      if (v>len) v=len;
      
      memcpy(m_srcbuf+m_srcbuf_valid,rdbuf,v);
      len-=v;
      rdbuf+=v;
      if ((m_srcbuf_valid+=v)>=m_blockalign)
      {
        DecodeBlock(m_srcbuf,parm_cotab);
        m_srcbuf_valid=0;
      }
    }

    while (len >= m_blockalign)
    {
      DecodeBlock(rdbuf,parm_cotab);
      rdbuf+=m_blockalign;
      len-=m_blockalign;
    }
    if (len>0) memcpy(m_srcbuf,rdbuf,m_srcbuf_valid=len);
  }


  int sourceBytesQueued() { return m_srcbuf_valid; }
  WDL_TypedQueue<short> samplesOut;

private:
  static int getwordsigned(unsigned char **rdptr)
  {
    int s = (*rdptr)[0] + ((*rdptr)[1]<<8);
    (*rdptr)+=2;
    if (s & 0x8000) s -= 0x10000;
    return s;
  }

  bool DecodeBlockIMA(unsigned char *buf)
  {
    int samples_block = samplesPerBlock();

    int nch=m_nch;
    int ch;
    short *outptr = samplesOut.Add(NULL,samples_block * nch);

    for (ch=0;ch<nch;ch++)
    {
      m_chans[ch].spl1 = getwordsigned(&buf);
      m_chans[ch].cf1 = buf[0] | (buf[1]<<8);
      buf+=2;
    }

    for (ch=0;ch<nch;ch++) *outptr++ = m_chans[ch].spl1;

    char bstate=0;
    unsigned char lastbyte=0;
    int x;

    static signed char index_table[8] = { -1, -1, -1, -1, 2, 4, 6, 8 }; 
    static short step_table[89] = { 
      7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 
      19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 
      50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 
      130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
      337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
      876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 
      2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
      5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 
      15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 
    };

 
    int splcnt = (samples_block-1)*nch;
    int cnt=0;
    ch=0;
    int wrpos = 0;
    
    int bps=m_bps;
    const int chunksize = bps == 2 ? 16 : 8;

    for (x=0; x < splcnt; x++)
    {
      int nib;

      if (bps==2)
      {
        switch (bstate++)
        {
          case 0: nib=(lastbyte=*buf++)<<2; break;
          case 1: nib=lastbyte; break;
          case 2: nib=lastbyte>>2; break;
          default: nib=lastbyte>>4; bstate=0; break;
        }
        nib &= 8|4;
      }
      else
      {
        if ((bstate^=1)) nib=(lastbyte=*buf++)&0xf;
        else nib=lastbyte>>4;
      }

      int step_index=m_chans[ch].cf1;
      if (step_index<0)step_index=0;
      else if (step_index>88)step_index=88;

      int step=step_table[step_index];

      int diff = ((nib&7)*step)/4 + step/8;

      int v=m_chans[ch].spl1 + ((nib&8) ? -diff : diff);

      if (v<-32768)v=-32768;
      else if (v>32767)v=32767;

      outptr[wrpos]=(short)v;
      wrpos+=nch;

      m_chans[ch].spl1=v;

      m_chans[ch].cf1=step_index + index_table[nib&7];


      // advance channelcounts
      if (++cnt==chunksize)
      {
        if (++ch>=nch) 
        {
          ch=0;
          outptr += chunksize*nch;
        }
        wrpos = ch;
        cnt=0;
      }
    }

    return true;
  }

  bool DecodeBlock(unsigned char *buf, short *parm_cotab=NULL)
  {
    if (m_type==IMAADPCM_TYPE||m_type==CADPCM2_TYPE) return DecodeBlockIMA(buf);
    static short cotab[14] = { 256,0, 512,-256, 0,0, 192,64, 240, 0,460, -208,  392, -232 };
    static short adtab[16] = { 230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230 };

    short *use_cotab = parm_cotab ? parm_cotab : cotab;
    int nch = m_nch;
    int ch;
    for(ch=0;ch<nch;ch++)
    {
      unsigned char c=*buf++;
      if (c > 6) return false;
      c*=2;
      m_chans[ch].cf1 = use_cotab[c];
      m_chans[ch].cf2 = use_cotab[c+1];
    }
    for(ch=0;ch<nch;ch++) m_chans[ch].deltas = getwordsigned(&buf);
    for(ch=0;ch<nch;ch++) m_chans[ch].spl1 = getwordsigned(&buf);
    for(ch=0;ch<nch;ch++) m_chans[ch].spl2 = getwordsigned(&buf);

    int samples_block = samplesPerBlock();

    short *outptr = samplesOut.Add(NULL,samples_block * nch);

    for(ch=0;ch<nch;ch++) *outptr++ = m_chans[ch].spl2;
    for(ch=0;ch<nch;ch++) *outptr++ = m_chans[ch].spl1;

    int x;
    char bstate=0;
    unsigned char lastbyte;
    for (x=2; x < samples_block; x++)
    {
      for(ch=0;ch<nch;ch++)
      {
        int nib;
        if ((bstate^=1)) nib=(lastbyte=*buf++)>>4;
        else nib=lastbyte&0xf;

        int sn=nib;
        if (sn & 8) sn -= 16;

        int pred = ( ((m_chans[ch].spl1 * m_chans[ch].cf1) + 
                       (m_chans[ch].spl2 * m_chans[ch].cf2)) / 256) + 
                        (sn * m_chans[ch].deltas);

        m_chans[ch].spl2 = m_chans[ch].spl1;

        if (pred < -32768) pred=-32768;
        else if (pred > 32767) pred=32767;
    
        *outptr++ = m_chans[ch].spl1 = pred;

        int i= (adtab[nib] * m_chans[ch].deltas) / 256;
        if (i <= 16) m_chans[ch].deltas=16;
        else m_chans[ch].deltas = i;
      }
    }
    return true;
  }

  WDL_adpcm_decode_chanctx *m_chans;
  unsigned char *m_srcbuf;
  int m_srcbuf_valid;

  int m_blockalign,m_nch,m_type,m_bps;
  

};


#endif
