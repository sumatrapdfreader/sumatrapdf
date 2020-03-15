/*
  Cockos WDL - LICE - Lightweight Image Compositing Engine
  Copyright (C) 2007 and later, Cockos Incorporated
  File: lice_jpg_write.cpp (JPG writing for LICE)
  See lice.h for license and other information
*/

#include <stdio.h>
#include "lice.h"
#include <setjmp.h>

extern "C" {
#include "../jpeglib/jpeglib.h"
};

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */
  jmp_buf setjmp_buffer;	/* for return to caller */
};
static void LICEJPEG_Error(j_common_ptr cinfo)
{
  longjmp(((my_error_mgr*)cinfo->err)->setjmp_buffer,1);
}
static void LICEJPEG_EmitMsg(j_common_ptr cinfo, int msg_level) { }
static void LICEJPEG_FmtMsg(j_common_ptr cinfo, char *) { }
static void LICEJPEG_OutMsg(j_common_ptr cinfo) { }
static void LICEJPEG_reset_error_mgr(j_common_ptr cinfo)
{
  cinfo->err->num_warnings = 0;
  cinfo->err->msg_code = 0;
}

bool LICE_WriteJPG(const char *filename, LICE_IBitmap *bmp, int quality, bool force_baseline)
{
  if (!bmp || !filename) return false;

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

  if (!fp) return false;

  struct jpeg_compress_struct cinfo;
  struct my_error_mgr jerr={0,};
  jerr.pub.error_exit = LICEJPEG_Error;
  jerr.pub.emit_message = LICEJPEG_EmitMsg;
  jerr.pub.output_message = LICEJPEG_OutMsg;
  jerr.pub.format_message = LICEJPEG_FmtMsg;
  jerr.pub.reset_error_mgr = LICEJPEG_reset_error_mgr;

  cinfo.err = &jerr.pub;
  unsigned char *buf = NULL;

  if (setjmp(jerr.setjmp_buffer)) 
  {
    jpeg_destroy_compress(&cinfo);
    if (fp) fclose(fp);
    free(buf);
    return false;
  }
  jpeg_create_compress(&cinfo);

  jpeg_stdio_dest(&cinfo, fp);

  cinfo.image_width = bmp->getWidth(); 	/* image width and height, in pixels */
  cinfo.image_height = bmp->getHeight();
  cinfo.input_components = 3;		/* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */

  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, quality, !!force_baseline);
  jpeg_start_compress(&cinfo, TRUE);

  buf = (unsigned char *)malloc(cinfo.image_width * 3);
  LICE_pixel_chan *rd = (LICE_pixel_chan *)bmp->getBits();
  int rowspan = bmp->getRowSpan()*4;
  if (bmp->isFlipped())
  {
    rd += rowspan*(bmp->getHeight()-1);
    rowspan=-rowspan;
  }
  while (cinfo.next_scanline < cinfo.image_height) 
  {
    unsigned char *outp=buf;
    LICE_pixel_chan *rdp = rd;
    int x=cinfo.image_width;
    while(x--)
    {
      outp[0] = rdp[LICE_PIXEL_R];
      outp[1] = rdp[LICE_PIXEL_G];
      outp[2] = rdp[LICE_PIXEL_B];
      outp+=3;
      rdp+=4;
    }
    jpeg_write_scanlines(&cinfo, &buf, 1);

    rd+=rowspan;
  }
  free(buf); 
  buf=0;

  jpeg_finish_compress(&cinfo);

  if (fp) fclose(fp);
  fp=0;

  jpeg_destroy_compress(&cinfo);

  return true;
}
