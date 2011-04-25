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

#include "JB2Image.h"
#include "GThreads.h"
#include "GRect.h"
#include "GBitmap.h"
#include <string.h>


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

////////////////////////////////////////
//// CLASS JB2Codec::Decode:  DECLARATION
////////////////////////////////////////

// This class is accessed via the decode
// functions of class JB2Image


//**** Class JB2Codec
// This class implements the JB2 decoder.
// Contains all contextual information for decoding a JB2Image.

class JB2Dict::JB2Codec::Decode : public JB2Dict::JB2Codec
{
public:
  Decode(void);
  void init(const GP<ByteStream> &gbs);
// virtual
  void code(const GP<JB2Image> &jim);
  void code(JB2Image *jim) {const GP<JB2Image> gjim(jim);code(gjim);}
  void code(const GP<JB2Dict> &jim);
  void code(JB2Dict *jim) {const GP<JB2Dict> gjim(jim);code(gjim);}
  void set_dict_callback(JB2DecoderCallback *cb, void *arg);
protected:
  int CodeNum(const int lo, const int hi, NumContext &ctx);

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
  void code_bitmap_by_cross_coding (GBitmap &bm, GBitmap &cbm,
    const int xd2c, const int dw, int dy, int cy,
    unsigned char *up1, unsigned char *up0, unsigned char *xup1, 
    unsigned char *xup0, unsigned char *xdn1 );
  int get_diff(const int x_diff,NumContext &rel_loc);

private:
  GP<ZPCodec> gzp;
  JB2DecoderCallback *cbfunc;
  void *cbarg;
};

////////////////////////////////////////
//// CLASS JB2DICT: IMPLEMENTATION
////////////////////////////////////////


JB2Dict::JB2Dict()
  : inherited_shapes(0)
{
}

void
JB2Dict::init()
{
  inherited_shapes = 0;
  inherited_dict = 0;
  shapes.empty();
}

JB2Shape &
JB2Dict::get_shape(const int shapeno)
{
  JB2Shape *retval;
  if(shapeno >= inherited_shapes)
  {
    retval=&shapes[shapeno - inherited_shapes];
  }else if(inherited_dict)
  {
    retval=&(inherited_dict->get_shape(shapeno));
  }else
  {
    G_THROW( ERR_MSG("JB2Image.bad_number") );
  }
  return *retval;
}

const JB2Shape &
JB2Dict::get_shape(const int shapeno) const
{
  const JB2Shape *retval;
  if(shapeno >= inherited_shapes)
  {
    retval=&shapes[shapeno - inherited_shapes];
  }else if(inherited_dict)
  {
    retval=&(inherited_dict->get_shape(shapeno));
  }else
  {
    G_THROW( ERR_MSG("JB2Image.bad_number") );
  }
  return *retval;
}

void 
JB2Dict::set_inherited_dict(const GP<JB2Dict> &dict)
{
  if (shapes.size() > 0)
    G_THROW( ERR_MSG("JB2Image.cant_set") );
  if (inherited_dict)
    G_THROW( ERR_MSG("JB2Image.cant_change") );
  inherited_dict = dict; 
  inherited_shapes = dict->get_shape_count();
  // Make sure that inherited bitmaps are marked as shared
  for (int i=0; i<inherited_shapes; i++)
    {
      JB2Shape &jshp = dict->get_shape(i);
      if (jshp.bits) jshp.bits->share();
    }
}

void
JB2Dict::compress()
{
  for (int i=shapes.lbound(); i<=shapes.hbound(); i++)
    shapes[i].bits->compress();
}

unsigned int
JB2Dict::get_memory_usage() const
{
  unsigned int usage = sizeof(JB2Dict);
  usage += sizeof(JB2Shape) * shapes.size();
  for (int i=shapes.lbound(); i<=shapes.hbound(); i++)
    if (shapes[i].bits)
      usage += shapes[i].bits->get_memory_usage();
  return usage;
}

int  
JB2Dict::add_shape(const JB2Shape &shape)
{
  if (shape.parent >= get_shape_count())
    G_THROW( ERR_MSG("JB2Image.bad_parent_shape") );
  int index = shapes.size();
  shapes.touch(index);
  shapes[index] = shape;
  return index + inherited_shapes;
}

void 
JB2Dict::decode(const GP<ByteStream> &gbs, JB2DecoderCallback *cb, void *arg)
{
  init();
  JB2Codec::Decode codec;
  codec.init(gbs);
  codec.set_dict_callback(cb,arg);
  codec.code(this);
}



////////////////////////////////////////
//// CLASS JB2IMAGE: IMPLEMENTATION
////////////////////////////////////////


JB2Image::JB2Image(void)
  : width(0), height(0), reproduce_old_bug(false)
{
}

void
JB2Image::init(void)
{
  width = height = 0;
  blits.empty();
  JB2Dict::init();
}

unsigned int
JB2Image::get_memory_usage() const
{
  unsigned int usage = JB2Dict::get_memory_usage();
  usage += sizeof(JB2Image) - sizeof(JB2Dict);
  usage += sizeof(JB2Blit) * blits.size();
  return usage;
}

void 
JB2Image::set_dimension(int awidth, int aheight)
{
  width = awidth;
  height = aheight;
}

int  
JB2Image::add_blit(const JB2Blit &blit)
{
  if (blit.shapeno >= (unsigned int)get_shape_count())
    G_THROW( ERR_MSG("JB2Image.bad_shape") );
  int index = blits.size();
  blits.touch(index);
  blits[index] = blit;
  return index;
}

GP<GBitmap>
JB2Image::get_bitmap(int subsample, int align) const
{
  if (width==0 || height==0)
    G_THROW( ERR_MSG("JB2Image.cant_create") );
  int swidth = (width + subsample - 1) / subsample;
  int sheight = (height + subsample - 1) / subsample;
  int border = ((swidth + align - 1) & ~(align - 1)) - swidth;
  GP<GBitmap> bm = GBitmap::create(sheight, swidth, border);
  bm->set_grays(1+subsample*subsample);
  for (int blitno = 0; blitno < get_blit_count(); blitno++)
    {
      const JB2Blit *pblit = get_blit(blitno);
      const JB2Shape  &pshape = get_shape(pblit->shapeno);
      if (pshape.bits)
        bm->blit(pshape.bits, pblit->left, pblit->bottom, subsample);
    }
  return bm;
}

GP<GBitmap>
JB2Image::get_bitmap(const GRect &rect, int subsample, int align, int dispy) const
{
  if (width==0 || height==0)
    G_THROW( ERR_MSG("JB2Image.cant_create") );
  int rxmin = rect.xmin * subsample;
  int rymin = rect.ymin * subsample;
  int swidth = rect.width();
  int sheight = rect.height();
  int border = ((swidth + align - 1) & ~(align - 1)) - swidth;
  GP<GBitmap> bm = GBitmap::create(sheight, swidth, border);
  bm->set_grays(1+subsample*subsample);
  for (int blitno = 0; blitno < get_blit_count(); blitno++)
    {
      const JB2Blit *pblit = get_blit(blitno);
      const JB2Shape  &pshape = get_shape(pblit->shapeno);
      if (pshape.bits)
        bm->blit(pshape.bits, pblit->left-rxmin, pblit->bottom-rymin+dispy, subsample);
    }
  return bm;
}

void 
JB2Image::decode(const GP<ByteStream> &gbs, JB2DecoderCallback *cb, void *arg)
{
  init();
  JB2Codec::Decode codec;
  codec.init(gbs);
  codec.set_dict_callback(cb,arg);
  codec.code(this);
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

JB2Dict::JB2Codec::Decode::Decode(void)
: JB2Dict::JB2Codec(0), cbfunc(0), cbarg(0) {}

void
JB2Dict::JB2Codec::Decode::init(const GP<ByteStream> &gbs)
{
  gzp=ZPCodec::create(gbs,false,true);
}

JB2Dict::JB2Codec::JB2Codec(const bool xencoding)
  : encoding(xencoding),
    cur_ncell(0),
    gbitcells(bitcells,CELLCHUNK+CELLEXTRA),
    gleftcell(leftcell,CELLCHUNK+CELLEXTRA),
    grightcell(rightcell,CELLCHUNK+CELLEXTRA),
    refinementp(false),
    gotstartrecordp(0),
    dist_comment_byte(0),
    dist_comment_length(0),
    dist_record_type(0),
    dist_match_index(0),
    dist_refinement_flag(0),
    abs_loc_x(0),
    abs_loc_y(0),
    abs_size_x(0),
    abs_size_y(0),
    image_size_dist(0),
    inherited_shape_count_dist(0),
    offset_type_dist(0),
    rel_loc_x_current(0),
    rel_loc_x_last(0),
    rel_loc_y_current(0),
    rel_loc_y_last(0),
    rel_size_x(0),
    rel_size_y(0)
{
  memset(bitdist, 0, sizeof(bitdist));
  memset(cbitdist, 0, sizeof(cbitdist));
  // Initialize numcoder
  bitcells[0] = 0; // dummy cell
  leftcell[0] = rightcell[0] = 0;
  cur_ncell = 1;
}

JB2Dict::JB2Codec::~JB2Codec() {}

void 
JB2Dict::JB2Codec::reset_numcoder()
{
  dist_comment_byte = 0;
  dist_comment_length = 0;
  dist_record_type = 0;
  dist_match_index = 0;
  abs_loc_x = 0;
  abs_loc_y = 0;
  abs_size_x = 0;
  abs_size_y = 0;
  image_size_dist = 0;
  inherited_shape_count_dist = 0;
  rel_loc_x_current = 0;
  rel_loc_x_last = 0;
  rel_loc_y_current = 0;
  rel_loc_y_last = 0;
  rel_size_x = 0;
  rel_size_y = 0;
  gbitcells.clear();
  gleftcell.clear();
  grightcell.clear();
  cur_ncell = 1;
}


void 
JB2Dict::JB2Codec::Decode::set_dict_callback(JB2DecoderCallback *cb, void *arg)
{
  cbfunc = cb;
  cbarg = arg;
}


// CODE NUMBERS

inline bool
JB2Dict::JB2Codec::Decode::CodeBit(const bool, BitContext &ctx)
{
  return gzp->decoder(ctx)?true:false;
}

int
JB2Dict::JB2Codec::Decode::CodeNum(int low, int high, NumContext &ctx)
{
  return JB2Codec::CodeNum(low,high,&ctx,0);
}

int
JB2Dict::JB2Codec::CodeNum(int low, int high, NumContext *pctx, int v)
{
  bool negative=false;
  int cutoff;
  // Check
  if (!pctx || ((int)*pctx >= cur_ncell))
    G_THROW( ERR_MSG("JB2Image.bad_numcontext") );
  // Start all phases
  cutoff = 0;
  for(int phase=1,range=0xffffffff;range != 1;)
    {
      if (! *pctx)
        {
          const int max_ncell=gbitcells;
          if (cur_ncell >= max_ncell)
            {
              const int nmax_ncell = max_ncell+CELLCHUNK;
              gbitcells.resize(nmax_ncell);
              gleftcell.resize(nmax_ncell);
              grightcell.resize(nmax_ncell);
            }
          *pctx = cur_ncell ++;
          bitcells[*pctx] = 0;
          leftcell[*pctx] = rightcell[*pctx] = 0;
        }
      // encode
      const bool decision = encoding
        ? ((low < cutoff && high >= cutoff)
          ? CodeBit((v>=cutoff),bitcells[*pctx])
          : (v >= cutoff))
        : ((low>=cutoff)||((high>=cutoff)&&CodeBit(false,bitcells[*pctx])));
      // context for new bit
      pctx = decision?(&rightcell[*pctx]):(&leftcell[*pctx]);
      // phase dependent part
      switch (phase) 
        {
	case 1:
          negative = !decision;
          if (negative) 
            {
              if (encoding)
                v = - v - 1;
              const int temp = - low - 1; 
              low = - high - 1; 
              high = temp;
	    }
          phase = 2; cutoff =  1;
          break;
          
	case 2:
          if (!decision) 
            {
              phase = 3;
              range = (cutoff + 1) / 2;
              if (range == 1)
                cutoff = 0;
              else
                cutoff -= range / 2;
	    }
          else 
            { 
              cutoff += cutoff + 1; 
            }
          break;

	case 3:
          range /= 2;
          if (range != 1) 
            {
              if (!decision)
                cutoff -= range / 2;
              else               
                cutoff += range / 2;
	    }
          else if (!decision) 
            {
                cutoff --;
	    }
          break;
	}
    }
    return (negative)?(- cutoff - 1):cutoff;
}



// CODE COMMENTS

void 
JB2Dict::JB2Codec::Decode::code_comment(GUTF8String &comment)
{
      int size=CodeNum(0, BIGPOSITIVE, dist_comment_length);
      comment.empty();
      char *combuf = comment.getbuf(size);
      for (int i=0; i<size; i++) 
        {
          combuf[i]=CodeNum(0, 255, dist_comment_byte);
        }
      comment.getbuf();
}


// LIBRARY


void
JB2Dict::JB2Codec::init_library(JB2Dict &jim)
{
  int nshape = jim.get_inherited_shape_count();
  shape2lib.resize(0,nshape-1);
  lib2shape.resize(0,nshape-1);
  libinfo.resize(0,nshape-1);
  for (int i=0; i<nshape; i++)
    {
      shape2lib[i] = i;
      lib2shape[i] = i;
      jim.get_bounding_box(i, libinfo[i]);
    }
}

int 
JB2Dict::JB2Codec::add_library(const int shapeno, JB2Shape &jshp)
{
  const int libno = lib2shape.hbound() + 1;
  lib2shape.touch(libno);
  lib2shape[libno] = shapeno;
  shape2lib.touch(shapeno);
  shape2lib[shapeno] = libno;
  libinfo.touch(libno);
  libinfo[libno].compute_bounding_box(*(jshp.bits));
  return libno;
}


// CODE SIMPLE VALUES

inline void 
JB2Dict::JB2Codec::Decode::code_record_type(int &rectype)
{
  rectype=CodeNum( START_OF_DATA, END_OF_DATA, dist_record_type);
}

int 
JB2Dict::JB2Codec::Decode::code_match_index(int &index, JB2Dict &)
{
    int match=CodeNum(0, lib2shape.hbound(), dist_match_index);
    index = lib2shape[match];
    return match;
}


// HANDLE SHORT LIST

int 
JB2Dict::JB2Codec::update_short_list(const int v)
{
  if (++ short_list_pos == 3)
    short_list_pos = 0;
  int * const s = short_list;
  s[short_list_pos] = v;

  return (s[0] >= s[1])
    ?((s[0] > s[2])?((s[1] >= s[2])?s[1]:s[2]):s[0])
    :((s[0] < s[2])?((s[1] >= s[2])?s[2]:s[1]):s[0]);
}



// CODE PAIRS


void
JB2Dict::JB2Codec::Decode::code_inherited_shape_count(JB2Dict &jim)
{
  int size=CodeNum(0, BIGPOSITIVE, inherited_shape_count_dist);
    {
      GP<JB2Dict> dict = jim.get_inherited_dict();
      if (!dict && size>0)
        {
          // Call callback function to obtain dictionary
          if (cbfunc)
            dict = (*cbfunc)(cbarg);
          if (dict)
            jim.set_inherited_dict(dict);
        }
      if (!dict && size>0)
        G_THROW( ERR_MSG("JB2Image.need_dict") );
      if (dict && size!=dict->get_shape_count())
        G_THROW( ERR_MSG("JB2Image.bad_dict") );
    }
}

void 
JB2Dict::JB2Codec::Decode::code_image_size(JB2Dict &jim)
{
  int w=CodeNum(0, BIGPOSITIVE, image_size_dist);
  int h=CodeNum(0, BIGPOSITIVE, image_size_dist);
  if (w || h)
    G_THROW( ERR_MSG("JB2Image.bad_dict2") );
  JB2Codec::code_image_size(jim);
}

void 
JB2Dict::JB2Codec::code_image_size(JB2Dict &)
{
  last_left = 1;
  last_row_left = 0;
  last_row_bottom = 0;
  last_right = 0;
  fill_short_list(last_row_bottom);
  gotstartrecordp = 1;
}

void 
JB2Dict::JB2Codec::Decode::code_image_size(JB2Image &jim)
{
  image_columns=CodeNum(0, BIGPOSITIVE, image_size_dist);
  image_rows=CodeNum(0, BIGPOSITIVE, image_size_dist);
  if (!image_columns || !image_rows)
    G_THROW( ERR_MSG("JB2Image.zero_dim") );
  jim.set_dimension(image_columns, image_rows);
  JB2Codec::code_image_size(jim);
}

void 
JB2Dict::JB2Codec::code_image_size(JB2Image &)
{
  last_left = 1 + image_columns;
  last_row_left = 0;
  last_row_bottom = image_rows;
  last_right = 0;
  fill_short_list(last_row_bottom);
  gotstartrecordp = 1;
}

inline int
JB2Dict::JB2Codec::Decode::get_diff(int,NumContext &rel_loc)
{
   return CodeNum(BIGNEGATIVE, BIGPOSITIVE, rel_loc);
}

void 
JB2Dict::JB2Codec::code_relative_location(JB2Blit *jblt, int rows, int columns)
{
  // Check start record
  if (!gotstartrecordp)
    G_THROW( ERR_MSG("JB2Image.no_start") );
  // Find location
  int bottom=0, left=0, top=0, right=0;
  int x_diff, y_diff;
  if (encoding)
    {
      left = jblt->left + 1;
      bottom = jblt->bottom + 1;
      right = left + columns - 1;
      top = bottom + rows - 1;
    }
  // Code offset type
  int new_row=CodeBit((left<last_left), offset_type_dist);
  if (new_row)
    {
      // Begin a new row
      x_diff=get_diff(left-last_row_left,rel_loc_x_last);
      y_diff=get_diff(top-last_row_bottom,rel_loc_y_last);
      if (!encoding)
        {
          left = last_row_left + x_diff;
          top = last_row_bottom + y_diff;
          right = left + columns - 1;
          bottom = top - rows + 1;
        }
      last_left = last_row_left = left;
      last_right = right;
      last_bottom = last_row_bottom = bottom;
      fill_short_list(bottom);
    }
  else
    {
      // Same row
      x_diff=get_diff(left-last_right,rel_loc_x_current);
      y_diff=get_diff(bottom-last_bottom,rel_loc_y_current);
      if (!encoding)
        {
          left = last_right + x_diff;
          bottom = last_bottom + y_diff;
          right = left + columns - 1;
          top = bottom + rows - 1;
        }
      last_left = left;
      last_right = right;
      last_bottom = update_short_list(bottom);
    }
  // Store in blit record
  if (!encoding)
    {
      jblt->bottom = bottom - 1;
      jblt->left = left - 1;
    }
}

void 
JB2Dict::JB2Codec::Decode::code_absolute_location(JB2Blit *jblt, int rows, int columns)
{
  // Check start record
  if (!gotstartrecordp)
    G_THROW( ERR_MSG("JB2Image.no_start") );
  int left=CodeNum(1, image_columns, abs_loc_x);
  int top=CodeNum(1, image_rows, abs_loc_y);
  jblt->bottom = top - rows + 1 - 1;
  jblt->left = left - 1;
}

void 
JB2Dict::JB2Codec::Decode::code_absolute_mark_size(GBitmap &bm, int border)
{
  int xsize=CodeNum(0, BIGPOSITIVE, abs_size_x);
  int ysize=CodeNum(0, BIGPOSITIVE, abs_size_y);
  if ((xsize!=(unsigned short)xsize) || (ysize!=(unsigned short)ysize))
    G_THROW( ERR_MSG("JB2Image.bad_number") );
  bm.init(ysize, xsize, border);
}

void 
JB2Dict::JB2Codec::Decode::code_relative_mark_size(GBitmap &bm, int cw, int ch, int border)
{
  int xdiff=CodeNum(BIGNEGATIVE, BIGPOSITIVE, rel_size_x);
  int ydiff=CodeNum(BIGNEGATIVE, BIGPOSITIVE, rel_size_y);
  int xsize = cw + xdiff;
  int ysize = ch + ydiff;
  if ((xsize!=(unsigned short)xsize) || (ysize!=(unsigned short)ysize))
    G_THROW( ERR_MSG("JB2Image.bad_number") );
  bm.init(ysize, xsize, border);
}




// CODE BITMAP DIRECTLY

void 
JB2Dict::JB2Codec::code_bitmap_directly (GBitmap &bm)
{
  // Make sure bitmap will not be disturbed
  GMonitorLock lock(bm.monitor());
  // ensure borders are adequate
  bm.minborder(3);
  // initialize row pointers
  int dy = bm.rows() - 1;
  code_bitmap_directly(bm,bm.columns(),dy,bm[dy+2],bm[dy+1],bm[dy]);
}

void 
JB2Dict::JB2Codec::Decode::code_bitmap_directly(
  GBitmap &bm,const int dw, int dy,
  unsigned char *up2, unsigned char *up1, unsigned char *up0 )
{
      ZPCodec &zp=*gzp;
      // iterate on rows (decoding)      
      while (dy >= 0)
        {
          int context=get_direct_context(up2, up1, up0, 0);
          for(int dx=0;dx < dw;)
            {
              int n = zp.decoder(bitdist[context]);
              up0[dx++] = n;
              context=shift_direct_context(context, n, up2, up1, up0, dx);
            }
          // next row
          dy -= 1;
          up2 = up1;
          up1 = up0;
          up0 = bm[dy];
        }
#ifndef NDEBUG
      bm.check_border();
#endif
}





// CODE BITMAP BY CROSS CODING

void 
JB2Dict::JB2Codec::code_bitmap_by_cross_coding (GBitmap &bm, GP<GBitmap> &cbm, const int libno)
{
  // Make sure bitmaps will not be disturbed
  GP<GBitmap> copycbm=GBitmap::create();
  if (cbm->monitor())
    {
      // Perform a copy when the bitmap is explicitely shared
      GMonitorLock lock2(cbm->monitor());
      copycbm->init(*cbm);
      cbm = copycbm;
    }
  GMonitorLock lock1(bm.monitor());
  // Center bitmaps
  const int cw = cbm->columns();
  const int dw = bm.columns();
  const int dh = bm.rows();
  const LibRect &l = libinfo[libno];
  const int xd2c = (dw/2 - dw + 1) - ((l.right - l.left + 1)/2 - l.right);
  const int yd2c = (dh/2 - dh + 1) - ((l.top - l.bottom + 1)/2 - l.top);
  // Ensure borders are adequate
  bm.minborder(2);
  cbm->minborder(2-xd2c);
  cbm->minborder(2+dw+xd2c-cw);
  // Initialize row pointers
  const int dy = dh - 1;
  const int cy = dy + yd2c;
#ifndef NDEBUG
  bm.check_border();
  cbm->check_border();
#endif
  code_bitmap_by_cross_coding (bm,*cbm, xd2c, dw, dy, cy, bm[dy+1], bm[dy],
    (*cbm)[cy+1] + xd2c, (*cbm)[cy  ] + xd2c, (*cbm)[cy-1] + xd2c);
}

void 
JB2Dict::JB2Codec::Decode::code_bitmap_by_cross_coding (GBitmap &bm, GBitmap &cbm,
  const int xd2c, const int dw, int dy, int cy,
  unsigned char *up1, unsigned char *up0, unsigned char *xup1, 
  unsigned char *xup0, unsigned char *xdn1 )
{
      ZPCodec &zp=*gzp;
      // iterate on rows (decoding)      
      while (dy >= 0)
        {
          int context=get_cross_context(
                            up1, up0, xup1, xup0, xdn1, 0);
          for(int dx=0;dx < dw;)
            {
              const int n = zp.decoder(cbitdist[context]);
              up0[dx++] = n;
              context=shift_cross_context(context, n,  
                                  up1, up0, xup1, xup0, xdn1, dx);
            }
          // next row
          up1 = up0;
          up0 = bm[--dy];
          xup1 = xup0;
          xup0 = xdn1;
          xdn1 = cbm[(--cy)-1] + xd2c;
#ifndef NDEBUG
          bm.check_border();
#endif
        }
}




// CODE JB2DICT RECORD

void
JB2Dict::JB2Codec::code_record(
  int &rectype, const GP<JB2Dict> &gjim, JB2Shape *xjshp)
{
  GP<GBitmap> cbm;
  GP<GBitmap> bm;
  int shapeno = -1;

  // Code record type
  code_record_type(rectype);
  
  // Pre-coding actions
  switch(rectype)
    {
    case NEW_MARK_LIBRARY_ONLY:
    case MATCHED_REFINE_LIBRARY_ONLY:
      {
        if(!xjshp)
        {
          G_THROW( ERR_MSG("JB2Image.bad_number") );
        }
        JB2Shape &jshp=*xjshp;
        if (!encoding) 
        {
          jshp.bits = GBitmap::create();
          jshp.parent = -1;
        }
        bm = jshp.bits;
        break;
      }
    }
  // Coding actions
  switch (rectype)
    {
    case START_OF_DATA:
      {
        if(!gjim)
        {
           G_THROW( ERR_MSG("JB2Image.bad_number") );
        }
        JB2Dict &jim=*gjim;
        code_image_size (jim);
        code_eventual_lossless_refinement ();
        if (! encoding)
          init_library(jim);
        break;
      }
    case NEW_MARK_LIBRARY_ONLY:
      {
        code_absolute_mark_size (*bm, 4);
        code_bitmap_directly (*bm);
        break;
      }
    case MATCHED_REFINE_LIBRARY_ONLY:
      {
        if(!xjshp||!gjim)
        {
           G_THROW( ERR_MSG("JB2Image.bad_number") );
        }
        JB2Dict &jim=*gjim;
        JB2Shape &jshp=*xjshp;
        int match = code_match_index (jshp.parent, jim);
        cbm = jim.get_shape(jshp.parent).bits;
        LibRect &l = libinfo[match];
        code_relative_mark_size (*bm, l.right-l.left+1, l.top-l.bottom+1, 4);
        code_bitmap_by_cross_coding (*bm, cbm, jshp.parent);
        break;
      }
    case PRESERVED_COMMENT:
      {
        if(!gjim)
        {
           G_THROW( ERR_MSG("JB2Image.bad_number") );
        }
        JB2Dict &jim=*gjim;
        code_comment(jim.comment);
        break;
      }
    case REQUIRED_DICT_OR_RESET:
      {
        if (! gotstartrecordp)
        {
	  // Indicates need for a shape dictionary
          if(!gjim)
          {
             G_THROW( ERR_MSG("JB2Image.bad_number") );
          }
	  code_inherited_shape_count(*gjim);
        }else
	  // Reset all numerical contexts to zero
	  reset_numcoder();
        break;
      }
    case END_OF_DATA:
      {
        break;
      }
    default:
      {
        G_THROW( ERR_MSG("JB2Image.bad_type") );
      }
    }
  // Post-coding action
  if (!encoding)
    {
      // add shape to dictionary
      switch(rectype)
        {
        case NEW_MARK_LIBRARY_ONLY:
        case MATCHED_REFINE_LIBRARY_ONLY:
          {
            if(!xjshp||!gjim)
            {
               G_THROW( ERR_MSG("JB2Image.bad_number") );
            }
            JB2Shape &jshp=*xjshp;
            shapeno = gjim->add_shape(jshp);
            add_library(shapeno, jshp);
            break;
          }
        }
      // make sure everything is compacted
      // decompaction will occur automatically when needed
      if (bm)
        bm->compress();
    }
}


// CODE JB2DICT

void 
JB2Dict::JB2Codec::Decode::code(const GP<JB2Dict> &gjim)
{
  if(!gjim)
  {
    G_THROW( ERR_MSG("JB2Image.bad_number") );
  }
  JB2Dict &jim=*gjim;
  // -------------------------
  // THIS IS THE DECODING PART
  // -------------------------
  int rectype;
  JB2Shape tmpshape;
  do {
    code_record(rectype, gjim, &tmpshape);        
  } while(rectype != END_OF_DATA);
  if (!gotstartrecordp)
    G_THROW( ERR_MSG("JB2Image.no_start") );
  // cache bounding boxes
  int nshapes = jim.get_shape_count();
  int ishapes = jim.get_inherited_shape_count();
  jim.boxes.resize(0, nshapes-ishapes-1);
  for (int i = ishapes; i < nshapes; i++)
    jim.boxes[i-ishapes] = libinfo[i];
  // compress
  jim.compress();
}



// CODE JB2IMAGE RECORD

void
JB2Dict::JB2Codec::code_record(
  int &rectype, const GP<JB2Image> &gjim, JB2Shape *xjshp, JB2Blit *jblt)
{
  GP<GBitmap> bm;
  GP<GBitmap> cbm;
  int shapeno = -1;
  int match;

  // Code record type
  code_record_type(rectype);
  
  // Pre-coding actions
  switch(rectype)
    {
    case NEW_MARK:
    case NEW_MARK_LIBRARY_ONLY:
    case NEW_MARK_IMAGE_ONLY:
    case MATCHED_REFINE:
    case MATCHED_REFINE_LIBRARY_ONLY:
    case MATCHED_REFINE_IMAGE_ONLY:
    case NON_MARK_DATA:
      {
        if(!xjshp)
        {
           G_THROW( ERR_MSG("JB2Image.bad_number") );
        }
        JB2Shape &jshp=*xjshp;
        if (!encoding) 
        {
          jshp.bits = GBitmap::create();
          jshp.parent = -1;
          if (rectype == NON_MARK_DATA)
            jshp.parent = -2;
        }
        bm = jshp.bits;
        break;
      }
    }
  // Coding actions
  switch (rectype)
    {
    case START_OF_DATA:
      {
        if(!gjim)
        {
           G_THROW( ERR_MSG("JB2Image.bad_number") );
        }
        JB2Image &jim=*gjim;
        code_image_size (jim);
        code_eventual_lossless_refinement ();
        if (! encoding)
          init_library(jim);
        break;
      }
    case NEW_MARK:
      {
        code_absolute_mark_size (*bm, 4);
        code_bitmap_directly (*bm);
        code_relative_location (jblt, bm->rows(), bm->columns() );
        break;
      }
    case NEW_MARK_LIBRARY_ONLY:
      {
        code_absolute_mark_size (*bm, 4);
        code_bitmap_directly (*bm);
        break;
      }
    case NEW_MARK_IMAGE_ONLY:
      {
        code_absolute_mark_size (*bm, 3);
        code_bitmap_directly (*bm);
        code_relative_location (jblt, bm->rows(), bm->columns() );
        break;
      }
    case MATCHED_REFINE:
      {
        if(!xjshp || !gjim)
        {
           G_THROW( ERR_MSG("JB2Image.bad_number") );
        }
        JB2Shape &jshp=*xjshp;
        JB2Image &jim=*gjim;
        match = code_match_index (jshp.parent, jim);
        cbm = jim.get_shape(jshp.parent).bits;
        LibRect &l = libinfo[match];
        code_relative_mark_size (*bm, l.right-l.left+1, l.top-l.bottom+1, 4); 
        code_bitmap_by_cross_coding (*bm, cbm, match);
        code_relative_location (jblt, bm->rows(), bm->columns() );
        break;
      }
    case MATCHED_REFINE_LIBRARY_ONLY:
      {
        if(!xjshp||!gjim)
        {
           G_THROW( ERR_MSG("JB2Image.bad_number") );
        }
        JB2Image &jim=*gjim;
        JB2Shape &jshp=*xjshp;
        match = code_match_index (jshp.parent, jim);
        cbm = jim.get_shape(jshp.parent).bits;
        LibRect &l = libinfo[match];
        code_relative_mark_size (*bm, l.right-l.left+1, l.top-l.bottom+1, 4);
        break;
      }
    case MATCHED_REFINE_IMAGE_ONLY:
      {
        if(!xjshp||!gjim)
        {
           G_THROW( ERR_MSG("JB2Image.bad_number") );
        }
        JB2Image &jim=*gjim;
        JB2Shape &jshp=*xjshp;
        match = code_match_index (jshp.parent, jim);
        cbm = jim.get_shape(jshp.parent).bits;
        LibRect &l = libinfo[match];
        code_relative_mark_size (*bm, l.right-l.left+1, l.top-l.bottom+1, 4);
        code_bitmap_by_cross_coding (*bm, cbm, match);
        code_relative_location (jblt, bm->rows(), bm->columns() );
        break;
      }
    case MATCHED_COPY:
      {
        int temp;
        if (encoding) temp = jblt->shapeno;
        if(!gjim)
        {
           G_THROW( ERR_MSG("JB2Image.bad_number") );
        }
        JB2Image &jim=*gjim;
        match = code_match_index (temp, jim);
        if (!encoding) jblt->shapeno = temp;
        bm = jim.get_shape(jblt->shapeno).bits;
        LibRect &l = libinfo[match];
        jblt->left += l.left;
        jblt->bottom += l.bottom;
        if (jim.reproduce_old_bug)
          code_relative_location (jblt, bm->rows(), bm->columns() );
        else
          code_relative_location (jblt, l.top-l.bottom+1, l.right-l.left+1 );
        jblt->left -= l.left;
        jblt->bottom -= l.bottom; 
        break;
      }
    case NON_MARK_DATA:
      {
        code_absolute_mark_size (*bm, 3);
        code_bitmap_directly (*bm);
        code_absolute_location (jblt, bm->rows(), bm->columns() );
        break;
      }
    case PRESERVED_COMMENT:
      {
        if(!gjim)
        {
           G_THROW( ERR_MSG("JB2Image.bad_number") );
        }
        JB2Image &jim=*gjim;
        code_comment(jim.comment);
        break;
      }
    case REQUIRED_DICT_OR_RESET:
      {
        if(!gjim)
        {
           G_THROW( ERR_MSG("JB2Image.bad_number") );
        }
        JB2Image &jim=*gjim;
        if (! gotstartrecordp)
	  // Indicates need for a shape dictionary
	  code_inherited_shape_count(jim);
	else
	  // Reset all numerical contexts to zero
	  reset_numcoder();
        break;
      }
    case END_OF_DATA:
      {
        break;
      }
    default:
      {
        G_THROW( ERR_MSG("JB2Image.unknown_type") );
      }
    }
  
  // Post-coding action
  if (!encoding)
    {
      // add shape to image
      switch(rectype)
        {
        case NEW_MARK:
        case NEW_MARK_LIBRARY_ONLY:
        case NEW_MARK_IMAGE_ONLY:
        case MATCHED_REFINE:
        case MATCHED_REFINE_LIBRARY_ONLY:
        case MATCHED_REFINE_IMAGE_ONLY:
        case NON_MARK_DATA:
          {
            if(!xjshp||!gjim)
            {
              G_THROW( ERR_MSG("JB2Image.bad_number") );
            }
            JB2Shape &jshp=*xjshp;
            shapeno = gjim->add_shape(jshp);
            shape2lib.touch(shapeno);
            shape2lib[shapeno] = -1;
            break;
          }
        }
      // add shape to library
      switch(rectype)
        {
        case NEW_MARK:
        case NEW_MARK_LIBRARY_ONLY:
        case MATCHED_REFINE:
        case MATCHED_REFINE_LIBRARY_ONLY:
          if(!xjshp)
          {
            G_THROW( ERR_MSG("JB2Image.bad_number") );
          }
          add_library(shapeno, *xjshp);
          break;
        }
      // make sure everything is compacted
      // decompaction will occur automatically on cross-coding bitmaps
      if (bm)
        bm->compress();
      // add blit to image
      switch (rectype)
        {
        case NEW_MARK:
        case NEW_MARK_IMAGE_ONLY:
        case MATCHED_REFINE:
        case MATCHED_REFINE_IMAGE_ONLY:
        case NON_MARK_DATA:
          jblt->shapeno = shapeno;
        case MATCHED_COPY:
          if(!gjim)
          {
            G_THROW( ERR_MSG("JB2Image.bad_number") );
          }
          gjim->add_blit(* jblt);
          break;
        }
    }
}


// CODE JB2IMAGE

void 
JB2Dict::JB2Codec::Decode::code(const GP<JB2Image> &gjim)
{
  if(!gjim)
  {
    G_THROW( ERR_MSG("JB2Image.bad_number") );
  }
  JB2Image &jim=*gjim;
      // -------------------------
      // THIS IS THE DECODING PART
      // -------------------------
      int rectype;
      JB2Blit tmpblit;
      JB2Shape tmpshape;
      do
        {
          code_record(rectype, gjim, &tmpshape, &tmpblit);        
        } 
      while(rectype!=END_OF_DATA);
      if (!gotstartrecordp)
        G_THROW( ERR_MSG("JB2Image.no_start") );
      jim.compress();
}



////////////////////////////////////////
//// HELPERS
////////////////////////////////////////

void 
JB2Dict::LibRect::compute_bounding_box(const GBitmap &bm)
{
  // Avoid trouble
  GMonitorLock lock(bm.monitor());
  // Get size
  const int w = bm.columns();
  const int h = bm.rows();
  const int s = bm.rowsize();
  // Right border
  for(right=w-1;right >= 0;--right)
    {
      unsigned char const *p = bm[0] + right;
      unsigned char const * const pe = p+(s*h);
      for (;(p<pe)&&(!*p);p+=s)
      	continue;
      if (p<pe)
        break;
    }
  // Top border
  for(top=h-1;top >= 0;--top)
    {
      unsigned char const *p = bm[top];
      unsigned char const * const pe = p+w;
      for (;(p<pe)&&(!*p); ++p)
      	continue;
      if (p<pe)
        break;
    }
  // Left border
  for (left=0;left <= right;++left)
    {
      unsigned char const *p = bm[0] + left;
      unsigned char const * const pe=p+(s*h);
      for (;(p<pe)&&(!*p);p+=s)
      	continue;
      if (p<pe)
        break;
    }
  // Bottom border
  for(bottom=0;bottom <= top;++bottom)
    {
      unsigned char const *p = bm[bottom];
      unsigned char const * const pe = p+w;
      for (;(p<pe)&&(!*p); ++p)
      	continue;
      if (p<pe)
        break;
    }
}


void
JB2Dict::get_bounding_box(int shapeno, LibRect &dest)
{
  if (shapeno < inherited_shapes && inherited_dict)
    {
      inherited_dict->get_bounding_box(shapeno, dest);
    }
  else if (shapeno >= inherited_shapes &&
           shapeno < inherited_shapes + boxes.size())
    {
      dest = boxes[shapeno - inherited_shapes];
    }
  else
    {
      JB2Shape &jshp = get_shape(shapeno);
      dest.compute_bounding_box(*(jshp.bits));
    }
}


GP<JB2Dict>
JB2Dict::create(void)
{
  return new JB2Dict();
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
