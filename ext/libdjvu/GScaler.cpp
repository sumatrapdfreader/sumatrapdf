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

// Rescale images with fast bilinear interpolation
// From: Leon Bottou, 1/31/2002
// Almost equal to my initial code.

#include "GScaler.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


////////////////////////////////////////
// CONSTANTS


#define FRACBITS  4
#define FRACSIZE  (1<<FRACBITS)
#define FRACSIZE2 (FRACSIZE>>1)
#define FRACMASK  (FRACSIZE-1)






////////////////////////////////////////
// UTILITIES


static int interp_ok = 0;
static short interp[FRACSIZE][512];

static void
prepare_interp()
{
  if (! interp_ok)
    {
      interp_ok = 1;
      for (int i=0; i<FRACSIZE; i++)
        {
          short *deltas = & interp[i][256];
          for (int j = -255; j <= 255; j++)
            deltas[j] = ( j*i + FRACSIZE2 ) >> FRACBITS;
        }
    }
}


static inline int
mini(int x, int y) 
{ 
  return (x < y ? x : y);
}


static inline int
maxi(int x, int y) 
{ 
  return (x > y ? x : y);
}






////////////////////////////////////////
// GSCALER


GScaler::GScaler()
  : inw(0), inh(0), 
    xshift(0), yshift(0), redw(0), redh(0), 
    outw(0), outh(0),
    gvcoord(vcoord,0), ghcoord(hcoord,0)
{
}


GScaler::~GScaler()
{
}


void
GScaler::set_input_size(int w, int h)
{ 
  inw = w;
  inh = h;
  if (vcoord)
  {
    gvcoord.resize(0);
  }
  if (hcoord)
  {
    ghcoord.resize(0);
  }
}


void
GScaler::set_output_size(int w, int h)
{ 
  outw = w;
  outh = h;
  if (vcoord)
  {
    gvcoord.resize(0);
  }
  if (hcoord)
  {
    ghcoord.resize(0);
  }
}


static void
prepare_coord(int *coord, int inmax, int outmax, int in, int out)
{
  int len = (in*FRACSIZE);
  int beg = (len+out)/(2*out) - FRACSIZE2;
  // Bresenham algorithm
  int y = beg;
  int z = out/2;
  int inmaxlim = (inmax-1)*FRACSIZE;
  for  (int x=0; x<outmax; x++)
    {
      coord[x] = mini(y,inmaxlim);
      z = z + len;
      y = y + z / out;  
      z = z % out;
    }
  // Result must fit exactly
  if (out==outmax && y!=beg+len)
    G_THROW( ERR_MSG("GScaler.assertion") );
}


void 
GScaler::set_horz_ratio(int numer, int denom)
{
  if (! (inw>0 && inh>0 && outw>0 && outh>0))
    G_THROW( ERR_MSG("GScaler.undef_size") );
  // Implicit ratio (determined by the input/output sizes)
  if (numer==0 && denom==0) {
    numer = outw;
    denom = inw;
  } else if (numer<=0 || denom<=0)
    G_THROW( ERR_MSG("GScaler.ratios") );
  // Compute horz reduction
  xshift = 0;
  redw = inw;
  while (numer+numer < denom) {
    xshift += 1;
    redw = (redw + 1) >> 1;
   numer = numer << 1;
  }
  // Compute coordinate table
  if (! hcoord)
    ghcoord.resize(outw);
  prepare_coord(hcoord, redw, outw, denom, numer);
}


void 
GScaler::set_vert_ratio(int numer, int denom)
{
  if (! (inw>0 && inh>0 && outw>0 && outh>0))
    G_THROW( ERR_MSG("GScaler.undef_size") );
  // Implicit ratio (determined by the input/output sizes)
  if (numer==0 && denom==0) {
    numer = outh;
    denom = inh;
  } else if (numer<=0 || denom<=0)
    G_THROW( ERR_MSG("GScaler.ratios") );
  // Compute horz reduction
  yshift = 0;
  redh = inh;
  while (numer+numer < denom) {
    yshift += 1;
    redh = (redh + 1) >> 1;
    numer = numer << 1;
  }
  // Compute coordinate table
  if (! vcoord)
  {
    gvcoord.resize(outh);
  }
  prepare_coord(vcoord, redh, outh, denom, numer);
}


void
GScaler::make_rectangles(const GRect &desired, GRect &red, GRect &inp)
{
  // Parameter validation
  if (desired.xmin<0 || desired.ymin<0 ||
      desired.xmax>outw || desired.ymax>outh )
    G_THROW( ERR_MSG("GScaler.too_big") );
  // Compute ratio (if not done yet)
  if (!vcoord) 
    set_vert_ratio(0,0);
  if (!hcoord) 
    set_horz_ratio(0,0);
  // Compute reduced bounds
  red.xmin = (hcoord[desired.xmin]) >> FRACBITS;
  red.ymin = (vcoord[desired.ymin]) >> FRACBITS;
  red.xmax = (hcoord[desired.xmax-1]+FRACSIZE-1) >> FRACBITS;
  red.ymax = (vcoord[desired.ymax-1]+FRACSIZE-1) >> FRACBITS;
  // Borders
  red.xmin = maxi(red.xmin, 0);
  red.xmax = mini(red.xmax+1, redw);
  red.ymin = maxi(red.ymin, 0);
  red.ymax = mini(red.ymax+1, redh);
  // Input
  inp.xmin = maxi(red.xmin<<xshift, 0); 
  inp.xmax = mini(red.xmax<<xshift, inw); 
  inp.ymin = maxi(red.ymin<<yshift, 0); 
  inp.ymax = mini(red.ymax<<yshift, inh); 
}


void 
GScaler::get_input_rect( const GRect &desired_output, GRect &required_input )
{
  GRect red;
  make_rectangles(desired_output, red, required_input);
}






////////////////////////////////////////
// GBITMAPSCALER


GBitmapScaler::GBitmapScaler()
  : glbuffer(lbuffer,0), gconv(conv,0), gp1(p1,0), gp2(p2,0)
{
}


GBitmapScaler::GBitmapScaler(int inw, int inh, int outw, int outh)
  : glbuffer(lbuffer,0), gconv(conv,0), gp1(p1,0), gp2(p2,0)
{
  set_input_size(inw, inh);
  set_output_size(outw, outh);
}


GBitmapScaler::~GBitmapScaler()
{
}


unsigned char *
GBitmapScaler::get_line(int fy, 
                        const GRect &required_red, 
                        const GRect &provided_input,
                        const GBitmap &input )
{
  if (fy < required_red.ymin)
    fy = required_red.ymin; 
  else if (fy >= required_red.ymax)
    fy = required_red.ymax - 1;
  // Cached line
  if (fy == l2)
    return p2;
  if (fy == l1)
    return p1;
  // Shift
  unsigned char *p = p1;
  p1 = p2;
  l1 = l2;
  p2 = p;
  l2 = fy;
  if (xshift==0 && yshift==0)
    {
      // Fast mode
      int dx = required_red.xmin-provided_input.xmin;
      int dx1 = required_red.xmax-provided_input.xmin;
      const unsigned char *inp1 = input[fy-provided_input.ymin] + dx;
      while (dx++ < dx1)
        *p++ = conv[*inp1++];
      return p2;
    }
  else
    {
      // Compute location of line
      GRect line;
      line.xmin = required_red.xmin << xshift;
      line.xmax = required_red.xmax << xshift;
      line.ymin = fy << yshift;
      line.ymax = (fy+1) << yshift;
      line.intersect(line, provided_input);
      line.translate(-provided_input.xmin, -provided_input.ymin);
      // Prepare variables
      const unsigned char *botline = input[line.ymin];
      int rowsize = input.rowsize();
      int sw = 1<<xshift;
      int div = xshift+yshift;
      int rnd = 1<<(div-1);
      // Compute averages
      for (int x=line.xmin; x<line.xmax; x+=sw,p++)
        {
          int g=0, s=0;
          const unsigned char *inp0 = botline + x;
          int sy1 = mini(line.height(), (1<<yshift));
          for (int sy=0; sy<sy1; sy++,inp0+=rowsize)
	    {
	      const unsigned char *inp1;
	      const unsigned char *inp2 = inp0 + mini(x+sw, line.xmax) - x;
	      for (inp1=inp0; inp1<inp2; inp1++)
		{
		  g += conv[*inp1];
		  s += 1;
		}
	    }
          if (s == rnd+rnd)
            *p = (g+rnd)>>div;
          else
            *p = (g+s/2)/s;
        }
      // Return
      return p2;
    }
}


void 
GBitmapScaler::scale( const GRect &provided_input, const GBitmap &input,
                      const GRect &desired_output, GBitmap &output )
{
  // Compute rectangles
  GRect required_input; 
  GRect required_red;
  make_rectangles(desired_output, required_red, required_input);
  // Parameter validation
  if (provided_input.width() != (int)input.columns() ||
      provided_input.height() != (int)input.rows() )
    G_THROW( ERR_MSG("GScaler.no_match") );
  if (provided_input.xmin > required_input.xmin ||
      provided_input.ymin > required_input.ymin ||
      provided_input.xmax < required_input.xmax ||
      provided_input.ymax < required_input.ymax  )
    G_THROW( ERR_MSG("GScaler.too_small") );
  // Adjust output pixmap
  if (desired_output.width() != (int)output.columns() ||
      desired_output.height() != (int)output.rows() )
    output.init(desired_output.height(), desired_output.width());
  output.set_grays(256);
  // Prepare temp stuff
  gp1.resize(0);
  gp2.resize(0);
  glbuffer.resize(0);
  prepare_interp();
  const int bufw = required_red.width();
  glbuffer.resize(bufw+2);
  gp1.resize(bufw);
  gp2.resize(bufw);
  l1 = l2 = -1;
  // Prepare gray conversion array (conv)
  gconv.resize(0);
  gconv.resize(256);
  int maxgray = input.get_grays()-1;
  for (int i=0; i<256; i++) 
    {
      conv[i]=(i<= maxgray)
        ?(((i*255) + (maxgray>>1)) / maxgray)
        :255;
    }
  // Loop on output lines
  for (int y=desired_output.ymin; y<desired_output.ymax; y++)
    {
      // Perform vertical interpolation
      {
        int fy = vcoord[y];
        int fy1 = fy>>FRACBITS;
        int fy2 = fy1+1;
        const unsigned char *lower, *upper;
        // Obtain upper and lower line in reduced image
        lower = get_line(fy1, required_red, provided_input, input);
        upper = get_line(fy2, required_red, provided_input, input);
        // Compute line
        unsigned char *dest = lbuffer+1;
        const short *deltas = & interp[fy&FRACMASK][256];
        for(unsigned char const * const edest=(unsigned char const *)dest+bufw;
          dest<edest;upper++,lower++,dest++)
        {
          const int l = *lower;
          const int u = *upper;
          *dest = l + deltas[u-l];
        }
      }
      // Perform horizontal interpolation
      {
        // Prepare for side effects
        lbuffer[0]   = lbuffer[1];
        lbuffer[bufw+1] = lbuffer[bufw];
        unsigned char *line = lbuffer+1-required_red.xmin;
        unsigned char *dest  = output[y-desired_output.ymin];
        // Loop horizontally
        for (int x=desired_output.xmin; x<desired_output.xmax; x++)
          {
            int n = hcoord[x];
            const unsigned char *lower = line + (n>>FRACBITS);
            const short *deltas = &interp[n&FRACMASK][256];
            int l = lower[0];
            int u = lower[1];
            *dest = l + deltas[u-l];
            dest++;
          }
      }
    }
  // Free temporaries
  gp1.resize(0);
  gp2.resize(0);
  glbuffer.resize(0);
  gconv.resize(0);
}






////////////////////////////////////////
// GPIXMAPSCALER


GPixmapScaler::GPixmapScaler()
  : glbuffer(lbuffer,0), 
    gp1(p1,0), 
    gp2(p2,0)
{
}


GPixmapScaler::GPixmapScaler(int inw, int inh, int outw, int outh)
  : glbuffer(lbuffer,0), 
    gp1(p1,0), 
    gp2(p2,0)
{
  set_input_size(inw, inh);
  set_output_size(outw, outh);
}


GPixmapScaler::~GPixmapScaler()
{
}


GPixel *
GPixmapScaler::get_line(int fy, 
                        const GRect &required_red, 
                        const GRect &provided_input,
                        const GPixmap &input )
{
  if (fy < required_red.ymin)
    fy = required_red.ymin; 
  else if (fy >= required_red.ymax)
    fy = required_red.ymax - 1;
  // Cached line
  if (fy == l2)
    return p2;
  if (fy == l1)
    return p1;
  // Shift
  GPixel *p=p1;
  p1 = p2;
  l1 = l2;
  p2 = p;
  l2 = fy;
  // Compute location of line
  GRect line;
  line.xmin = required_red.xmin << xshift;
  line.xmax = required_red.xmax << xshift;
  line.ymin = fy << yshift;
  line.ymax = (fy+1) << yshift;
  line.intersect(line, provided_input);
  line.translate(-provided_input.xmin, -provided_input.ymin);
  // Prepare variables
  const GPixel *botline = input[line.ymin];
  int rowsize = input.rowsize();
  int sw = 1<<xshift;
  int div = xshift+yshift;
  int rnd = 1<<(div-1);
  // Compute averages
  for (int x=line.xmin; x<line.xmax; x+=sw,p++)
    {
      int r=0, g=0, b=0, s=0;
      const GPixel *inp0 = botline + x;
      int sy1 = mini(line.height(), (1<<yshift));
      for (int sy=0; sy<sy1; sy++,inp0+=rowsize)
        {
	  const GPixel *inp1;
	  const GPixel *inp2 = inp0 + mini(x+sw, line.xmax) - x;
          for (inp1 = inp0; inp1<inp2; inp1++)
            {
              r += inp1->r;  
              g += inp1->g;  
              b += inp1->b; 
              s += 1;
            }
        }
      if (s == rnd+rnd)
        {
          p->r = (r+rnd) >> div;
          p->g = (g+rnd) >> div;
          p->b = (b+rnd) >> div;
        }
      else
        {
          p->r = (r+s/2)/s;
          p->g = (g+s/2)/s;
          p->b = (b+s/2)/s;
        }
    }
  // Return
  return (GPixel *)p2;
}


void 
GPixmapScaler::scale( const GRect &provided_input, const GPixmap &input,
                      const GRect &desired_output, GPixmap &output )
{
  // Compute rectangles
  GRect required_input; 
  GRect required_red;
  make_rectangles(desired_output, required_red, required_input);
  // Parameter validation
  if (provided_input.width() != (int)input.columns() ||
      provided_input.height() != (int)input.rows() )
    G_THROW( ERR_MSG("GScaler.no_match") );
  if (provided_input.xmin > required_input.xmin ||
      provided_input.ymin > required_input.ymin ||
      provided_input.xmax < required_input.xmax ||
      provided_input.ymax < required_input.ymax  )
    G_THROW( ERR_MSG("GScaler.too_small") );
  // Adjust output pixmap
  if (desired_output.width() != (int)output.columns() ||
      desired_output.height() != (int)output.rows() )
    output.init(desired_output.height(), desired_output.width());
  // Prepare temp stuff 
  gp1.resize(0);
  gp2.resize(0);
  glbuffer.resize(0);
  prepare_interp();
  const int bufw = required_red.width();
  glbuffer.resize(bufw+2);
  if (xshift>0 || yshift>0)
    {
      gp1.resize(bufw);
      gp2.resize(bufw);
      l1 = l2 = -1;
    }
  // Loop on output lines
  for (int y=desired_output.ymin; y<desired_output.ymax; y++)
    {
      // Perform vertical interpolation
      {
        int fy = vcoord[y];
        int fy1 = fy>>FRACBITS;
        int fy2 = fy1+1;
        const GPixel *lower, *upper;
        // Obtain upper and lower line in reduced image
        if (xshift>0 || yshift>0)
          {
            lower = get_line(fy1, required_red, provided_input, input);
            upper = get_line(fy2, required_red, provided_input, input);
          }
        else
          {
            int dx = required_red.xmin-provided_input.xmin;
            fy1 = maxi(fy1, required_red.ymin);
            fy2 = mini(fy2, required_red.ymax-1);
            lower = input[fy1-provided_input.ymin] + dx;
            upper = input[fy2-provided_input.ymin] + dx;
          }
        // Compute line
        GPixel *dest = lbuffer+1;
        const short *deltas = & interp[fy&FRACMASK][256];
        for(GPixel const * const edest = (GPixel const *)dest+bufw;
          dest<edest;upper++,lower++,dest++)
        {
          const int lower_r = lower->r;
          const int delta_r = deltas[(int)upper->r - lower_r];
          dest->r = lower_r + delta_r;
          const int lower_g = lower->g;
          const int delta_g = deltas[(int)upper->g - lower_g];
          dest->g = lower_g + delta_g;
          const int lower_b = lower->b;
          const int delta_b = deltas[(int)upper->b - lower_b];
          dest->b = lower_b + delta_b;
        }
      }
      // Perform horizontal interpolation
      {
        // Prepare for side effects
        lbuffer[0]   = lbuffer[1];
        lbuffer[bufw+1] = lbuffer[bufw];
        GPixel *line = lbuffer+1-required_red.xmin;
        GPixel *dest  = output[y-desired_output.ymin];
        // Loop horizontally
        for (int x=desired_output.xmin; x<desired_output.xmax; x++,dest++)
          {
            const int n = hcoord[x];
            const GPixel *lower = line + (n>>FRACBITS);
            const short *deltas = &interp[n&FRACMASK][256];
            const int lower_r = lower[0].r;
            const int delta_r = deltas[(int)lower[1].r - lower_r];
            dest->r = lower_r + delta_r;
            const int lower_g = lower[0].g;
            const int delta_g = deltas[(int)lower[1].g - lower_g];
            dest->g = lower_g + delta_g;
            const int lower_b = lower[0].b;
            const int delta_b = deltas[(int)lower[1].b - lower_b];
            dest->b = lower_b + delta_b;
          }
      }
    }
  // Free temporaries
  gp1.resize(0);
  gp2.resize(0);
  glbuffer.resize(0);
}



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

