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

#ifndef _JPEGDECODER_H_
#define _JPEGDECODER_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif

#ifdef NEED_JPEG_DECODER

#include <stddef.h>
#include <string.h>
#include <setjmp.h>

#include "GSmartPointer.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class ByteStream;
class GPixmap;


/** @name JPEGDecoder.h
    Files #"JPEGDecoder.h"# and #"JPEGDecoder.cpp"# implement an
    interface to the decoding subset of the IJG JPEG library.
    @memo
    Decoding interface to the IJG JPEG library.
    @author
    Parag Deshmukh <parag@sanskrit.lz.att.com> 
*/
//@{

class GUTF8String;

/** This class ensures namespace isolation. */
class JPEGDecoder
{
public:
  class Impl;

  /** Decodes the JPEG formated ByteStream */ 
  static GP<GPixmap> decode(ByteStream & bs);
  static void decode(ByteStream & bs,GPixmap &pix);
#ifdef LIBJPEGNAME
  static void *jpeg_lookup(const GUTF8String &name);
  static jpeg_error_mgr *jpeg_std_error(jpeg_error_mgr *x);
  static void jpeg_CreateDecompress(jpeg_decompress_struct *x,int v, size_t s);
  static void jpeg_destroy_decompress(j_decompress_ptr x);
  static int jpeg_read_header(j_decompress_ptr x,boolean y);
  static JDIMENSION jpeg_read_scanlines(j_decompress_ptr x,JSAMPARRAY y,JDIMENSION z);
  static boolean jpeg_finish_decompress(j_decompress_ptr x);
  static boolean jpeg_resync_to_restart(jpeg_decompress_struct *x,int d);
  static boolean jpeg_start_decompress(j_decompress_ptr x);
#endif // LIBJPEGNAME
};


//@}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

#endif // NEED_JPEG_DECODER
#endif // _JPEGDECODER_H_

