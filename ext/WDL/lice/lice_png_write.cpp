/*
  Cockos WDL - LICE - Lightweight Image Compositing Engine
  Copyright (C) 2007 and later, Cockos Incorporated
  File: lice_png_write.cpp (PNG saving for LICE)
  See lice.h for license and other information
*/

#include "lice.h"


#include <stdio.h>
#include "../libpng/png.h"


bool LICE_WritePNG(const char *filename, LICE_IBitmap *bmp, bool wantalpha /*=true*/)
{
  if (!bmp || !filename) return false;
  /*
  **  Joshua Teitelbaum 1/1/2008
  **  Gifted to cockos for toe nail clippings.
  **
  ** JF> tweaked some
  */
  png_structp png_ptr=NULL;
  png_infop info_ptr=NULL;
  unsigned char *rowbuf=NULL;

  FILE *fp=NULL;
#if defined(_WIN32) && !defined(WDL_NO_SUPPORT_UTF8)
  #ifdef WDL_SUPPORT_WIN9X
  if (GetVersion()<0x80000000)
  #endif
  {
    WCHAR wf[2048];
    if (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,filename,-1,wf,2048))
      fp = _wfopen(wf,L"wb");
  }
#endif
  if (!fp) fp = fopen(filename,"wb");

  if (fp == NULL) return false;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL, NULL, NULL);

  if (png_ptr == NULL) {
    fclose(fp);
    return false;
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    fclose(fp);
    png_destroy_write_struct(&png_ptr,  (png_infopp)NULL);
    return false;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    /* If we get here, we had a problem reading the file */
    if (fp) fclose(fp);
    fp=0;
    free(rowbuf);
    rowbuf=0;
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return false;
  }


  png_init_io(png_ptr, fp);
  int width=bmp->getWidth();
  int height = bmp->getHeight();

#define BITDEPTH 8
  png_set_IHDR(png_ptr, info_ptr, width, height, BITDEPTH, wantalpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB,
    PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  png_write_info(png_ptr, info_ptr);

  png_set_bgr(png_ptr);

  // kill alpha channel bytes if not wanted
  if (!wantalpha) png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);

  LICE_pixel *ptr=(LICE_pixel *)bmp->getBits();
  int rowspan=bmp->getRowSpan();
  if (bmp->isFlipped()) 
  {
    ptr+=rowspan*(bmp->getHeight()-1); 
    rowspan=-rowspan;
  }


  if (LICE_PIXEL_B != 0 || LICE_PIXEL_G != 1 || LICE_PIXEL_R != 2 || LICE_PIXEL_A != 3)
  {
    rowbuf=(unsigned char *)malloc(width*4);
    int k;
    for (k = 0; k < height; k++)
    {
      int x;
      unsigned char *bout = rowbuf;
      LICE_pixel_chan *bin = (LICE_pixel_chan *) ptr;
      for(x=0;x<width;x++)
      {
        bout[0] = bin[LICE_PIXEL_B];
        bout[1] = bin[LICE_PIXEL_G];
        bout[2] = bin[LICE_PIXEL_R];
        bout[3] = bin[LICE_PIXEL_A];        
        bout+=4;
        bin+=4;
      }
      png_write_row(png_ptr, (unsigned char *)rowbuf);
      ptr += rowspan;
    }
    free(rowbuf);
    rowbuf=0;
  }
  else
  {
    int k;
    for (k = 0; k < height; k++)
    {
      png_write_row(png_ptr, (unsigned char *)ptr);
      ptr += rowspan;
    }
  }

  png_write_end(png_ptr, info_ptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);

  if (fp) fclose(fp);
  fp=0;

  return true;
}
