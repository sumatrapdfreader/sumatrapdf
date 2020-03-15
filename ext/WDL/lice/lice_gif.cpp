/*
  Cockos WDL - LICE - Lightweight Image Compositing Engine
  Copyright (C) 2007 and later, Cockos Incorporated
  File: lice_gif.cpp (GIF loading for LICE)
  See lice.h for license and other information
*/

#include "lice.h"
#include "../heapbuf.h"
#include "../fileread.h"
#include <stdio.h>

extern "C" {

#include "../giflib/gif_lib.h"
int _GifError;
};

static void applyGifFrameToBitmap(LICE_IBitmap *bmp, GifFileType *fp, int transparent_pix, bool clear)
{
  const int width=fp->Image.Width,height=fp->Image.Height;

  LICE_pixel cmap[256];
  GifPixelType _linebuf[2048];
  GifPixelType *linebuf=width > 2048 ? (GifPixelType*)malloc(width*sizeof(GifPixelType)) : _linebuf;
  int y;

  if (bmp)
  {
    y=0;
    if (fp->Image.ColorMap && fp->Image.ColorMap->Colors) for (;y<256 && y < fp->Image.ColorMap->ColorCount;y++)
    {
      GifColorType *ct=fp->Image.ColorMap->Colors+y;
      cmap[y]=LICE_RGBA(ct->Red,ct->Green,ct->Blue,255);
    }

    if (fp->SColorMap && fp->SColorMap->Colors) for (;y<256 && y < fp->SColorMap->ColorCount;y++)
    {
      GifColorType *ct=fp->SColorMap->Colors+y;
      cmap[y]=LICE_RGBA(ct->Red,ct->Green,ct->Blue,255);
    }
    for (;y<256;y++) cmap[y]=0;

    if (clear)
    {
      LICE_pixel col = 0;

      if (fp->SColorMap && fp->SColorMap->Colors && fp->SBackGroundColor >= 0 && fp->SBackGroundColor < fp->SColorMap->ColorCount)
      {
        GifColorType *ct=fp->SColorMap->Colors+fp->SBackGroundColor;
        col = LICE_RGBA(ct->Red,ct->Green,ct->Blue,0);
      }
      else if (fp->SBackGroundColor>=0 && fp->SBackGroundColor<256)
      {
        col = cmap[fp->SBackGroundColor] & LICE_RGBA(255,255,255,0);
      }
      LICE_Clear(bmp,col);
    }
  }

  const int bmp_h = bmp ? bmp->getHeight() : 0;
  const int bmp_w = bmp ? bmp->getWidth() : 0;
  LICE_pixel *bmp_ptr = bmp ? bmp->getBits() : NULL;
  int bmp_span = bmp ? bmp->getRowSpan() : 0;
  if (bmp && bmp->isFlipped() )
  {
    bmp_ptr += (bmp_h-1)*bmp_span;
    bmp_span = -bmp_span;
  }
  const int xpos = fp->Image.Left;

  int skip=0;
  int use_width = width;
  if (xpos < 0) skip = -xpos;

  if (use_width > bmp_w - xpos) use_width = bmp_w - xpos;

  int ystate = 0, ypass=0;
  for (y=0; y < height; y ++)
  {
    if (DGifGetLine(fp,linebuf,width)==GIF_ERROR) break;

    int ypos;
    if (fp->Image.Interlace)
    {
      if (ypass == 0)
      {
        ypos = ystate++ * 8;
        if (ypos >= height) { ypass++; ystate=0; }
      }
      if (ypass == 1)
      {
        ypos = ystate++ * 8 + 4;
        if (ypos >= height) { ypass++; ystate=0; }
      }
      if (ypass == 2)
      {
        ypos = ystate++ * 4 + 2;
        if (ypos >= height) { ypass++; ystate=0; }
      }
      if (ypass==3) ypos = ystate++ * 2 + 1;
    }
    else
    {
      ypos = ystate++;
    }

    ypos += fp->Image.Top;
    if (ypos < 0 || ypos >= bmp_h) continue;

    LICE_pixel *p = bmp_ptr + ypos*bmp_span + xpos;
    int x;
    for (x = skip; x < use_width; x ++) 
    {
      const int a = (int) linebuf[x];
      if (a != transparent_pix) p[x]=cmap[a];
    }
  }
  if (linebuf != _linebuf) free(linebuf);
}

static int readfunc_fh(GifFileType *fh, GifByteType *buf, int sz) 
{ 
  return ((WDL_FileRead*)fh->UserData)->Read(buf,sz);
}


LICE_IBitmap *LICE_LoadGIF(const char *filename, LICE_IBitmap *bmp, int *nframes)
{
  WDL_FileRead fpp(filename,0,65536);

  if (nframes) *nframes=0;

  if (!fpp.IsOpen()) return 0;

  GifFileType *fp=DGifOpen(&fpp, readfunc_fh);
  if (!fp) return 0;
  
  int transparent_pix = -1;
  
  bool had_image = false;
  bool had_delay = false;
  bool need_new_frame = false;

  const bool own_bmp = !bmp;
  GifRecordType RecordType;
  GifByteType *Extension;
  int ExtCode;
  LICE_WrapperBitmap workbm(NULL,0,0,0,false);
  WDL_HeapBuf workbuf(128*1024);
  

  do
  {
    if (DGifGetRecordType(fp, &RecordType) == GIF_ERROR) break;

    switch (RecordType)
    {
      case IMAGE_DESC_RECORD_TYPE:
        if (DGifGetImageDesc(fp) == GIF_ERROR)
        {
          RecordType = TERMINATE_RECORD_TYPE;
          break;
        }

        if (!bmp) bmp=new WDL_NEW LICE_MemBitmap;

        if (!had_image || (nframes && need_new_frame))
        {
          if (had_image)
          {
            // implies nframes
            const int ht = (*nframes+1) * (int)fp->SHeight;
            workbm.m_h = ht;
            workbm.m_w = (int)fp->SWidth;
            workbm.m_span = (workbm.m_w+3)&~3;
            workbm.m_buf = (LICE_pixel *) workbuf.ResizeOK(ht * sizeof(LICE_pixel) * workbm.m_span);
            if (!workbm.m_buf)
            {
              RecordType = TERMINATE_RECORD_TYPE;
              break;
            }
          }
          else
          {
            if (bmp) bmp->resize(fp->SWidth,fp->SHeight);

            if (!bmp || bmp->getWidth() != (int)fp->SWidth || bmp->getHeight() != (int)fp->SHeight)
            {
              RecordType = TERMINATE_RECORD_TYPE;
              break;
            }
          }
          
          if (nframes)
          {
            if (*nframes > 0)
            {
              if (*nframes == 1)
                LICE_Blit(&workbm,bmp,0,0,0,0,bmp->getWidth(),bmp->getHeight(),1.0,LICE_BLIT_MODE_COPY);
            
              LICE_Blit(&workbm,&workbm,0,workbm.getHeight()-(int)fp->SHeight,0,
                        workbm.getHeight()-(int)fp->SHeight*2, workbm.getWidth(), (int)fp->SHeight,1.0f,LICE_BLIT_MODE_COPY);
            }
            
            *nframes += 1;
          }
        }

        if (nframes)
        {
          LICE_SubBitmap tmp(&workbm, 0,workbm.getHeight() - (int) fp->SHeight, workbm.getWidth(), (int)fp->SHeight);
          applyGifFrameToBitmap(&tmp,fp,transparent_pix,!had_image);
        }
        else
          applyGifFrameToBitmap(bmp,fp,transparent_pix,!had_image);

        had_image = true;

        need_new_frame = false;
        transparent_pix = -1;

        if (had_delay) 
        {
          if (!nframes)
          {
            RecordType = TERMINATE_RECORD_TYPE; // finish up if first frame is finished
          }
          else
          {
            need_new_frame = true;
          }
          had_delay = false;
        }
      break;
      case EXTENSION_RECORD_TYPE:
        if (DGifGetExtension(fp, &ExtCode, &Extension) == GIF_ERROR) 
        {
          RecordType = TERMINATE_RECORD_TYPE;
        }
        else
        {
          while (Extension != NULL) 
          {
            if (ExtCode == 0xF9 && *Extension >= 4)
            {
              transparent_pix = -1;
              if (Extension[1]&1)
              {
                transparent_pix = Extension[4];
              }
              if (Extension[2] || Extension[3]) had_delay=true;
            }

            if (DGifGetExtensionNext(fp, &Extension) == GIF_ERROR) 
            {
              RecordType = TERMINATE_RECORD_TYPE;
              break;
            }
          }
        }
      break;
      case TERMINATE_RECORD_TYPE:
      break;
      default: /* Should be traps by DGifGetRecordType. */
      break;
    }
  }
  while (RecordType != TERMINATE_RECORD_TYPE);

  DGifCloseFile(fp);
  
  if (had_image)
  {
    if (workbm.getWidth()) LICE_Copy(bmp,&workbm);
    return bmp;
  }
  
  if (own_bmp) delete bmp;
  return NULL;
}



class LICE_GIFLoader
{
public:
  _LICE_ImageLoader_rec rec;
  LICE_GIFLoader() 
  {
    rec.loadfunc = loadfunc;
    rec.get_extlist = get_extlist;
    rec._next = LICE_ImageLoader_list;
    LICE_ImageLoader_list = &rec;
  }

  static LICE_IBitmap *loadfunc(const char *filename, bool checkFileName, LICE_IBitmap *bmpbase)
  {
    if (checkFileName)
    {
      const char *p=filename;
      while (*p)p++;
      while (p>filename && *p != '\\' && *p != '/' && *p != '.') p--;
      if (stricmp(p,".gif")) return 0;
    }
    return LICE_LoadGIF(filename,bmpbase,NULL);
  }
  static const char *get_extlist()
  {
    return "GIF files (*.GIF)\0*.GIF\0";
  }

};

LICE_GIFLoader LICE_gifldr;





struct lice_gif_read_ctx
{
  WDL_FileRead *fh;
  GifFileType *gif;
  int msecpos;
  int state;
  WDL_FILEREAD_POSTYPE ipos;
};


void *LICE_GIF_LoadEx(const char *filename)
{
  WDL_FileRead *fpp = new WDL_FileRead(filename,0,32768);

  if (!fpp->IsOpen()) 
  {
    delete fpp;
    return 0;
  }

  GifFileType *gif=DGifOpen(fpp, readfunc_fh);
  if (!gif)
  {
    delete fpp;
    return 0;
  }

  lice_gif_read_ctx *ret = (lice_gif_read_ctx*)calloc(sizeof(lice_gif_read_ctx), 1);
  if (!ret)
  {
    DGifCloseFile(gif);
    delete fpp;
    return NULL;
  }
  ret->fh = fpp;
  ret->gif = gif;
  ret->msecpos = 0;
  ret->state = 0;
  ret->ipos = fpp->GetPosition();

  return ret;
}


void LICE_GIF_Close(void *handle)
{
  lice_gif_read_ctx *h = (lice_gif_read_ctx*)handle;
  if (h)
  {
    DGifCloseFile(h->gif);
    delete h->fh;
    free(h);
  }
}
void LICE_GIF_Rewind(void *handle)
{
  lice_gif_read_ctx *h = (lice_gif_read_ctx*)handle;
  if (h)
  {
    h->state = 0;
    h->msecpos = 0;
    h->fh->SetPosition(h->ipos);
    // todo: flush giflib too
  }
}
unsigned int LICE_GIF_GetFilePos(void *handle)
{
  // only used for accounting at the moment, so meh
  lice_gif_read_ctx *h = (lice_gif_read_ctx*)handle;
  if (h && h->fh)
  {
    return (unsigned int) h->fh->GetPosition();
  }

  return 0;
}

int LICE_GIF_UpdateFrame(void *handle, LICE_IBitmap *bm) 
{
  lice_gif_read_ctx *h = (lice_gif_read_ctx*)handle;
  if (!h||h->state<0) return -2;
  GifFileType *fp = h->gif;

  if (bm)
  {
    bm->resize(fp->SWidth, fp->SHeight);
    if (bm->getWidth() != (int)fp->SWidth || bm->getHeight() != (int)fp->SHeight) return -3;
  }

  int has_delay = 0;
  int transparent_pix = -1;
  GifRecordType RecordType;
  GifByteType *Extension;
  int ExtCode;
  do
  {
    if (DGifGetRecordType(fp, &RecordType) == GIF_ERROR) return h->state = -5;
    switch (RecordType)
    {
      case IMAGE_DESC_RECORD_TYPE:
        if (DGifGetImageDesc(fp) == GIF_ERROR)  return h->state = -4;

        applyGifFrameToBitmap(bm,fp,transparent_pix,!h->state);
        h->state += 1;
        h->msecpos += has_delay*10;
      return has_delay*10;

      case EXTENSION_RECORD_TYPE:
        if (DGifGetExtension(fp, &ExtCode, &Extension) == GIF_ERROR) 
        {
          return h->state = -9;
        }
        else
        {
          while (Extension != NULL) 
          {
            if (ExtCode == 0xF9 && *Extension >= 4)
            {
              transparent_pix = -1;
              if (Extension[1]&1)
              {
                transparent_pix = Extension[4];
              }
              has_delay = ((int)Extension[3] << 8) + Extension[2];
            }

            if (DGifGetExtensionNext(fp, &Extension) == GIF_ERROR) return h->state = -8;
          }
        }
      break;
      case TERMINATE_RECORD_TYPE:
      break;
      default: /* Should be traps by DGifGetRecordType. */
      break;
    }
  }
  while (RecordType != TERMINATE_RECORD_TYPE);

  return h->state = -10;
}

