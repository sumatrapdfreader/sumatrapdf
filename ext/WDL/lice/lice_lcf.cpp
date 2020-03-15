#include <stdio.h>
#include <stdlib.h>

#include "lice_lcf.h"

#include "../filewrite.h"
#include "../fileread.h"


#define LCF_VERSION 0x11CEb001

LICECaptureCompressor::LICECaptureCompressor(const char *outfn, int w, int h, int interval, int bsize_w, int bsize_h)
{
  m_inframes = m_outframes=0;
  m_file = new WDL_FileWrite(outfn,1,512*1024);
  if (!m_file->IsOpen()) { delete m_file; m_file=0; }

  memset(&m_compstream,0,sizeof(m_compstream));
  if (m_file)
  {
    if (deflateInit(&m_compstream,9)!=Z_OK)
    {
      delete m_file; 
      m_file=0;
    }
  }

  m_inbytes=0;
  m_outsize=0;
  m_w=w;
  m_h=h;
  m_interval=interval;
  m_bsize_w=bsize_w;
  m_bsize_h=bsize_h;
  m_state=0;
  m_which=0;
  m_outchunkpos=0;
  m_numcols = (m_w+bsize_w-1) / (bsize_w>0?bsize_w:1);
  if (m_numcols<1) m_numcols=1;
  m_numrows = (m_h+bsize_h-1)/ (bsize_h>0?bsize_h:1);

  m_current_block_srcsize=0;
}

void LICECaptureCompressor::OnFrame(LICE_IBitmap *fr, int delta_t_ms)
{
  if (fr) 
  {
    if (fr->getWidth()!=m_w || fr->getHeight()!=m_h) return;

    frameRec *rec = m_framelists[m_which].Get(m_state);
    if (!rec)
    {
      rec = new frameRec(m_w*m_h);
      m_framelists[m_which].Add(rec);
    }
    rec->delta_t_ms=delta_t_ms;
    BitmapToFrameRec(fr,rec);
    m_state++;
    m_inframes++;
  }


  bool isLastBlock = m_state >= m_interval || !fr;

  if (m_framelists[!m_which].GetSize())
  {
    int compressTo;
    if (isLastBlock) compressTo = m_numcols*m_numrows;
    else compressTo = (m_state * m_numcols*m_numrows) / m_interval;

    frameRec **list = m_framelists[!m_which].GetList();
    int list_size = m_framelists[!m_which].GetSize();

    // compress some data
    int chunkpos = m_outchunkpos;
    while (chunkpos < compressTo)
    {
      int xpos = (chunkpos%m_numcols) * m_bsize_w;
      int ypos = (chunkpos/m_numcols) * m_bsize_h;

      int wid = m_w-xpos;
      int hei = m_h-ypos;
      if (wid > m_bsize_w) wid=m_bsize_w;
      if (hei > m_bsize_h) hei=m_bsize_h;

      int i;
      int rdoffs = xpos + ypos*m_w;
      int rdspan = m_w;

      int repeat_cnt=0;

      for(i=0;i<list_size; i++)
      {
        unsigned short *rd = list[i]->data + rdoffs;
        if (i&&repeat_cnt<255)
        {
          unsigned short *rd1=rd;
          unsigned short *rd2=list[i-1]->data+rdoffs;
          int a=hei;
          while(a--)
          {
            if (memcmp(rd1,rd2,wid*sizeof(short))) break;
            rd1+=rdspan;
            rd2+=rdspan;
          }
          if (a<0)
          {
            repeat_cnt++;
            continue;
          }          
        }

        if (i || repeat_cnt)
        {
          unsigned char c = (unsigned char)repeat_cnt;
          DeflateBlock(&c,1,false);
          repeat_cnt=0;
        }
        int a=hei;
        while (a--)
        {
          DeflateBlock(rd,wid*sizeof(short),false);
          rd+=rdspan;
        }
      }
      if (repeat_cnt)
      {
        unsigned char c = (unsigned char)repeat_cnt;
        DeflateBlock(&c,1,false);
      }

      chunkpos++;
    }
    m_outchunkpos=chunkpos;
  }

  if (isLastBlock)
  {
    if (m_framelists[!m_which].GetSize())
    {
      m_outframes += m_framelists[!m_which].GetSize();

      DeflateBlock(NULL,0,true);

      deflateReset(&m_compstream);

      m_hdrqueue.Clear();
      AddHdrInt(LCF_VERSION);
      AddHdrInt(16);
      AddHdrInt(m_w);
      AddHdrInt(m_h);
      AddHdrInt(m_bsize_w);
      AddHdrInt(m_bsize_h);
      int nf = m_framelists[!m_which].GetSize();
      AddHdrInt(nf);
      int sz = m_current_block.Available();
      AddHdrInt(sz);

      int uncomp_sz = m_current_block_srcsize;
      AddHdrInt(uncomp_sz);

      {
        int x;
        for(x=0;x<nf;x++)
        {
          AddHdrInt(m_framelists[!m_which].Get(x)->delta_t_ms);
        }
      }


      m_file->Write(m_hdrqueue.Get(),m_hdrqueue.Available());
      m_outsize += m_hdrqueue.Available();
      m_file->Write(m_current_block.Get(),sz);
      m_outsize += sz;

      m_current_block.Clear();
      m_current_block_srcsize=0;
    }


    int old_state=m_state;
    m_state=0;
    m_outchunkpos=0;
    m_which=!m_which;


    if (old_state>0 && !fr)
    {
      while (m_framelists[!m_which].GetSize() > old_state)
        m_framelists[!m_which].Delete(m_framelists[!m_which].GetSize()-1,true);

      OnFrame(NULL,0);
    }

    if (!fr)
    {
      m_framelists[0].Empty(true);
      m_framelists[1].Empty(true);
    }
  }
}

void LICECaptureCompressor::BitmapToFrameRec(LICE_IBitmap *fr, frameRec *dest)
{
  unsigned short *outptr = dest->data;
  const LICE_pixel *p = fr->getBits();
  int span = fr->getRowSpan();
  if (fr->isFlipped())
  {
    p+=(fr->getHeight()-1)*span;
    span=-span;
  }
  int h = fr->getHeight(),w=fr->getWidth();
  while (h--)
  {
    int x=w;
    const LICE_pixel *sp = p;
    while (x--)
    {
      LICE_pixel pix = *sp++;
      *outptr++ = (((int)LICE_GETR(pix)&0xF8)>>3) | (((int)LICE_GETG(pix)&0xFC)<<3) | (((int)LICE_GETB(pix)&0xF8)<<8);
    }
    p += span;
  }
}

void LICECaptureCompressor::DeflateBlock(void *data, int data_size, bool flush)
{
  m_current_block_srcsize += data_size;
  m_inbytes += data_size;
  int bytesout=0;

  m_compstream.next_in = (unsigned char *)data;
  m_compstream.avail_in = data_size;
  
  for (;;)
  {
    int add_sz = data_size+32768;
    m_compstream.next_out = (unsigned char *)m_current_block.Add(NULL,add_sz);
    m_compstream.avail_out = add_sz;

    int e = deflate(&m_compstream,flush?Z_FULL_FLUSH:Z_NO_FLUSH);
  
    m_current_block.Add(NULL,-(int)m_compstream.avail_out);

    bytesout+=add_sz-m_compstream.avail_out;


    if (e != Z_OK)
    {
      break;
    }

    if (!m_compstream.avail_in && (!flush || add_sz==(int)m_compstream.avail_out)) break;
  }
  m_outsize += bytesout;
    
}



LICECaptureCompressor::~LICECaptureCompressor()
{
  // process any pending frames
  if (m_file)
  {
    OnFrame(NULL,0);
    deflateEnd(&m_compstream);
  }

  delete m_file;
  m_framelists[0].Empty(true);
  m_framelists[1].Empty(true);
}














//////////////////////////////////////////
///////DECOMPRESS
//////////////////////////////////////////


LICECaptureDecompressor::LICECaptureDecompressor(const char *fn, bool want_seekable) : m_workbm(0,0,1)
{
  m_bytes_read=0;
  m_file_length_ms=0;
  m_rd_which=0;
  m_frameidx=0;
  memset(&m_compstream,0,sizeof(m_compstream));
  memset(&m_curhdr,0,sizeof(m_curhdr));
  m_file = new WDL_FileRead(fn,2,1024*1024);
  if (m_file->IsOpen())
  {
    if (inflateInit(&m_compstream)!=Z_OK)
    {
      delete m_file;
      m_file=0;
    }
    if (m_file)
    {
      m_file_frame_info.Clear();
      m_file_length_ms=0;
      if (want_seekable)
      {
        unsigned int lastpos = 0;
        int first_frame_delay = 0;
        while (ReadHdr(0))
        {
          m_file_frame_info.Add(&lastpos,1);
          unsigned int mst = m_file_length_ms;
          if (m_frame_deltas[0].GetSize()) 
          {
            if (lastpos > 0)
              mst += m_frame_deltas[0].Get()[0]-first_frame_delay; // TOC is by time of first frames, ignore first delay when seeking
            else
              first_frame_delay = m_frame_deltas[0].Get()[0];
          }
          m_file_frame_info.Add(&mst,1);         

          int x;
          for(x=0;x<m_frame_deltas[0].GetSize();x++)
          {
            m_file_length_ms+=m_frame_deltas[0].Get()[x];
          }

          m_file->SetPosition(lastpos = (unsigned int)(m_file->GetPosition() + m_curhdr[0].cdata_left));
        }
      }

      Seek(0);
    }
  }

  if (m_curhdr[m_rd_which].bpp!=16) 
  {
    delete m_file;
    m_file=0;
  }

}

LICECaptureDecompressor::~LICECaptureDecompressor()
{
  inflateEnd(&m_compstream);
  delete m_file;
}

bool LICECaptureDecompressor::NextFrame() // TRUE if out of frames
{
  if (++m_frameidx >= m_frame_deltas[m_rd_which].GetSize())
  {
    m_rd_which=!m_rd_which;

    DecompressBlock(m_rd_which,1.0);
    if (!ReadHdr(!m_rd_which))
      memset(&m_curhdr[!m_rd_which],0,sizeof(m_curhdr[!m_rd_which]));
    m_frameidx=0;
    if (!m_curhdr[m_rd_which].bpp) return false;
    DecodeSlices();
  }
  else
    DecompressBlock(!m_rd_which,m_frameidx/(double)m_frame_deltas[m_rd_which].GetSize());
  return false;
}

int LICECaptureDecompressor::Seek(unsigned int offset_ms)
{

  memset(m_curhdr,0,sizeof(m_curhdr));
  if (!m_file) return -1;

  int rval=0;

  unsigned int seekpos=0;
  m_frameidx=0;
  if (offset_ms>0&&m_file_frame_info.GetSize())
  {
    int x;
    for(x=0;x<m_file_frame_info.GetSize()-2;x+=2)
    {
      if (offset_ms < m_file_frame_info.Get()[x+2+1]) break;
    }
    seekpos = m_file_frame_info.Get()[x];
    offset_ms -= m_file_frame_info.Get()[x+1];
    // figure out the best place to seek
  }
  else 
  {
    if (offset_ms>0) rval=-1;
    offset_ms=0;
  }

  m_rd_which=0;
  m_file->SetPosition(seekpos);
  if (!ReadHdr(m_rd_which)||!DecompressBlock(m_rd_which,1.0)) 
  {
    rval=-1;
    memset(&m_curhdr,0,sizeof(m_curhdr));
  }
  else
  {
    if (offset_ms>0 && rval==0)
    {
      int x;
      for (x = 1; x < m_frame_deltas[m_rd_which].GetSize(); x++)
      {
        if (offset_ms < m_frame_deltas[m_rd_which].Get()[x])
        {
          rval = offset_ms;
          break;
        }
        offset_ms -= m_frame_deltas[m_rd_which].Get()[x];
      }
      m_frameidx=x-1;
    }
    if (!ReadHdr(!m_rd_which))
        memset(&m_curhdr[!m_rd_which],0,sizeof(m_curhdr[!m_rd_which]));

    DecodeSlices();
  }

  return rval;
}


bool LICECaptureDecompressor::ReadHdr(int whdr) // todo: eventually make this read/decompress the next header as it goes
{
  m_tmp.Clear();
  int hdr_sz = (4*9);
  if (m_file->Read(m_tmp.Add(NULL,hdr_sz),hdr_sz)!=hdr_sz) return false;
  m_bytes_read+=hdr_sz;
  int ver=0;
  m_tmp.GetTFromLE(&ver);
  if (ver !=LCF_VERSION) return false;
  m_tmp.GetTFromLE(&m_curhdr[whdr].bpp);
  m_tmp.GetTFromLE(&m_curhdr[whdr].w);
  m_tmp.GetTFromLE(&m_curhdr[whdr].h);
  m_tmp.GetTFromLE(&m_curhdr[whdr].bsize_w);
  m_tmp.GetTFromLE(&m_curhdr[whdr].bsize_h);
  int nf=0;
  m_tmp.GetTFromLE(&nf);
  int csize=0;
  m_tmp.GetTFromLE(&csize);

  int dsize=0;
  m_tmp.GetTFromLE(&dsize);
  
  if (nf<1 || nf > 1024) return false;

  m_frame_deltas[whdr].Resize(nf);

  if (m_frame_deltas[whdr].GetSize()!=nf) return false;

  if (m_file->Read(m_frame_deltas[whdr].Get(),nf*4)!=nf*4) return false;
  m_bytes_read+=nf*4;
  int x;
  for(x=0;x<nf;x++)
  {
    WDL_Queue::WDL_Queue__bswap_buffer(m_frame_deltas[whdr].Get()+x,4);
  }
  m_curhdr[whdr].cdata_left = csize;

  inflateReset(&m_compstream);
  m_compstream.avail_out = dsize;
  m_compstream.next_out = (unsigned char *)m_decompdata[whdr].Resize(dsize,false);
  if (m_decompdata[whdr].GetSize()!=dsize) return false;


  return true;
}
  
bool LICECaptureDecompressor::DecompressBlock(int whdr, double percent)
{
  if (m_compstream.avail_out) 
  {
    unsigned char buf[16384];
    for (;;)
    {
      if (percent<1.0&&m_decompdata[whdr].GetSize())
      {
        double p =  ((m_decompdata[whdr].GetSize()-m_compstream.avail_out)/(double)m_decompdata[whdr].GetSize());
        if (p>percent) break;
      }
      m_compstream.next_in = buf;
      m_compstream.avail_in = m_curhdr[whdr].cdata_left;
      if (m_compstream.avail_in > (int)sizeof(buf)) m_compstream.avail_in=(int)sizeof(buf);

      m_compstream.avail_in = m_file->Read(buf,m_compstream.avail_in);
      m_bytes_read+=m_compstream.avail_in;
      m_curhdr[whdr].cdata_left -= m_compstream.avail_in;

      int e = inflate(&m_compstream,0);
      if (e != Z_OK&&e!=Z_STREAM_END) 
      {
//        printf("inflate error: %d (%d/%d)\n",e,m_compstream.avail_in, m_curhdr[whdr].cdata_left);
        return !m_compstream.avail_out;
      }   
      if (!m_compstream.avail_out && !m_compstream.avail_in) break;
    }
    m_compstream.next_in = NULL;
  }

  return true;
}

int LICECaptureDecompressor::GetTimeToNextFrame()
{
  int nf = m_frame_deltas[m_rd_which].GetSize();
  int fidx = m_frameidx;

  if (fidx<0&& nf) return m_frame_deltas[m_rd_which].Get()[0];

  if (fidx+1 < nf) return m_frame_deltas[m_rd_which].Get()[fidx+1];

  if (m_curhdr[!m_rd_which].bpp && m_frame_deltas[!m_rd_which].GetSize()) 
    return m_frame_deltas[!m_rd_which].Get()[0];

  return 100;
}

void LICECaptureDecompressor::DecodeSlices()
{
  int nf = m_frame_deltas[m_rd_which].GetSize();
  unsigned char *sp = (unsigned char *)m_decompdata[m_rd_which].Get();
  int sp_left = m_decompdata[m_rd_which].GetSize();
  hdrType *hdr = m_curhdr+m_rd_which;
  int ns_x = (hdr->w + hdr->bsize_w-1)/hdr->bsize_w;
  int ns_y = (hdr->h + hdr->bsize_h-1)/hdr->bsize_h;

  int ns_frame = ns_x*ns_y;
  void **slicelist = m_slices.Resize(nf * ns_frame);

  // format of sp is:
  // nf slices
  // each slice is :
  // initial value
  // repeat cnt
  // frame
  // repeat cnt
  // ..

  int ypos,
      toth=hdr->h,
      totw=hdr->w;

  int bytespersample = (hdr->bpp+7)/8;

  for (ypos = 0; ypos < toth; ypos+=hdr->bsize_h)
  {
    int hei = toth-ypos;
    if (hei>hdr->bsize_h) hei=hdr->bsize_h;
    int xpos;
    for (xpos=0; xpos<totw; xpos+=hdr->bsize_w)
    {
      int wid  = totw-xpos;
      if (wid>hdr->bsize_w) wid=hdr->bsize_w;

      int sz1=wid*hei*bytespersample;

      int slicewritepos = 0,i = 0;
      void *lvalid = NULL;
      while (i<nf&&sp_left>0)
      {
        if (lvalid)
        {
          unsigned char c = *sp++;
          sp_left--;
          while (c-->0 && i++ < nf)
          {
            // repeat last slice
            slicelist[slicewritepos] = lvalid;
            slicewritepos += ns_frame;
          }
        }
        if (i<nf)
        {
          lvalid = slicelist[slicewritepos] = sp;
          slicewritepos += ns_frame;
          sp += sz1;
          sp_left -= sz1;
          i++;
        }
      }
      if( sp_left < 0) 
      {
        m_slices.Resize(0);
        return;
      }

      slicelist++;
    }
  }
}


LICE_IBitmap *LICECaptureDecompressor::GetCurrentFrame()
{
  int nf = m_frame_deltas[m_rd_which].GetSize();
  int fidx = m_frameidx;
  hdrType *hdr = m_curhdr+m_rd_which;
  if (fidx >=0 && fidx < nf && m_slices.GetSize() && hdr->bsize_w && hdr->bsize_h)
  {
    int ns_x = (hdr->w + hdr->bsize_w-1)/hdr->bsize_w;
    int ns_y = (hdr->h + hdr->bsize_h-1)/hdr->bsize_h;

    int ns_frame = ns_x*ns_y;

    if (m_slices.GetSize() != ns_frame*nf)
      return NULL; // invalid slices

    if (hdr->bpp == 16)
    {
      m_workbm.resize(hdr->w,hdr->h);
      //unsigned short *
      // format of m_decompdata is:
      // nf frames of slice1, nf frames of slice2, etc

      LICE_pixel *pout = m_workbm.getBits();
      int span = m_workbm.getRowSpan();

      int ypos,
          toth=hdr->h,
          totw=hdr->w;
      void **sliceptr = m_slices.Get() + ns_frame * fidx;

      for (ypos = 0; ypos < toth; ypos+=hdr->bsize_h)
      {
        int hei = toth-ypos;
        if (hei>hdr->bsize_h) hei=hdr->bsize_h;
        int xpos;
        for (xpos=0; xpos<totw; xpos+=hdr->bsize_w)
        {
          int wid  = totw-xpos;
          if (wid>hdr->bsize_w) wid=hdr->bsize_w;

          unsigned short *rdptr = (unsigned short *)*sliceptr;

          sliceptr++;

          LICE_pixel *dest = pout + xpos + ypos*span;
          int y;
          for (y=0;y<hei;y++)
          {
            int x=wid;
            LICE_pixel *wr = dest;
            while (x--)
            {
              unsigned short px = *rdptr++;
              *wr++ = LICE_RGBA((px<<3)&0xF8,(px>>3)&0xFC,(px>>8)&0xF8,255);
            }
            dest+=span;
          }         
        }
      }


      return &m_workbm;
    }
  }
  return NULL;
}