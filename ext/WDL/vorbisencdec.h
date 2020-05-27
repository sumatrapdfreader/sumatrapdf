/*
    WDL - vorbisencdec.h
    Copyright (C) 2005 and later, Cockos Incorporated

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

/*

  This file provides simple interfaces for encoding and decoding of OGG Vorbis data.
  It is a wrapper around what their SDKs expose, which is not too easy to use.

  This stuff is pretty limited and simple, but works, usually.

  For full control you probably want to #define VORBISENC_WANT_FULLCONFIG
  but for compatibility with some older code, it's left disabled here.
 
*/

#ifndef _VORBISENCDEC_H_
#define _VORBISENCDEC_H_

#ifndef OV_EXCLUDE_STATIC_CALLBACKS
#define OV_EXCLUDE_STATIC_CALLBACKS
#endif
#include "vorbis/vorbisenc.h"
#include "vorbis/codec.h"

class VorbisDecoderInterface
{
public:
  virtual ~VorbisDecoderInterface(){}
  virtual int GetSampleRate()=0;
  virtual int GetNumChannels()=0;
  virtual void *DecodeGetSrcBuffer(int srclen)=0;
  virtual void DecodeWrote(int srclen)=0;
  virtual void Reset()=0;
  virtual int Available()=0;
  virtual float *Get()=0;
  virtual void Skip(int amt)=0;
  virtual int GenerateLappingSamples()=0;
};

class VorbisEncoderInterface
{
public:
  virtual ~VorbisEncoderInterface(){}
  virtual void Encode(float *in, int inlen, int advance=1, int spacing=1)=0; // length in sample (PAIRS)
  virtual int isError()=0;
  virtual int Available()=0;
  virtual void *Get()=0;
  virtual void Advance(int)=0;
  virtual void Compact()=0;
  virtual void reinit(int bla=0)=0;
};


#ifndef WDL_VORBIS_INTERFACE_ONLY

#include "../WDL/queue.h"
#include "../WDL/assocarray.h"


class VorbisDecoder : public VorbisDecoderInterface
{
  public:
    VorbisDecoder()
    {
    	packets=0;
	    memset(&oy,0,sizeof(oy));
	    memset(&os,0,sizeof(os));
	    memset(&og,0,sizeof(og));
	    memset(&op,0,sizeof(op));
	    memset(&vi,0,sizeof(vi));
	    memset(&vc,0,sizeof(vc));
	    memset(&vd,0,sizeof(vd));
	    memset(&vb,0,sizeof(vb));


      ogg_sync_init(&oy); /* Now we can read pages */
      m_err=0;
    }
    ~VorbisDecoder()
    {
      ogg_stream_clear(&os);
      vorbis_block_clear(&vb);
      vorbis_dsp_clear(&vd);
	    vorbis_comment_clear(&vc);
      vorbis_info_clear(&vi);

  	  ogg_sync_clear(&oy);
    }

    int GetSampleRate() { return vi.rate; }
    int GetNumChannels() { return vi.channels?vi.channels:1; }

    void *DecodeGetSrcBuffer(int srclen)
    {
		  return ogg_sync_buffer(&oy,srclen);
    }

    void DecodeWrote(int srclen)
    {
      ogg_sync_wrote(&oy,srclen);
  
		  while(ogg_sync_pageout(&oy,&og)>0)
		  {
			  int serial=ogg_page_serialno(&og);
			  if (!packets) ogg_stream_init(&os,serial);
			  else if (serial!=os.serialno)
			  {
				  vorbis_block_clear(&vb);
				  vorbis_dsp_clear(&vd);
				  vorbis_comment_clear(&vc);
				  vorbis_info_clear(&vi);

				  ogg_stream_clear(&os);
				  ogg_stream_init(&os,serial);
				  packets=0;
			  }
			  if (!packets)
			  {
				  vorbis_info_init(&vi);
				  vorbis_comment_init(&vc);
			  }
			  ogg_stream_pagein(&os,&og);
			  while(ogg_stream_packetout(&os,&op)>0)
			  {
				  if (packets<3)
				  {
					  if(vorbis_synthesis_headerin(&vi,&vc,&op)<0) return;
				  }
				  else
				  {
					  float ** pcm;
					  int samples;
					  if(vorbis_synthesis(&vb,&op)==0) vorbis_synthesis_blockin(&vd,&vb);
					  while((samples=vorbis_synthesis_pcmout(&vd,&pcm))>0)
					  {
						  int n,c;


              float *bufmem = m_buf.Add(NULL,samples*vi.channels);

						  if (bufmem) for(n=0;n<samples;n++)
						  {
							  for(c=0;c<vi.channels;c++) *bufmem++=pcm[c][n];
						  }
						  vorbis_synthesis_read(&vd,samples);
					  }
				  }
				  packets++;
				  if (packets==3)
				  {
					  vorbis_synthesis_init(&vd,&vi);
					  vorbis_block_init(&vd,&vb);
				  }
			  }
		  }
    }
    int Available() { return m_buf.Available(); }
    float *Get() { return m_buf.Get(); }

    void Skip(int amt)
    {
      m_buf.Advance(amt);
      m_buf.Compact();
    }
    int GenerateLappingSamples()
    {
      if (vd.pcm_returned<0 ||
          !vd.vi ||
          !vd.vi->codec_setup)
      {
        return 0;
      }
      float ** pcm;
      int samples = vorbis_synthesis_lapout(&vd,&pcm);
      if (samples <= 0) return 0;
      float *bufmem = m_buf.Add(NULL,samples*vi.channels);
      if (bufmem) for(int n=0;n<samples;n++)
      {
        for (int c=0;c<vi.channels;c++) *bufmem++=pcm[c][n];
      }
      return samples;
    }

    void Reset()
    {
      m_buf.Clear();

			vorbis_block_clear(&vb);
			vorbis_dsp_clear(&vd);
			vorbis_comment_clear(&vc);
			vorbis_info_clear(&vi);

			ogg_stream_clear(&os);
			packets=0;
    }

  private:

    WDL_TypedQueue<float> m_buf;

    int m_err;
    int packets;

    ogg_sync_state   oy; /* sync and verify incoming physical bitstream */
    ogg_stream_state os; /* take physical pages, weld into a logical
			    stream of packets */
    ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
    ogg_packet       op; /* one raw packet of data for decode */
  
    vorbis_info      vi; /* struct that stores all the static vorbis bitstream
			    settings */
    vorbis_comment   vc; /* struct that stores all the bitstream user comments */
    vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
    vorbis_block     vb; /* local working space for packet->PCM decode */


} WDL_FIXALIGN;


class VorbisEncoder : public VorbisEncoderInterface
{
public:
#ifdef VORBISENC_WANT_FULLCONFIG
  VorbisEncoder(int srate, int nch, int serno, float qv, int cbr=-1, int minbr=-1, int maxbr=-1,
    const char *encname=NULL, WDL_StringKeyedArray<char*> *metadata=NULL)
#elif defined(VORBISENC_WANT_QVAL)
  VorbisEncoder(int srate, int nch, float qv, int serno, const char *encname=NULL)
#else
  VorbisEncoder(int srate, int nch, int bitrate, int serno, const char *encname=NULL)
#endif
  {
    m_flushmode=false;
    m_ds=0;

    memset(&vi,0,sizeof(vi));
    memset(&vc,0,sizeof(vc));
    memset(&vd,0,sizeof(vd));
    memset(&vb,0,sizeof(vb));

    m_nch=nch;
    vorbis_info_init(&vi);

#ifdef VORBISENC_WANT_FULLCONFIG

    if (cbr > 0)
    {
      m_err=vorbis_encode_init(&vi,nch,srate,maxbr*1000,cbr*1000,minbr*1000);
    }
    else
      m_err=vorbis_encode_init_vbr(&vi,nch,srate,qv);

#else // VORBISENC_WANT_FULLCONFIG

  #ifndef VORBISENC_WANT_QVAL
      float qv=0.0;
      if (nch == 2) bitrate=  (bitrate*5)/8;
      // at least for mono 44khz
      //-0.1 = ~40kbps
      //0.0 == ~64kbps
      //0.1 == 75
      //0.3 == 95
      //0.5 == 110
      //0.75== 140
      //1.0 == 240
      if (bitrate <= 32)
      {
        m_ds=1;
        bitrate*=2;
      }
   
      if (bitrate < 40) qv=-0.1f;
      else if (bitrate < 64) qv=-0.10f + (bitrate-40)*(0.10f/24.0f);
      else if (bitrate < 75) qv=(bitrate-64)*(0.1f/9.0f);
      else if (bitrate < 95) qv=0.1f+(bitrate-75)*(0.2f/20.0f);
      else if (bitrate < 110) qv=0.3f+(bitrate-95)*(0.2f/15.0f);
      else if (bitrate < 140) qv=0.5f+(bitrate-110)*(0.25f/30.0f);
      else qv=0.75f+(bitrate-140)*(0.25f/100.0f);

      if (qv<-0.10f)qv=-0.10f;
      if (qv>1.0f)qv=1.0f;
  #endif // !VORBISENC_WANT_QVAL

      m_err=vorbis_encode_init_vbr(&vi,nch,srate>>m_ds,qv);
#endif // !VORBISENC_WANT_FULLCONFIG

    vorbis_comment_init(&vc);
    if (encname) vorbis_comment_add_tag(&vc,"ENCODER",(char *)encname);

#ifdef VORBISENC_WANT_FULLCONFIG
    if (metadata)
    {
      for (int i=0; i < metadata->GetSize(); ++i)
      {
        const char *key;
        const char *val=metadata->Enumerate(i, &key);
        if (key && val && key[0] && val[0])
        {
          vorbis_comment_add_tag(&vc, key, val);
        }
      }
    }
#endif // VORBISENC_WANT_FULLCONFIG

    vorbis_analysis_init(&vd,&vi);
    vorbis_block_init(&vd,&vb);
    ogg_stream_init(&os,m_ser=serno);

    if (m_err) return;


    reinit(1);
  }

  void reinit(int bla=0)
  {
    if (!bla)
    {
      ogg_stream_clear(&os);
      vorbis_block_clear(&vb);
      vorbis_dsp_clear(&vd);

      vorbis_analysis_init(&vd,&vi);
      vorbis_block_init(&vd,&vb);
      ogg_stream_init(&os,m_ser++); //++?
 
      outqueue.Advance(outqueue.Available());
      outqueue.Compact();
    }


    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;
    vorbis_analysis_headerout(&vd,&vc,&header,&header_comm,&header_code);
    ogg_stream_packetin(&os,&header); /* automatically placed in its own page */
    ogg_stream_packetin(&os,&header_comm);
    ogg_stream_packetin(&os,&header_code);

	  for (;;)
    {
      ogg_page og;
		  int result=ogg_stream_flush(&os,&og);
		  if(result==0)break;
      outqueue.Add(og.header,og.header_len);
		  outqueue.Add(og.body,og.body_len);
	  }
  }

  void Encode(float *in, int inlen, int advance=1, int spacing=1) // length in sample (PAIRS)
  {
    if (m_err) return;

    if (inlen == 0)
    {
      // disable this for now, it fucks us sometimes
      // maybe we should throw some silence in instead?
        vorbis_analysis_wrote(&vd,0);
    }
    else
    {
      inlen >>= m_ds;
      float **buffer=vorbis_analysis_buffer(&vd,inlen);
      int i=0,i2=0;
      
      if (m_nch==1)
      {
        for (i = 0; i < inlen; i ++)
        {
          buffer[0][i]=in[i2];
          i2+=advance<<m_ds;
        }
      }
      else if (m_nch==2)
      {
        for (i = 0; i < inlen; i ++)
        {
          buffer[0][i]=in[i2];
          buffer[1][i]=in[i2+spacing];
          i2+=advance<<m_ds;
        }
      }
      else if (m_nch>2)
      {
        int n=m_nch;
        for (i = 0; i < inlen; i ++)
        {
          int a;
          int i3=i2;
          for(a=0;a<n;a++,i3+=spacing)
            buffer[a][i]=in[i3];
          i2+=advance<<m_ds;
        }
      }
      vorbis_analysis_wrote(&vd,i);
    }

    int eos=0;
    while(vorbis_analysis_blockout(&vd,&vb)==1)
    {
      vorbis_analysis(&vb,NULL);
      vorbis_bitrate_addblock(&vb);
      ogg_packet       op;

      while(vorbis_bitrate_flushpacket(&vd,&op))
      {
	
      	ogg_stream_packetin(&os,&op);

	      while (!eos)
        {
          ogg_page og;
          int result=m_flushmode ? ogg_stream_flush(&os,&og) : ogg_stream_pageout(&os,&og);
		      if(result==0)break;
          outqueue.Add(og.header,og.header_len);
		      outqueue.Add(og.body,og.body_len);
          if(ogg_page_eos(&og)) eos=1;
	      }
      }
    }
  }

  int isError() { return m_err; }

  int Available()
  {
    return outqueue.Available();
  }
  void *Get()
  {
    return outqueue.Get();
  }
  void Advance(int amt)
  {
    outqueue.Advance(amt);
  }

  void Compact()
  {
    outqueue.Compact();
  }

  ~VorbisEncoder()
  {
    ogg_stream_clear(&os);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    if (!m_err) vorbis_info_clear(&vi);
  }

  WDL_Queue outqueue;

private:
  int m_err,m_nch;

  ogg_stream_state os;
  vorbis_info      vi;
  vorbis_comment   vc;
  vorbis_dsp_state vd;
  vorbis_block     vb;
  int m_ser;
  int m_ds;

public:
  bool m_flushmode;
} WDL_FIXALIGN;

#endif//WDL_VORBIS_INTERFACE_ONLY

#endif//_VORBISENCDEC_H_
