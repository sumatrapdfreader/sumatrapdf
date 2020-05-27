/*
  Cockos WDL - LICE - Lightweight Image Compositing Engine
  Copyright (C) 2007 and later, Cockos Incorporated
  File: lice_png.cpp (PNG loading for LICE)
  See lice.h for license and other information
*/

#include "lice.h"

#include "../wdltypes.h"

#include <stdio.h>
#include "../libpng/png.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h> // for loading images from embedded resource 
#endif


LICE_IBitmap *LICE_LoadPNG(const char *filename, LICE_IBitmap *bmp)
{
  FILE *fp = NULL;
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

  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); 
  if(!png_ptr) 
  {
    fclose(fp);
    return 0;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr); 
  if(!info_ptr)
  {
    png_destroy_read_struct(&png_ptr, NULL, NULL); 
    fclose(fp);
    return 0;
  }
  
  if (setjmp(png_jmpbuf(png_ptr)))
  { 
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL); 
    fclose(fp);
    return 0;
  }

  png_init_io(png_ptr, fp); 

  png_read_info(png_ptr, info_ptr);

  unsigned int width, height;
  int bit_depth, color_type, interlace_type, compression_type, filter_method;
  png_get_IHDR(png_ptr, info_ptr, &width, &height,
       &bit_depth, &color_type, &interlace_type,
       &compression_type, &filter_method);

  //convert whatever it is to RGBA
  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) 
    png_set_expand_gray_1_2_4_to_8(png_ptr);

  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) 
  {
    png_set_tRNS_to_alpha(png_ptr);
    color_type |= PNG_COLOR_MASK_ALPHA;
  }

  if (bit_depth == 16)
    png_set_strip_16(png_ptr);

  if (bit_depth < 8)
    png_set_packing(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png_ptr);

  if (color_type & PNG_COLOR_MASK_ALPHA)
    png_set_swap_alpha(png_ptr);
  else
    png_set_filler(png_ptr, 0xff, PNG_FILLER_BEFORE);

  LICE_IBitmap *delbmp = NULL;

  if (bmp) bmp->resize(width,height);
  else delbmp = bmp = new WDL_NEW LICE_MemBitmap(width,height);

  if (!bmp || bmp->getWidth() != (int)width || bmp->getHeight() != (int)height) 
  {
    delete delbmp;
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    fclose(fp);
    return 0;
  }

  unsigned char **row_pointers=(unsigned char **)malloc(height*sizeof(unsigned char *));;
  LICE_pixel *srcptr = bmp->getBits();
  int dsrcptr=bmp->getRowSpan();
  if (bmp->isFlipped())
  {
    srcptr += dsrcptr*(bmp->getHeight()-1);
    dsrcptr=-dsrcptr;
  }
  unsigned int i;
  for(i=0;i<height;i++)
  {
    row_pointers[i]=(unsigned char *)srcptr;
    srcptr+=dsrcptr;
  }
  png_read_image(png_ptr, row_pointers);
  png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
  fclose(fp);

  #if !(LICE_PIXEL_A == 0 && LICE_PIXEL_R == 1 && LICE_PIXEL_G == 2 && LICE_PIXEL_B == 3)
  for(i=0;i<height;i++)
  {
    unsigned char *bp = row_pointers[i];
    int j=width;
    while (j-->0)
    {
      unsigned char a = bp[0];
      unsigned char r = bp[1];
      unsigned char g = bp[2];
      unsigned char b = bp[3];
      ((LICE_pixel*)bp)[0] = LICE_RGBA(r,g,b,a);
      bp+=4;
    }
  }
  #endif
  free(row_pointers);
  
  return bmp;
}

typedef struct 
{
  unsigned char *data;
  int len;
} pngReadStruct;

static void staticPngReadFunc(png_structp png_ptr, png_bytep data, png_size_t length)
{
  pngReadStruct *readStruct = (pngReadStruct *)png_get_io_ptr(png_ptr);
  memset(data, 0, length);

  int l = (int)length;
  if (l > readStruct->len) l = readStruct->len;
  memcpy(data, readStruct->data, l);
  readStruct->data += l;
  readStruct->len -= l;
}

#ifndef _WIN32
LICE_IBitmap *LICE_LoadPNGFromNamedResource(const char *name, LICE_IBitmap *bmp) // returns a bitmap (bmp if nonzero) on success
{
  char buf[2048];
  buf[0]=0;
  if (strlen(name)>400) return NULL; // max name for this is 400 chars
  
#ifdef __APPLE__  
  CFBundleRef bund = CFBundleGetMainBundle();
  if (bund) 
  {
    CFURLRef url=CFBundleCopyBundleURL(bund);
    if (url)
    {
      CFURLGetFileSystemRepresentation(url,true,(UInt8*)buf,sizeof(buf)-512);
      CFRelease(url);
    }
  }
  if (!buf[0]) return 0;
  strcat(buf,"/Contents/Resources/");
#else  
  int sz = readlink("/proc/self/exe", buf, sizeof(buf)-512);  
  if (sz < 1)
  {
    static char tmp;
    // this will likely not work if the program was launched with a relative path 
    // and the cwd has changed, but give it a try anyway
    Dl_info inf={0,};
    if (dladdr(&tmp,&inf) && inf.dli_fname) 
      sz = (int) strlen(inf.dli_fname);
    else
      sz = 0;
  }

  if ((unsigned int)sz >= sizeof(buf)-512) sz = sizeof(buf)-512-1;
  buf[sz]=0;
  char *p = buf;
  while (*p) p++;
  while (p > buf && *p != '/') p--;
  *p=0;
  strcat(buf,"/Resources/");
#endif // !__APPLE__
  
  strcat(buf,name);
  return LICE_LoadPNG(buf,bmp);
}
#endif

LICE_IBitmap *LICE_LoadPNGFromMemory(const void *data_in, int buflen, LICE_IBitmap *bmp)
{
  if (buflen<8) return NULL;
  unsigned char *data = (unsigned char *)(void*)data_in;
  if(png_sig_cmp(data, 0, 8)) return NULL;

  pngReadStruct readStruct = {data, buflen};

  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); 
  if(!png_ptr) 
  {
    return 0;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr); 
  if(!info_ptr)
  {
    png_destroy_read_struct(&png_ptr, NULL, NULL); 
    return 0;
  }
  
  if (setjmp(png_jmpbuf(png_ptr)))
  { 
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL); 
    return 0;
  }

  png_set_read_fn(png_ptr, &readStruct, staticPngReadFunc);

  png_read_info(png_ptr, info_ptr);

  unsigned int width, height;
  int bit_depth, color_type, interlace_type, compression_type, filter_method;
  png_get_IHDR(png_ptr, info_ptr, &width, &height,
       &bit_depth, &color_type, &interlace_type,
       &compression_type, &filter_method);

  //convert whatever it is to RGBA
  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) 
    png_set_expand_gray_1_2_4_to_8(png_ptr);

  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) 
  {
    png_set_tRNS_to_alpha(png_ptr);
    color_type |= PNG_COLOR_MASK_ALPHA;
  }

  if (bit_depth == 16)
    png_set_strip_16(png_ptr);

  if (bit_depth < 8)
    png_set_packing(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png_ptr);

  if (color_type & PNG_COLOR_MASK_ALPHA)
    png_set_swap_alpha(png_ptr);
  else
    png_set_filler(png_ptr, 0xff, PNG_FILLER_BEFORE);

  LICE_IBitmap *delbmp = NULL;
  
  if (bmp) bmp->resize(width,height);
  else delbmp = bmp = new WDL_NEW LICE_MemBitmap(width,height);
  if (!bmp || bmp->getWidth() != (int)width || bmp->getHeight() != (int)height) 
  {
    delete delbmp;
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    return 0;
  }

  unsigned char **row_pointers=(unsigned char **)malloc(height*sizeof(unsigned char *));;
  LICE_pixel *srcptr = bmp->getBits();
  int dsrcptr=bmp->getRowSpan();
  unsigned int i;
  for(i=0;i<height;i++)
  {
    row_pointers[i]=(unsigned char *)srcptr;
    srcptr+=dsrcptr;
  }
  png_read_image(png_ptr, row_pointers);
  png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);

  //put shit in correct order
  #if !(LICE_PIXEL_A == 0 && LICE_PIXEL_R == 1 && LICE_PIXEL_G == 2 && LICE_PIXEL_B == 3)
  for(i=0;i<height;i++)
  {
    unsigned char *bp = row_pointers[i];
    int j=width;
    while (j-->0)
    {
      unsigned char a = bp[0];
      unsigned char r = bp[1];
      unsigned char g = bp[2];
      unsigned char b = bp[3];
      ((LICE_pixel*)bp)[0] = LICE_RGBA(r,g,b,a);
      bp+=4;
    }
  }
  #endif
  free(row_pointers);
  return bmp;  
}
LICE_IBitmap *LICE_LoadPNGFromResource(HINSTANCE hInst, const char *resid, LICE_IBitmap *bmp)
{
#ifdef _WIN32
  HRSRC hResource = FindResource(hInst, resid, "PNG");
  if(!hResource) return NULL;

  DWORD imageSize = SizeofResource(hInst, hResource);
  if(imageSize < 8) return NULL;

  HGLOBAL res = LoadResource(hInst, hResource);
  const void* pResourceData = LockResource(res);
  if(!pResourceData) return NULL;

  LICE_IBitmap * ret = LICE_LoadPNGFromMemory(pResourceData,imageSize,bmp);

  // todo : cleanup res??

  return ret;
#else
  return 0;
#endif
}


class LICE_PNGLoader
{
public:
  _LICE_ImageLoader_rec rec;
  LICE_PNGLoader() 
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
      if (stricmp(p,".png")) return 0;
    }
    return LICE_LoadPNG(filename,bmpbase);
  }
  static const char *get_extlist()
  {
    return "PNG files (*.PNG)\0*.PNG\0";
  }

};

LICE_PNGLoader LICE_pngldr;
