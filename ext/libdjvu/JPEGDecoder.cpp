//C-  -*- C++ -*-
//C- -------------------------------------------------------------------
//C- DjVuLibre-3.5
//C- Copyright (c) 2002  Leon Bottou and Yann Le Cun.
//C- Copyright (c) 2001  AT&T
//C-
//C- This software is subject to, and may be distributed under, the
//C- GNU General Public License, either Version 2 of the license,
//C- or (at your option) any later version. The license should have
//C- accompanied the software or you may obtain a copy of the license
//C- from the Free Software Foundation at http://www.fsf.org .
//C-
//C- This program is distributed in the hope that it will be useful,
//C- but WITHOUT ANY WARRANTY; without even the implied warranty of
//C- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//C- GNU General Public License for more details.
//C- 
//C- DjVuLibre-3.5 is derived from the DjVu(r) Reference Library from
//C- Lizardtech Software.  Lizardtech Software has authorized us to
//C- replace the original DjVu(r) Reference Library notice by the following
//C- text (see doc/lizard2002.djvu and doc/lizardtech2007.djvu):
//C-
//C-  ------------------------------------------------------------------
//C- | DjVu (r) Reference Library (v. 3.5)
//C- | Copyright (c) 1999-2001 LizardTech, Inc. All Rights Reserved.
//C- | The DjVu Reference Library is protected by U.S. Pat. No.
//C- | 6,058,214 and patents pending.
//C- |
//C- | This software is subject to, and may be distributed under, the
//C- | GNU General Public License, either Version 2 of the license,
//C- | or (at your option) any later version. The license should have
//C- | accompanied the software or you may obtain a copy of the license
//C- | from the Free Software Foundation at http://www.fsf.org .
//C- |
//C- | The computer code originally released by LizardTech under this
//C- | license and unmodified by other parties is deemed "the LIZARDTECH
//C- | ORIGINAL CODE."  Subject to any third party intellectual property
//C- | claims, LizardTech grants recipient a worldwide, royalty-free, 
//C- | non-exclusive license to make, use, sell, or otherwise dispose of 
//C- | the LIZARDTECH ORIGINAL CODE or of programs derived from the 
//C- | LIZARDTECH ORIGINAL CODE in compliance with the terms of the GNU 
//C- | General Public License.   This grant only confers the right to 
//C- | infringe patent claims underlying the LIZARDTECH ORIGINAL CODE to 
//C- | the extent such infringement is reasonably necessary to enable 
//C- | recipient to make, have made, practice, sell, or otherwise dispose 
//C- | of the LIZARDTECH ORIGINAL CODE (or portions thereof) and not to 
//C- | any greater extent that may be necessary to utilize further 
//C- | modifications or combinations.
//C- |
//C- | The LIZARDTECH ORIGINAL CODE is provided "AS IS" WITHOUT WARRANTY
//C- | OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//C- | TO ANY WARRANTY OF NON-INFRINGEMENT, OR ANY IMPLIED WARRANTY OF
//C- | MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
//C- +------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma implementation
#endif

#ifdef NEED_JPEG_DECODER

#include "JPEGDecoder.h"

#ifdef __cplusplus
extern "C" {
#endif
#undef HAVE_STDLIB_H
#undef HAVE_STDDEF_H
#include <stdio.h>
#include <jconfig.h>
#include <jpeglib.h>
#include <jerror.h>
#ifdef __cplusplus
}
#endif

#include "ByteStream.h"
#include "GPixmap.h"
#ifdef LIBJPEGNAME
#include "DjVuDynamic.h"
#include "GString.h"
#endif // LIBJPEGNAME



#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


class JPEGDecoder::Impl : public JPEGDecoder
{
public:
  static void jpeg_byte_stream_src(j_decompress_ptr, ByteStream &);
};

extern "C"
{

struct djvu_error_mgr
{
  struct jpeg_error_mgr pub;  /* "public" fields */

  jmp_buf setjmp_buffer;  /* for return to caller */
};

typedef struct djvu_error_mgr * djvu_error_ptr;

METHODDEF(void)
djvu_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a djvu_error_mgr struct, so coerce pointer */
  djvu_error_ptr djvuerr = (djvu_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(djvuerr->setjmp_buffer, 1);
}

}

GP<GPixmap>
JPEGDecoder::decode(ByteStream & bs )
{
  GP<GPixmap> retval=GPixmap::create();
  G_TRY
  {
    decode(bs,*retval);
  } G_CATCH_ALL
  {
    retval=0;
  }
  G_ENDCATCH;
  return retval;
}

void
JPEGDecoder::decode(ByteStream & bs,GPixmap &pix)
{
  struct jpeg_decompress_struct cinfo;

  /* We use our private extension JPEG error handler. */
  struct djvu_error_mgr jerr;

  JSAMPARRAY buffer;    /* Output row buffer */
  int row_stride;   /* physical row width in output buffer */
  int isGrey,i;

  cinfo.err = jpeg_std_error(&jerr.pub);

  jerr.pub.error_exit = djvu_error_exit;

  if (setjmp(jerr.setjmp_buffer))
  {

    jpeg_destroy_decompress(&cinfo);
    G_THROW( ERR_MSG("GPixmap.unk_PPM") );
  }

  jpeg_create_decompress(&cinfo);

  Impl::jpeg_byte_stream_src(&cinfo, bs);

  (void) jpeg_read_header(&cinfo, TRUE);

  jpeg_start_decompress(&cinfo);
  
  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */

  /* JSAMPLEs per row in output buffer */
  row_stride = cinfo.output_width * cinfo.output_components;

  /* Make a one-row-high sample array that will go away when done with image */
  buffer = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

  GP<ByteStream> goutputBlock=ByteStream::create();
  ByteStream &outputBlock=*goutputBlock;
  outputBlock.format("P6\n%d %d\n%d\n",cinfo.output_width, 
                                 cinfo.output_height,255);

  isGrey = ( cinfo.out_color_space == JCS_GRAYSCALE) ? 1 : 0; 

  while (cinfo.output_scanline < cinfo.output_height)
  {
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);

    if ( isGrey == 1 )
    {
      for (i=0; i<row_stride; i++)
      {
        outputBlock.write8((char)buffer[0][i]); 
        outputBlock.write8((char)buffer[0][i]); 
        outputBlock.write8((char)buffer[0][i]); 
      }
    }else
    {
      for (i=0; i<row_stride; i++) 
        outputBlock.write8((char)buffer[0][i]); 
    }
  }

  (void) jpeg_finish_decompress(&cinfo);   

  jpeg_destroy_decompress(&cinfo);
  
  outputBlock.seek(0,SEEK_SET);

  pix.init(outputBlock);
}         

/*** From here onwards code is to make ByteStream as the data
     source for the JPEG library */

extern "C"
{

typedef struct
{
  struct jpeg_source_mgr pub; /* public fields */

  ByteStream * byteStream;    /* source stream */
  JOCTET * buffer;    /* start of buffer */
  boolean start_of_stream;  
} byte_stream_src_mgr;
                

typedef byte_stream_src_mgr * byte_stream_src_ptr; 

#define INPUT_BUF_SIZE   4096

METHODDEF(void)
init_source (j_decompress_ptr cinfo)
{
  byte_stream_src_ptr src = (byte_stream_src_ptr) cinfo->src;

  src->start_of_stream = TRUE;
}

METHODDEF(boolean)
fill_input_buffer (j_decompress_ptr cinfo)
{
  byte_stream_src_ptr src = (byte_stream_src_ptr) cinfo->src;
  size_t nbytes;

  nbytes = src->byteStream->readall(src->buffer, INPUT_BUF_SIZE);

  if (nbytes <= 0)
  {
    if (src->start_of_stream) /* Treat empty input as fatal error */
      ERREXIT(cinfo, JERR_INPUT_EMPTY);
    WARNMS(cinfo, JWRN_JPEG_EOF);
    /* Insert a fake EOI marker */
    src->buffer[0] = (JOCTET) 0xFF;
    src->buffer[1] = (JOCTET) JPEG_EOI;
    nbytes = 2;
  }

  src->pub.next_input_byte = src->buffer;
  src->pub.bytes_in_buffer = nbytes;
  src->start_of_stream = FALSE; 

  return TRUE;
}


METHODDEF(void)
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
  byte_stream_src_ptr src = (byte_stream_src_ptr) cinfo->src;

  if (num_bytes > (long) src->pub.bytes_in_buffer)
  {
    src->byteStream->seek((num_bytes - src->pub.bytes_in_buffer), SEEK_CUR);
    (void) fill_input_buffer(cinfo);
  }else
  {
    src->pub.bytes_in_buffer -= num_bytes;
    src->pub.next_input_byte += num_bytes;
  }
}
                 
METHODDEF(void)
term_source (j_decompress_ptr cinfo)
{
  /* no work necessary here */
}

}

void
JPEGDecoder::Impl::jpeg_byte_stream_src(j_decompress_ptr cinfo,ByteStream &bs)
{
  byte_stream_src_ptr src;

  if (cinfo->src == NULL)
  { /* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *)      
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
          sizeof(byte_stream_src_mgr));
    src = (byte_stream_src_ptr) cinfo->src;
    src->buffer = (JOCTET *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
          INPUT_BUF_SIZE * sizeof(JOCTET));
  }

  src = (byte_stream_src_ptr) cinfo->src;
  src->pub.init_source = init_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->pub.term_source = term_source;
  src->byteStream = &bs;
  src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
  src->pub.next_input_byte = NULL; /* until buffer loaded */
}

#ifdef LIBJPEGNAME
void *
JPEGDecoder::jpeg_lookup(const GUTF8String &name)
{
  static DjVuDynamic lib(GUTF8String(LIBJPEGNAME));
  void *sym=lib.lookup(name);
  if(!sym)
    G_THROW(ERR_MSG("DjVuFile.JPEG_bg2"));
  return sym;
}

jpeg_error_mgr *
JPEGDecoder::jpeg_std_error(jpeg_error_mgr *x)
{
  static void *sym=jpeg_lookup("jpeg_std_error");
  return ((jpeg_error_mgr *(*)(jpeg_error_mgr *))sym)(x);
}

void
JPEGDecoder::jpeg_CreateDecompress(jpeg_decompress_struct *x,int v, size_t s)
{
  static void *sym=jpeg_lookup("jpeg_CreateDecompress");
  ((void (*)(jpeg_decompress_struct *,int,size_t))sym)(x,v,s);
}

void
JPEGDecoder::jpeg_destroy_decompress(j_decompress_ptr x)
{
  static void *sym=jpeg_lookup("jpeg_destroy_decompress");
  ((void (*)(j_decompress_ptr))sym)(x);
}

int
JPEGDecoder::jpeg_read_header(j_decompress_ptr x,boolean y)
{
  static void *sym=jpeg_lookup("jpeg_read_header");
  return ((int (*)(j_decompress_ptr,boolean))sym)(x,y);
}

JDIMENSION
JPEGDecoder::jpeg_read_scanlines(j_decompress_ptr x,JSAMPARRAY y,JDIMENSION z)
{
  static void *sym=jpeg_lookup("jpeg_read_scanlines");
  return ((JDIMENSION (*)(j_decompress_ptr,JSAMPARRAY,JDIMENSION))sym)(x,y,z);
}

boolean
JPEGDecoder::jpeg_finish_decompress(j_decompress_ptr x)
{
  static void *sym=jpeg_lookup("jpeg_finish_decompress");
  return ((boolean (*)(j_decompress_ptr))sym)(x);
}

boolean
JPEGDecoder::jpeg_resync_to_restart(jpeg_decompress_struct *x,int d)
{
  static void *sym=jpeg_lookup("jpeg_resync_to_restart");
  return ((boolean (*)(jpeg_decompress_struct *,int))sym)(x,d);
}

boolean
JPEGDecoder::jpeg_start_decompress(j_decompress_ptr x)
{
  static void *sym=jpeg_lookup("jpeg_start_decompress");
  return ((boolean (*)(j_decompress_ptr))sym)(x);
}

#endif // LIBJPEGNAME


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

#endif

