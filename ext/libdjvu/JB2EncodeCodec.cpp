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

// From: Leon Bottou, 1/31/2002
// Lizardtech has split the corresponding cpp file into a decoder and an encoder.
// Only superficial changes.  The meat is mine.

#ifndef NEED_DECODER_ONLY

#include "JB2Image.h"
#include "GBitmap.h"
#include <string.h>


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

////////////////////////////////////////
//// CLASS JB2Codec::Encode:  DECLARATION
////////////////////////////////////////

// This class is accessed via the encode
// functions of class JB2Image


//**** Class JB2Codec
// This class implements the JB2 coder.
// Contains all contextual information for encoding a JB2Image.

class JB2Dict::JB2Codec::Encode : public JB2Dict::JB2Codec
{
public:
  Encode(void);
  void init(const GP<ByteStream> &gbs);
//virtual
  void code(const GP<JB2Image> &jim);
  void code(JB2Image *jim) { const GP<JB2Image> gjim(jim);code(gjim); }
  void code(const GP<JB2Dict> &jim);
  void code(JB2Dict *jim) { const GP<JB2Dict> gjim(jim);code(gjim); }

protected:
  void CodeNum(const int num, const int lo, const int hi, NumContext &ctx);
  void encode_libonly_shape(const GP<JB2Image> &jim, int shapeno);
// virtual
  bool CodeBit(const bool bit, BitContext &ctx);
  void code_comment(GUTF8String &comment);
  void code_record_type(int &rectype);
  int code_match_index(int &index, JB2Dict &jim);
  void code_inherited_shape_count(JB2Dict &jim);
  void code_image_size(JB2Dict &jim);
  void code_image_size(JB2Image &jim);
  void code_absolute_location(JB2Blit *jblt,  int rows, int columns);
  void code_absolute_mark_size(GBitmap &bm, int border=0);
  void code_relative_mark_size(GBitmap &bm, int cw, int ch, int border=0);
  void code_bitmap_directly(GBitmap &bm,const int dw, int dy,
    unsigned char *up2, unsigned char *up1, unsigned char *up0 );
  int get_diff(const int x_diff,NumContext &rel_loc);
  void code_bitmap_by_cross_coding (GBitmap &bm, GBitmap &cbm,
    const int xd2c, const int dw, int dy, int cy,
    unsigned char *up1, unsigned char *up0, unsigned char *xup1, 
    unsigned char *xup0, unsigned char *xdn1 );

private:
  GP<ZPCodec> gzp;
};


////////////////////////////////////////
//// CLASS JB2DICT: IMPLEMENTATION
////////////////////////////////////////

void 
JB2Dict::encode(const GP<ByteStream> &gbs) const
{
  JB2Codec::Encode codec;
  codec.init(gbs);
  codec.code(const_cast<JB2Dict *>(this));
}

////////////////////////////////////////
//// CLASS JB2IMAGE: IMPLEMENTATION
////////////////////////////////////////

void 
JB2Image::encode(const GP<ByteStream> &gbs) const
{
  JB2Codec::Encode codec;
  codec.init(gbs);
  codec.code(const_cast<JB2Image *>(this));
}

////////////////////////////////////////
//// CLASS JB2CODEC : IMPLEMENTATION
////////////////////////////////////////

#define START_OF_DATA                   (0)
#define NEW_MARK                        (1)
#define NEW_MARK_LIBRARY_ONLY           (2)
#define NEW_MARK_IMAGE_ONLY             (3)
#define MATCHED_REFINE                  (4)
#define MATCHED_REFINE_LIBRARY_ONLY     (5)
#define MATCHED_REFINE_IMAGE_ONLY       (6)
#define MATCHED_COPY                    (7)
#define NON_MARK_DATA                   (8)
#define REQUIRED_DICT_OR_RESET          (9)
#define PRESERVED_COMMENT               (10)
#define END_OF_DATA                     (11)

// STATIC DATA MEMBERS

static const int BIGPOSITIVE = 262142;
static const int BIGNEGATIVE = -262143;
static const int CELLCHUNK = 20000;
static const int CELLEXTRA =   500;

// CONSTRUCTOR

JB2Dict::JB2Codec::Encode::Encode(void)
: JB2Dict::JB2Codec(1) {}

void
JB2Dict::JB2Codec::Encode::init(const GP<ByteStream> &gbs)
{
  gzp=ZPCodec::create(gbs,true,true);
}

inline bool
JB2Dict::JB2Codec::Encode::CodeBit(const bool bit, BitContext &ctx)
{
    gzp->encoder(bit?1:0, ctx);
    return bit;
}

void
JB2Dict::JB2Codec::Encode::CodeNum(int num, int low, int high, NumContext &ctx)
{
  if (num < low || num > high)
    G_THROW( ERR_MSG("JB2Image.bad_number") );
  JB2Codec::CodeNum(low,high,&ctx,num);
}

// CODE COMMENTS

void 
JB2Dict::JB2Codec::Encode::code_comment(GUTF8String &comment)
{
  // Encode size
      int size=comment.length();
      CodeNum(size, 0, BIGPOSITIVE, dist_comment_length);
      for (int i=0; i<size; i++) 
        {
          CodeNum(comment[i], 0, 255, dist_comment_byte);
        }
}

// CODE SIMPLE VALUES

inline void 
JB2Dict::JB2Codec::Encode::code_record_type(int &rectype)
{
  CodeNum(rectype, START_OF_DATA, END_OF_DATA, dist_record_type);
}

int 
JB2Dict::JB2Codec::Encode::code_match_index(int &index, JB2Dict &jim)
{
    int match=shape2lib[index];
    CodeNum(match, 0, lib2shape.hbound(), dist_match_index);
    return match;
}

// CODE PAIRS

void
JB2Dict::JB2Codec::Encode::code_inherited_shape_count(JB2Dict &jim)
{
  CodeNum(jim.get_inherited_shape_count(),
    0, BIGPOSITIVE, inherited_shape_count_dist);
}

void 
JB2Dict::JB2Codec::Encode::code_image_size(JB2Dict &jim)
{
  CodeNum(0, 0, BIGPOSITIVE, image_size_dist);
  CodeNum(0, 0, BIGPOSITIVE, image_size_dist);
  JB2Codec::code_image_size(jim);
}

void 
JB2Dict::JB2Codec::Encode::code_image_size(JB2Image &jim)
{
  image_columns = jim.get_width();
  CodeNum(image_columns, 0, BIGPOSITIVE, image_size_dist);
  image_rows = jim.get_height();
  CodeNum(image_rows, 0, BIGPOSITIVE, image_size_dist);
  JB2Codec::code_image_size(jim);
}

inline int
JB2Dict::JB2Codec::Encode::get_diff(int x_diff,NumContext &rel_loc)
{
   CodeNum(x_diff, BIGNEGATIVE, BIGPOSITIVE, rel_loc);
   return x_diff;
}

void 
JB2Dict::JB2Codec::Encode::code_absolute_location(JB2Blit *jblt, int rows, int columns)
{
  // Check start record
  if (!gotstartrecordp)
    G_THROW( ERR_MSG("JB2Image.no_start") );
  // Code TOP and LEFT
  CodeNum(jblt->left+1, 1, image_columns, abs_loc_x);
  CodeNum(jblt->bottom+rows-1+1, 1, image_rows, abs_loc_y);
}

void 
JB2Dict::JB2Codec::Encode::code_absolute_mark_size(GBitmap &bm, int border)
{
  CodeNum(bm.columns(), 0, BIGPOSITIVE, abs_size_x);
  CodeNum(bm.rows(), 0, BIGPOSITIVE, abs_size_y);
}

void 
JB2Dict::JB2Codec::Encode::code_relative_mark_size(GBitmap &bm, int cw, int ch, int border)
{
  CodeNum(bm.columns()-cw, BIGNEGATIVE, BIGPOSITIVE, rel_size_x);
  CodeNum(bm.rows()-ch, BIGNEGATIVE, BIGPOSITIVE, rel_size_y);
}

// CODE BITMAP DIRECTLY

void 
JB2Dict::JB2Codec::Encode::code_bitmap_directly(
  GBitmap &bm,const int dw, int dy,
  unsigned char *up2, unsigned char *up1, unsigned char *up0 )
{
      ZPCodec &zp=*gzp;
      // iterate on rows (encoding)
      while (dy >= 0)
        {
          int context=get_direct_context(up2, up1, up0, 0);
          for (int dx=0;dx < dw;)
            {
              int n = up0[dx++];
              zp.encoder(n, bitdist[context]);
              context=shift_direct_context(context, n, up2, up1, up0, dx);
            }
          // next row
          dy -= 1;
          up2 = up1;
          up1 = up0;
          up0 = bm[dy];
        }
}

// CODE BITMAP BY CROSS CODING

void 
JB2Dict::JB2Codec::Encode::code_bitmap_by_cross_coding (GBitmap &bm, GBitmap &cbm,
  const int xd2c, const int dw, int dy, int cy,
  unsigned char *up1, unsigned char *up0, unsigned char *xup1, 
  unsigned char *xup0, unsigned char *xdn1 )
{
      ZPCodec &zp=*gzp;
      // iterate on rows (encoding)
      while (dy >= 0)
        {
          int context=get_cross_context(up1, up0, xup1, xup0, xdn1, 0);
          for(int dx=0;dx < dw;)
            {
              const int n = up0[dx++];
              zp.encoder(n, cbitdist[context]);
              context=shift_cross_context(context, n,  
                                  up1, up0, xup1, xup0, xdn1, dx);
            }
          // next row
          up1 = up0;
          up0 = bm[--dy];
          xup1 = xup0;
          xup0 = xdn1;
          xdn1 = cbm[(--cy)-1] + xd2c;
        }
}

// CODE JB2DICT

void 
JB2Dict::JB2Codec::Encode::code(const GP<JB2Dict> &gjim)
{
  if(!gjim)
  {
    G_THROW( ERR_MSG("JB2Image.bad_number") );
  }
  JB2Dict &jim=*gjim;
      // -------------------------
      // THIS IS THE ENCODING PART
      // -------------------------
      int firstshape = jim.get_inherited_shape_count();
      int nshape = jim.get_shape_count();
      init_library(jim);
      // Code headers.
      int rectype = REQUIRED_DICT_OR_RESET;
      if (jim.get_inherited_shape_count() > 0)
        code_record(rectype, gjim, 0);
      rectype = START_OF_DATA;
      code_record(rectype, gjim, 0);
      // Code Comment.
      rectype = PRESERVED_COMMENT;
      if (!! jim.comment)
        code_record(rectype, gjim, 0);
      // Encode every shape
      int shapeno;
      DJVU_PROGRESS_TASK(jb2code,"jb2 encode", nshape-firstshape);
      for (shapeno=firstshape; shapeno<nshape; shapeno++)
        {
          DJVU_PROGRESS_RUN(jb2code, (shapeno-firstshape)|0xff);
          // Code shape
          JB2Shape &jshp = jim.get_shape(shapeno);
          rectype=(jshp.parent >= 0)
            ?MATCHED_REFINE_LIBRARY_ONLY:NEW_MARK_LIBRARY_ONLY;
          code_record(rectype, gjim, &jshp);
          add_library(shapeno, jshp);
	  // Check numcoder status
	  if (cur_ncell > CELLCHUNK) 
	    {
	      rectype = REQUIRED_DICT_OR_RESET;
	      code_record(rectype, 0, 0);	      
	    }
        }
      // Code end of data record
      rectype = END_OF_DATA;
      code_record(rectype, gjim, 0); 
      gzp=0;
}

// CODE JB2IMAGE

void 
JB2Dict::JB2Codec::Encode::code(const GP<JB2Image> &gjim)
{
  if(!gjim)
  {
    G_THROW( ERR_MSG("JB2Image.bad_number") );
  }
  JB2Image &jim=*gjim;
      // -------------------------
      // THIS IS THE ENCODING PART
      // -------------------------
      int i;
      init_library(jim);
      int firstshape = jim.get_inherited_shape_count();
      int nshape = jim.get_shape_count();
      int nblit = jim.get_blit_count();
      // Initialize shape2lib 
      shape2lib.resize(0,nshape-1);
      for (i=firstshape; i<nshape; i++)
        shape2lib[i] = -1;
      // Determine shapes that go into library (shapeno>=firstshape)
      //  shape2lib is -2 if used by one blit
      //  shape2lib is -3 if used by more than one blit
      //  shape2lib is -4 if used as a parent
      for (i=0; i<nblit; i++)
        {
          JB2Blit *jblt = jim.get_blit(i);
          int shapeno = jblt->shapeno;
          if (shapeno < firstshape)
            continue;
          if (shape2lib[shapeno] >= -2) 
            shape2lib[shapeno] -= 1;
          shapeno = jim.get_shape(shapeno).parent;
          while (shapeno>=firstshape && shape2lib[shapeno]>=-3)
            {
              shape2lib[shapeno] = -4;
              shapeno = jim.get_shape(shapeno).parent;
            }
        }
      // Code headers.
      int rectype = REQUIRED_DICT_OR_RESET;
      if (jim.get_inherited_shape_count() > 0)
        code_record(rectype, gjim, 0, 0);
      rectype = START_OF_DATA;
      code_record(rectype, gjim, 0, 0);
      // Code Comment.
      rectype = PRESERVED_COMMENT;
      if (!! jim.comment)
        code_record(rectype, gjim, 0, 0);
      // Encode every blit
      int blitno;
      DJVU_PROGRESS_TASK(jb2code,"jb2 encode", nblit);
      for (blitno=0; blitno<nblit; blitno++)
        {
          DJVU_PROGRESS_RUN(jb2code, blitno|0xff);
          JB2Blit *jblt = jim.get_blit(blitno);
          int shapeno = jblt->shapeno;
          JB2Shape &jshp = jim.get_shape(shapeno);
          // Tests if shape exists in library
          if (shape2lib[shapeno] >= 0)
            {
              int rectype = MATCHED_COPY;
              code_record(rectype, gjim, 0, jblt);
            }
          // Avoid coding null shapes/blits
          else if (jshp.bits) 
            {
              // Make sure all parents have been coded
              if (jshp.parent>=0 && shape2lib[jshp.parent]<0)
                encode_libonly_shape(gjim, jshp.parent);
              // Allocate library entry when needed
#define LIBRARY_CONTAINS_ALL
              int libraryp = 0;
#ifdef LIBRARY_CONTAINS_MARKS // baseline
              if (jshp.parent >= -1)
                libraryp = 1;
#endif
#ifdef LIBRARY_CONTAINS_SHARED // worse             
              if (shape2lib[shapeno] <= -3)
                libraryp = 1;
#endif
#ifdef LIBRARY_CONTAINS_ALL // better
              libraryp = 1;
#endif
              // Test all blit cases
              if (jshp.parent<-1 && !libraryp)
                {
                  int rectype = NON_MARK_DATA;
                  code_record(rectype, gjim, &jshp, jblt);
                }
              else if (jshp.parent < 0)
                {
                  int rectype = (libraryp ? NEW_MARK : NEW_MARK_IMAGE_ONLY);
                  code_record(rectype, gjim, &jshp, jblt);
                }
              else 
                {
                  int rectype = (libraryp ? MATCHED_REFINE : MATCHED_REFINE_IMAGE_ONLY);
                  code_record(rectype, gjim, &jshp, jblt);
                }
              // Add shape to library
              if (libraryp) 
                add_library(shapeno, jshp);
            }
	  // Check numcoder status
	  if (cur_ncell > CELLCHUNK) 
	    {
	      rectype = REQUIRED_DICT_OR_RESET;
	      code_record(rectype, 0, 0);
	    }
        }
      // Code end of data record
      rectype = END_OF_DATA;
      code_record(rectype, gjim, 0, 0); 
      gzp=0;
}

////////////////////////////////////////
//// HELPERS
////////////////////////////////////////

void 
JB2Dict::JB2Codec::Encode::encode_libonly_shape(
  const GP<JB2Image> &gjim, int shapeno )
{
  if(!gjim)
  {
    G_THROW( ERR_MSG("JB2Image.bad_number") );
  }
  JB2Image &jim=*gjim;
  // Recursively encode parent shape
  JB2Shape &jshp = jim.get_shape(shapeno);
  if (jshp.parent>=0 && shape2lib[jshp.parent]<0)
    encode_libonly_shape(gjim, jshp.parent);
  // Test that library shape must be encoded
  if (shape2lib[shapeno] < 0)
    {
      // Code library entry
      int rectype=(jshp.parent >= 0)
            ?NEW_MARK_LIBRARY_ONLY:MATCHED_REFINE_LIBRARY_ONLY;
      code_record(rectype, gjim, &jshp, 0);      
      // Add shape to library
      add_library(shapeno, jshp);
      // Check numcoder status
      if (cur_ncell > CELLCHUNK) 
	{
	  rectype = REQUIRED_DICT_OR_RESET;
	  code_record(rectype, 0, 0);
	}
    }
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

#endif /* NEED_DECODER_ONLY */

