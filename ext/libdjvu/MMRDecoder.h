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

#ifndef _MMRDECODER_H_
#define _MMRDECODER_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


#include "GSmartPointer.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class ByteStream;
class JB2Image;

/** @name MMRDecoder.h
    Files #"MMRDecoder.h"# and #"MMRDecoder.cpp"# implement a 
    CCITT-G4/MMR decoder suitable for use in DjVu.  The main 
    entry point is function \Ref{MMRDecoder::decode}.

    The foreground mask layer of a DjVu file is usually encoded with a
    #"Sjbz"# chunk containing JB2 encoded data (cf. \Ref{JB2Image.h}).
    Alternatively, the qmask layer may be encoded with a #"Smmr"#
    chunk containing a small header followed by MMR encoded data.
    This encoding scheme produces significantly larger files. On the
    other hand, many scanners a printers talk MMR using very efficient
    hardware components.  This is the reason behind the introduction
    of #"Smmr"# chunks.

    The #Smmr# chunk starts by a header containing the following data:
    \begin{verbatim}
        BYTE*3    :  'M' 'M' 'R'
        BYTE      :  0xb000000<s><i>
        INT16     :  <width> (MSB first)
        INT16     :  <height> (MSB first)
    \end{verbatim}

    The header is followed by the encoded data.  Bit 0 of the fourth header
    byte (#<i>#) is similar to TIFF's ``min-is-black'' tag.  This bit is set
    for a reverse video image.  The encoded data can be in either ``regular''
    MMR form or ``striped'' MMR form.  This is indicated by bit 1 of the
    fourth header byte (#<s>#).  This bit is set to indicate ``striped''
    data.  The ``regular'' data format consists of ordinary MMR encoded data.
    The ``striped'' data format consists of one sixteen bit integer (msb
    first) containing the number of rows per stripe, followed by data for each
    stripe as follows.
    \begin{verbatim}
        INT16     :  <rowsperstripe> (MSB first)
        INT32          :  <nbytes1>
        BYTE*<nbytes1> :  <mmrdata1>
        INT32          :  <nbytes2>
        BYTE*<nbytes2> :  <mmrdata2>
          ...
    \end{verbatim}
    Static function \Ref{MMRDecoder::decode_header} decodes the header.  You
    can then create a \Ref{MMRDecoder} object with the flags #inverted# and
    #striped# as obtained when decoding the header.  One can also decode raw
    MMR data by simply initialising a \Ref{MMRDecoder} object with flag
    #striped# unset.  Each call to \Ref{MMRDecoder::scanruns},
    \Ref{MMRDecoder::scanrle} or \Ref{MMRDecoder::scanline} will then decode a
    row of the MMR encoded image.

    Function \Ref{MMRDecoder::decode} is a convenience function for decoding
    the contents of a #"Smmr"# chunk.  It returns a \Ref{JB2Image} divided
    into manageable blocks in order to provide the zooming and panning
    features implemented by class \Ref{JB2Image}.

    @memo
    CCITT-G4/MMR decoder.
    @author
    Parag Deshmukh <parag@sanskrit.lz.att.com> \\
    Leon Bottou <leonb@research.att.com> */
//@{



#define MMRDECODER_HAS_SCANRUNS  1
#define MMRDECODER_HAS_SCANRLE   1



/** Class for G4/MMR decoding.  The simplest way to use this class is
    the static member function \Ref{MMRDecoder::decode}.  This
    function internally creates an instance of #MMRDecoder# which
    processes the MMR data scanline by scanline.  */
class DJVUAPI MMRDecoder : public GPEnabled
{
protected:
  MMRDecoder(const int width, const int height);
  void init(GP<ByteStream> gbs, const bool striped=false);
public:
  /** Main decoding routine that (a) decodes the header using
      #decode_header#, (b) decodes the MMR data using an instance of
      #MMRDecoder#, and returns a new \Ref{JB2Image} composed of tiles
      whose maximal width and height is derived from the size of the
      image. */
  static GP<JB2Image> decode(GP<ByteStream> gbs);

  /// Only decode the header.
  static bool decode_header(ByteStream &inp, 
                            int &width, int &height, int &invert);

public:
  /// Non-virtual destructor.
  ~MMRDecoder();
  /** Create a MMRDecoder object for decoding an image
      of size #width# by #height#. Flag $striped# must be set
      if the image is composed of multiple stripes. */
  static GP<MMRDecoder> create(GP<ByteStream> gbs, 
                               const int width, const int height,
                               const bool striped=false );

  /** Decodes a scanline and returns a pointer to an array of run lengths.
      The returned buffer contains the length of alternative white and black
      runs.  These run lengths sum to the image width. They are followed by
      two zeroes.  The position of these two zeroes is stored in the pointer
      specified by the optional argument #endptr#.  The buffer data should be
      processed before calling this function again. */
  const unsigned short *scanruns(const unsigned short **endptr=0);
  /** Decodes a scanline and returns a pointer to RLE encoded data.  The
      buffer contains the length of the runs for the current line encoded as
      described in \Ref{PNM and RLE file formats}.)  The flag #invert# can be
      used to indicate that the MMR data is encoded in reverse video.  The RLE
      data is followed by two zero bytes.  The position of these two zeroes is
      stored in the pointer specified by the optional argument #endptr#.  The
      buffer data should be processed before calling this function again. This
      is implemented by calling \Ref{MMRDecoder::scanruns}. */
  const unsigned char  *scanrle(const bool invert, 
                                const unsigned char **endptr=0);
#if 0
  /** Decodes a scanline and returns a pointer to an array of #0# or #1# bytes.
      Returns a pointer to the scanline buffer containing one byte per pixel. 
      The buffer data should be processed before calling this function again.
      This is implemented by calling \Ref{MMRDecoder::scanruns}. */
  const unsigned char *scanline();
#endif
 private:
  int width;
  int height;
  int lineno;
  int striplineno;
  int rowsperstrip;
  unsigned char  *line;
  GPBuffer<unsigned char> gline;
  unsigned short *lineruns;
  GPBuffer<unsigned short> glineruns;
  unsigned short *prevruns;
  GPBuffer<unsigned short> gprevruns;
public:
  class VLSource;
  class VLTable;
private:
  GP<VLSource> src;
  GP<VLTable> mrtable;
  GP<VLTable> wtable;
  GP<VLTable> btable;
  friend class VLSource;
  friend class VLTable;
};


//@}


// -----------

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
