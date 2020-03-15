#ifndef _LICE_LCF_H_
#define _LICE_LCF_H_

#include "../zlib/zlib.h"
#include "lice.h"

#include "../ptrlist.h"
#include "../queue.h"
class WDL_FileWrite;
class WDL_FileRead;

class LICECaptureCompressor
{
public:
  LICECaptureCompressor(const char *outfn, int w, int h, int interval=20, int bsize_w=128, int bsize_h=16);

  ~LICECaptureCompressor();

  bool IsOpen() { return !!m_file; }
  void OnFrame(LICE_IBitmap *fr, int delta_t_ms);

  WDL_INT64 GetOutSize() { return m_outsize; }
  WDL_INT64 GetInSize() { return m_inbytes; }

private:
  WDL_FileWrite *m_file;
  WDL_INT64 m_outsize,m_inbytes;
  int m_inframes, m_outframes;

  int m_w,m_h,m_interval,m_bsize_w,m_bsize_h;


  struct frameRec
  {
    frameRec(int sz) { data=(unsigned short *)malloc(sz*sizeof(short)); delta_t_ms=0; }
    ~frameRec() { free(data); }
    unsigned short *data; // shorts
    int delta_t_ms; // time (ms) since last frame
  };
  WDL_PtrList<frameRec> m_framelists[2];
  WDL_Queue m_current_block;
  WDL_Queue m_hdrqueue;

  int m_state, m_which,m_outchunkpos,m_numrows,m_numcols;
  int m_current_block_srcsize;

  z_stream m_compstream;

  void BitmapToFrameRec(LICE_IBitmap *fr, frameRec *dest);
  void DeflateBlock(void *data, int data_size, bool flush);
  void AddHdrInt(int a) { m_hdrqueue.AddToLE(&a); }


};


class LICECaptureDecompressor
{
public:
  LICECaptureDecompressor(const char *fn, bool want_seekable=false);
  ~LICECaptureDecompressor();


  bool IsOpen() { return !!m_file; }

  // only supported if want_seekable=true
  int GetLength() { return m_file_length_ms; } // length in ms
  int Seek(unsigned int offset_ms); // return -1 on fail (out of range), or >0 to tell you how far into the frame you seeked (0=exact hit)

  bool NextFrame(); // TRUE if out of frames
  LICE_IBitmap *GetCurrentFrame(); // can return NULL if error
  int GetTimeToNextFrame(); // delta in ms

  int GetWidth(){ return m_curhdr[m_rd_which].w; }
  int GetHeight(){ return m_curhdr[m_rd_which].h; }

  int m_bytes_read; // increases for statistics, caller can clear 

private:
  LICE_MemBitmap m_workbm;

  struct hdrType
  {
    int bpp;
    int w, h;
    int bsize_w, bsize_h;
    int cdata_left;
  } m_curhdr[2];

  int m_rd_which;
  int m_frameidx;

  bool ReadHdr(int whdr);
  bool DecompressBlock(int whdr, double percent=1.0);

  z_stream m_compstream;
  WDL_Queue m_tmp;
  
  WDL_FileRead *m_file;

  unsigned int m_file_length_ms;
  WDL_TypedQueue<unsigned int> m_file_frame_info; //pairs of offset_bytes, offset_ms

  WDL_TypedBuf<int> m_frame_deltas[2];
  WDL_HeapBuf m_decompdata[2];
  WDL_TypedBuf<void *> m_slices; // indexed by [frame][slice]

  void DecodeSlices();
};

#endif
