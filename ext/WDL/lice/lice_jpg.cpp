/*
  Cockos WDL - LICE - Lightweight Image Compositing Engine
  Copyright (C) 2007 and later, Cockos Incorporated
  File: lice_jpg.cpp (JPG loading for LICE)
  See lice.h for license and other information
*/

#include <stdio.h>
#include "lice.h"
#include <setjmp.h>
#include "../wdltypes.h"

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

#ifdef _WIN32
static void LICEJPEG_init_source(j_decompress_ptr cinfo) {}
static unsigned char EOI_data[2] = { 0xFF, 0xD9 };
static boolean LICEJPEG_fill_input_buffer(j_decompress_ptr cinfo)
{
  cinfo->src->next_input_byte = EOI_data;
  cinfo->src->bytes_in_buffer = 2;
  return TRUE;
}
static void LICEJPEG_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
  if (num_bytes > 0) 
  {
    if (num_bytes > (long) cinfo->src->bytes_in_buffer)
    {
      num_bytes = (long) cinfo->src->bytes_in_buffer;
    }
    cinfo->src->next_input_byte += (size_t) num_bytes;
    cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
  }
}
static void LICEJPEG_term_source(j_decompress_ptr cinfo) {}
#endif


LICE_IBitmap *LICE_LoadJPGFromResource(HINSTANCE hInst, const char *resid, LICE_IBitmap *bmp)
{
#ifdef _WIN32
  HRSRC hResource = FindResource(hInst, resid, "JPG");
  if(!hResource) return NULL;

  DWORD imageSize = SizeofResource(hInst, hResource);
  if(imageSize < 8) return NULL;

  HGLOBAL res = LoadResource(hInst, hResource);
  const void* pResourceData = LockResource(res);
  if(!pResourceData) return NULL;

  unsigned char *data = (unsigned char *)pResourceData;

  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr={0,};
  JSAMPARRAY buffer;
  int row_stride;

  jerr.pub.error_exit = LICEJPEG_Error;
  jerr.pub.emit_message = LICEJPEG_EmitMsg;
  jerr.pub.output_message = LICEJPEG_OutMsg;
  jerr.pub.format_message = LICEJPEG_FmtMsg;
  jerr.pub.reset_error_mgr = LICEJPEG_reset_error_mgr;

  cinfo.err = &jerr.pub;

  if (setjmp(jerr.setjmp_buffer)) 
  {
    jpeg_destroy_decompress(&cinfo);
    return 0;
  }
  jpeg_create_decompress(&cinfo);

  cinfo.src = (struct jpeg_source_mgr *) (*cinfo.mem->alloc_small) ((j_common_ptr) &cinfo, JPOOL_PERMANENT, sizeof (struct jpeg_source_mgr));
  
  cinfo.src->init_source = LICEJPEG_init_source;
  cinfo.src->fill_input_buffer = LICEJPEG_fill_input_buffer;
  cinfo.src->skip_input_data = LICEJPEG_skip_input_data;
  cinfo.src->resync_to_restart = jpeg_resync_to_restart;	
  cinfo.src->term_source = LICEJPEG_term_source;

  cinfo.src->next_input_byte = data;
  cinfo.src->bytes_in_buffer = imageSize;

  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);

  row_stride = cinfo.output_width * cinfo.output_components;

  buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

  LICE_IBitmap *delbmp = NULL;
  if (bmp) bmp->resize(cinfo.output_width,cinfo.output_height);
  else delbmp = bmp = new WDL_NEW LICE_MemBitmap(cinfo.output_width,cinfo.output_height);

  if (!bmp || bmp->getWidth() != (int)cinfo.output_width || bmp->getHeight() != (int)cinfo.output_height) 
  {
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    delete delbmp;
    return 0;
  }

  LICE_pixel *bmpptr = bmp->getBits();
  int dbmpptr=bmp->getRowSpan();
  if (bmp->isFlipped())
  {
    bmpptr += dbmpptr*(bmp->getHeight()-1);
    dbmpptr=-dbmpptr;
  }

  while (cinfo.output_scanline < cinfo.output_height)
  {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
    jpeg_read_scanlines(&cinfo, buffer, 1);
    /* Assume put_scanline_someplace wants a pointer and sample count. */
//    put_scanline_someplace(buffer[0], row_stride);
    if (cinfo.output_components==3)
    {
      int x;
      for (x = 0; x < (int)cinfo.output_width; x++)
      {
        bmpptr[x]=LICE_RGBA(buffer[0][x*3],buffer[0][x*3+1],buffer[0][x*3+2],255);
      }
    }
    else if (cinfo.output_components==1)
    {
      int x;
      for (x = 0; x < (int)cinfo.output_width; x++)
      {
        int v=buffer[0][x];
        bmpptr[x]=LICE_RGBA(v,v,v,255);
      }
    }
    else
    {
      memset(bmpptr,0,4*cinfo.output_width);
    }
    bmpptr+=dbmpptr;
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);  // we created cinfo.src with some special alloc so I think it gets collected

  return bmp;

#else
  return 0;
#endif
}


LICE_IBitmap *LICE_LoadJPG(const char *filename, LICE_IBitmap *bmp)
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr={{0},};
  JSAMPARRAY buffer;
  int row_stride;

  FILE *fp=NULL;
#if defined(_WIN32) && !defined(WDL_NO_SUPPORT_UTF8)
  #ifdef WDL_SUPPORT_WIN9X
  if (GetVersion()<0x80000000)
  #endif
  {
    WCHAR wf[2048];
    if (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,filename,-1,wf,2048))
      fp = _wfopen(wf,L"rb");
  }
#endif
  if (!fp) fp = WDL_fopenA(filename,"rb");

  if (!fp) return 0;

  jerr.pub.error_exit = LICEJPEG_Error;
  jerr.pub.emit_message = LICEJPEG_EmitMsg;
  jerr.pub.output_message = LICEJPEG_OutMsg;
  jerr.pub.format_message = LICEJPEG_FmtMsg;
  jerr.pub.reset_error_mgr = LICEJPEG_reset_error_mgr;

  cinfo.err = &jerr.pub;

  if (setjmp(jerr.setjmp_buffer)) 
  {
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);
    return 0;
  }
  jpeg_create_decompress(&cinfo);

  jpeg_stdio_src(&cinfo, fp);
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);

  row_stride = cinfo.output_width * cinfo.output_components;

  buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);


  LICE_IBitmap *delbmp = NULL;
  if (bmp) bmp->resize(cinfo.output_width,cinfo.output_height);
  else delbmp = bmp = new WDL_NEW LICE_MemBitmap(cinfo.output_width,cinfo.output_height);

  if (!bmp || bmp->getWidth() != (int)cinfo.output_width || bmp->getHeight() != (int)cinfo.output_height)
  {
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);
    delete delbmp;
    return 0;
  }

  LICE_pixel *bmpptr = bmp->getBits();
  int dbmpptr=bmp->getRowSpan();
  if (bmp->isFlipped())
  {
    bmpptr += dbmpptr*(bmp->getHeight()-1);
    dbmpptr=-dbmpptr;
  }

  while (cinfo.output_scanline < cinfo.output_height) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
    jpeg_read_scanlines(&cinfo, buffer, 1);
    /* Assume put_scanline_someplace wants a pointer and sample count. */
//    put_scanline_someplace(buffer[0], row_stride);
    if (cinfo.output_components==3)
    {
      int x;
      for (x = 0; x < (int)cinfo.output_width; x++)
      {
        bmpptr[x]=LICE_RGBA(buffer[0][x*3],buffer[0][x*3+1],buffer[0][x*3+2],255);
      }
    }
    else if (cinfo.output_components==1)
    {
      int x;
      for (x = 0; x < (int)cinfo.output_width; x++)
      {
        int v=buffer[0][x];
        bmpptr[x]=LICE_RGBA(v,v,v,255);
      }
    }
    else
      memset(bmpptr,0,4*cinfo.output_width);
    bmpptr+=dbmpptr;
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  fclose(fp);

  return bmp;
}

LICE_IBitmap *LICE_LoadJPGFromMemory(const void *data_in, int buflen, LICE_IBitmap *bmp)
{
  if (buflen < 8) return NULL;

  unsigned char *data = (unsigned char *)(void *)data_in;

  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr={{0},};
  JSAMPARRAY buffer;
  int row_stride;

  jerr.pub.error_exit = LICEJPEG_Error;
  jerr.pub.emit_message = LICEJPEG_EmitMsg;
  jerr.pub.output_message = LICEJPEG_OutMsg;
  jerr.pub.format_message = LICEJPEG_FmtMsg;
  jerr.pub.reset_error_mgr = LICEJPEG_reset_error_mgr;

  cinfo.err = &jerr.pub;

  if (setjmp(jerr.setjmp_buffer)) 
  {
    jpeg_destroy_decompress(&cinfo);
    return 0;
  }
  jpeg_create_decompress(&cinfo);

  jpeg_mem_src(&cinfo, data, buflen);
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);

  row_stride = cinfo.output_width * cinfo.output_components;

  buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);


  LICE_IBitmap *delbmp = NULL;
  if (bmp) bmp->resize(cinfo.output_width,cinfo.output_height);
  else delbmp = bmp = new WDL_NEW LICE_MemBitmap(cinfo.output_width,cinfo.output_height);

  if (!bmp || bmp->getWidth() != (int)cinfo.output_width || bmp->getHeight() != (int)cinfo.output_height)
  {
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    delete delbmp;
    return 0;
  }

  LICE_pixel *bmpptr = bmp->getBits();
  int dbmpptr=bmp->getRowSpan();
  if (bmp->isFlipped())
  {
    bmpptr += dbmpptr*(bmp->getHeight()-1);
    dbmpptr=-dbmpptr;
  }

  while (cinfo.output_scanline < cinfo.output_height) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
    jpeg_read_scanlines(&cinfo, buffer, 1);
    /* Assume put_scanline_someplace wants a pointer and sample count. */
//    put_scanline_someplace(buffer[0], row_stride);
    if (cinfo.output_components==3)
    {
      int x;
      for (x = 0; x < (int)cinfo.output_width; x++)
      {
        bmpptr[x]=LICE_RGBA(buffer[0][x*3],buffer[0][x*3+1],buffer[0][x*3+2],255);
      }
    }
    else if (cinfo.output_components==1)
    {
      int x;
      for (x = 0; x < (int)cinfo.output_width; x++)
      {
        int v=buffer[0][x];
        bmpptr[x]=LICE_RGBA(v,v,v,255);
      }
    }
    else
      memset(bmpptr,0,4*cinfo.output_width);
    bmpptr+=dbmpptr;
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  return bmp;
}

class LICE_JPGLoader
{
public:
  _LICE_ImageLoader_rec rec;
  LICE_JPGLoader() 
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
      if (stricmp(p,".jpg")&&stricmp(p,".jpeg")&&stricmp(p,".jfif")) return 0;
    }
    return LICE_LoadJPG(filename,bmpbase);
  }
  static const char *get_extlist()
  {
    return "JPEG files (*.JPG;*.JPEG;*.JFIF)\0*.JPG;*.JPEG;*.JFIF\0";
  }

};

LICE_JPGLoader LICE_jgpldr;
