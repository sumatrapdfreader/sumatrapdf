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

#include "GMapAreas.h"
#include "GException.h"
#include "debug.h"

#include <math.h>
#include <stdio.h>


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


/****************************************************************************
***************************** GMapArea definition ***************************
****************************************************************************/

const char GMapArea::MAPAREA_TAG[] = 		"maparea";
const char GMapArea::RECT_TAG[] = 		"rect";
const char GMapArea::POLY_TAG[] = 		"poly";
const char GMapArea::OVAL_TAG[] = 		"oval";
const char GMapArea::NO_BORDER_TAG[] = 		"none";
const char GMapArea::XOR_BORDER_TAG[] = 	"xor";
const char GMapArea::SOLID_BORDER_TAG[] = 	"border";
const char GMapArea::SHADOW_IN_BORDER_TAG[] = 	"shadow_in";
const char GMapArea::SHADOW_OUT_BORDER_TAG[] = 	"shadow_out";
const char GMapArea::SHADOW_EIN_BORDER_TAG[] = 	"shadow_ein";
const char GMapArea::SHADOW_EOUT_BORDER_TAG[] = "shadow_eout";
const char GMapArea::BORDER_AVIS_TAG[] = 	"border_avis";
const char GMapArea::HILITE_TAG[] = 		"hilite";
const char GMapArea::URL_TAG[] = 		"url";
const char GMapArea::TARGET_SELF[] = 		"_self";
static const char zero_width[] = ERR_MSG("GMapAreas.zero_width");
static const char zero_height[] = ERR_MSG("GMapAreas.zero_height");
static const char width_1[] = ERR_MSG("GMapAreas.width_1");
static const char width_3_32 [] = ERR_MSG("GMapAreas.width_3-32");
static const char error_poly_border [] = ERR_MSG("GMapAreas.poly_border");
static const char error_poly_hilite [] = ERR_MSG("GMapAreas.poly_hilite");
static const char error_oval_border [] = ERR_MSG("GMapAreas.oval_border");
static const char error_oval_hilite [] = ERR_MSG("GMapAreas.oval_hilite");
static const char error_too_few_points [] = ERR_MSG("GMapAreas.too_few_points");
static const char error_intersect [] = ERR_MSG("GMapAreas.intersect");

GMapArea::~GMapArea() {}

GMapRect::~GMapRect() {}

GMapPoly::~GMapPoly() {}

GMapOval::~GMapOval() {}

void
GMapArea::initialize_bounds(void)
{
   xmin=gma_get_xmin();
   xmax=gma_get_xmax();
   ymin=gma_get_ymin();
   ymax=gma_get_ymax();
   bounds_initialized=true;
}

int
GMapArea::get_xmin(void) const
{
   if (!bounds_initialized)
     const_cast<GMapArea *>(this)->initialize_bounds();
   return xmin;
}

int
GMapArea::get_ymin(void) const
{
   if (!bounds_initialized)
     const_cast<GMapArea *>(this)->initialize_bounds();
   return ymin;
}

int
GMapArea::get_xmax(void) const
{
   if (!bounds_initialized)
     const_cast<GMapArea *>(this)->initialize_bounds();
   return xmax;
}

int
GMapArea::get_ymax(void) const
{
   if (!bounds_initialized)
     const_cast<GMapArea *>(this)->initialize_bounds();
   return ymax;
}

GRect
GMapArea::get_bound_rect(void) const
{
   return GRect(get_xmin(), get_ymin(), get_xmax()-get_xmin(),
		get_ymax()-get_ymin());
}

void
GMapArea::move(int dx, int dy)
{
   if (dx || dy)
   {
     if (bounds_initialized)
     {
        xmin+=dx;
        ymin+=dy;
        xmax+=dx;
        ymax+=dy;
     }
     gma_move(dx, dy);
   }
}

void
GMapArea::resize(int new_width, int new_height)
{
   if (get_xmax()-get_xmin()!=new_width ||
       get_ymax()-get_ymin()!=new_height)
   {
     gma_resize(new_width, new_height);
     bounds_initialized=false;
   }
}

void
GMapArea::transform(const GRect & grect)
{
   if (grect.xmin!=get_xmin() || grect.ymin!=get_ymin() ||
       grect.xmax!=get_xmax() || grect.ymax!=get_ymax())
   {
     gma_transform(grect);
     bounds_initialized=false;
   }
}

char const * const
GMapArea::check_object(void)
{
   char const *retval;
   if (get_xmax()==get_xmin())
   {
     retval=zero_width;
   }
   else if (get_ymax()==get_ymin())
   {
     retval=zero_height;
   }
   else if ((border_type==XOR_BORDER ||
       border_type==SOLID_BORDER) && border_width!=1)
   {
     retval=width_1;
   }
   else if ((border_type==SHADOW_IN_BORDER ||
       border_type==SHADOW_OUT_BORDER ||
       border_type==SHADOW_EIN_BORDER ||
       border_type==SHADOW_EOUT_BORDER)&&
       (border_width<3 || border_width>32))
   {
     retval=width_3_32;
   }else
   {
     retval=gma_check_object();
   }
   return retval;
}

bool
GMapArea::is_point_inside(int x, int y) const
{
   if (!bounds_initialized)
     const_cast<GMapArea *>(this)->initialize_bounds();
   return (x>=xmin && x<xmax && y>=ymin && y<ymax) ?
	      gma_is_point_inside(x, y) : false;
}

static GUTF8String make_c_string(GUTF8String string)
{
  GUTF8String buffer;
  const char *data = (const char*)string;
  int length = string.length();
  buffer = GUTF8String("\"");
  while (*data && length>0) 
    {
      int span = 0;
      while (span<length && (unsigned char)(data[span])>=0x20 && 
             data[span]!=0x7f && data[span]!='"' && data[span]!='\\' )
        span++;
      if (span > 0) 
        {  
          buffer = buffer + GUTF8String(data, span);
          data += span;
          length -= span;
        }  
      else 
        {
          char buf[8];
          static const char *tr1 = "\"\\tnrbf";
          static const char *tr2 = "\"\\\t\n\r\b\f";
          sprintf(buf,"\\%03o", (int)(((unsigned char*)data)[span]));
          for (int i=0; tr2[i]; i++)
            if (data[span] == tr2[i])
              buf[1] = tr1[i];
          if (buf[1]<'0' || buf[1]>'3')
            buf[2] = 0;
          buffer = buffer + GUTF8String(buf);
          data += 1;
          length -= 1;
        }
    }
  buffer = buffer + GUTF8String("\"");
  return buffer;
}

GUTF8String
GMapArea::print(void)
{
      // Make this hard check to make sure, that *no* illegal GMapArea
      // can be stored into a file.
   const char * const errors=check_object();
   if (errors[0])
   {
     G_THROW(errors);
   }
   
   GUTF8String url1 = make_c_string((GUTF8String)url);
   GUTF8String target1 = make_c_string(target);
   GUTF8String comment1 = make_c_string(comment);
   
   GUTF8String border_color_str;
   border_color_str.format("#%02X%02X%02X",
	   (border_color & 0xff0000) >> 16,
	   (border_color & 0xff00) >> 8,
	   (border_color & 0xff));

   static const GUTF8String left('(');
   static const GUTF8String right(')');
   static const GUTF8String space(' ');
   GUTF8String border_type_str;
   switch(border_type)
   {
      case NO_BORDER:
        border_type_str=left+NO_BORDER_TAG+right;
        break;
      case XOR_BORDER:
        border_type_str=left+XOR_BORDER_TAG+right;
        break;
      case SOLID_BORDER:
        border_type_str=left+SOLID_BORDER_TAG+space+border_color_str+right;
        break;
      case SHADOW_IN_BORDER:
        border_type_str=left+SHADOW_IN_BORDER_TAG+space+GUTF8String(border_width)+right;
        break;
      case SHADOW_OUT_BORDER:
        border_type_str=left+SHADOW_OUT_BORDER_TAG+space+GUTF8String(border_width)+right;
        break;
      case SHADOW_EIN_BORDER:
        border_type_str=left+SHADOW_EIN_BORDER_TAG+space+GUTF8String(border_width)+right;
        break;
      case SHADOW_EOUT_BORDER:
        border_type_str=left+SHADOW_EOUT_BORDER_TAG+space+GUTF8String(border_width)+right;
        break;
      default:
        border_type_str=left+XOR_BORDER_TAG+right;
        break;
   }

   GUTF8String hilite_str;
   if (hilite_color!=0xffffffff)
   {
      hilite_str.format("(%s #%02X%02X%02X)",
	      HILITE_TAG, (hilite_color & 0xff0000) >> 16,
	      (hilite_color & 0xff00) >> 8,
	      (hilite_color & 0xff));
   }
   
   GUTF8String URL;
   if (target1==TARGET_SELF)
   {
      URL=url1;
   }else
   {
      URL=left+URL_TAG+space+url1+space+target1+right;
   }

   GUTF8String total=left+MAPAREA_TAG+space+URL+space+comment1+space+gma_print()+border_type_str;
   if (border_always_visible)
     total+=space+left+BORDER_AVIS_TAG+right;
   if ( hilite_str.length() > 0 )
     total+=space+hilite_str;
   total+=right;
   return total;
}

/*
void 
GMapArea::map(GRectMapper &mapper)
{
    get_bound_rect();
    GRect rect = GRect(xmin, ymin, xmax, ymax);
    mapper.map(rect);
    xmin = rect.xmin;
    ymin = rect.ymin;
    xmax = rect.xmax;
    ymax = rect.ymax;
    clear_bounds();
}
void 
GMapArea::unmap(GRectMapper &mapper)
{
    get_bound_rect();
    GRect rect = GRect(xmin, ymin, xmax, ymax);
    mapper.unmap(rect);
    xmin = rect.xmin;
    ymin = rect.ymin;
    xmax = rect.xmax;
    ymax = rect.ymax;
    clear_bounds();
}
*/


/// Virtual function generating a list of defining coordinates
/// (default are the opposite corners of the enclosing rectangle)
void GMapArea::get_coords( GList<int> & CoordList ) const
{
  CoordList.append( get_xmin() );
  CoordList.append( get_ymin() );
  CoordList.append( get_xmax() );
  CoordList.append( get_ymax() );
}


/****************************************************************************
**************************** GMapRect definition ****************************
****************************************************************************/

void
GMapRect::gma_resize(int new_width, int new_height)
{
   xmax=xmin+new_width;
   ymax=ymin+new_height;
}

void
GMapRect::gma_transform(const GRect & grect)
{
   xmin=grect.xmin; ymin=grect.ymin;
   xmax=grect.xmax; ymax=grect.ymax;
}

GUTF8String
GMapRect::gma_print(void)
{
   GUTF8String buffer;
   return buffer.format("(%s %d %d %d %d) ",
	   RECT_TAG, xmin, ymin, xmax-xmin, ymax-ymin);
}

void 
GMapRect::map(GRectMapper &mapper)
{
    get_bound_rect();
    GRect rect;
    rect.xmin = xmin;
    rect.xmax = xmax;
    rect.ymin = ymin;
    rect.ymax = ymax;
    mapper.map(rect);
    xmin = rect.xmin;
    ymin = rect.ymin;
    xmax = rect.xmax;
    ymax = rect.ymax;
    clear_bounds();
}
void 
GMapRect::unmap(GRectMapper &mapper)
{
    get_bound_rect();
    GRect rect;
    rect.xmin = xmin;
    rect.xmax = xmax;
    rect.ymin = ymin;
    rect.ymax = ymax;
    mapper.unmap(rect);
    xmin = rect.xmin;
    ymin = rect.ymin;
    xmax = rect.xmax;
    ymax = rect.ymax;
    clear_bounds();
}

/****************************************************************************
**************************** GMapPoly definition ****************************
****************************************************************************/

inline int
GMapPoly::sign(int x) { return x<0 ? -1 : x>0 ? 1 : 0; }

bool
GMapPoly::does_side_cross_rect(const GRect & grect, int side)
{
   int x1=xx[side], x2=xx[(side+1)%points];
   int y1=yy[side], y2=yy[(side+1)%points];
   int xmin=x1<x2 ? x1 : x2;
   int ymin=y1<y2 ? y1 : y2;
   int xmax=x1+x2-xmin;
   int ymax=y1+y2-ymin;

   if (xmax<grect.xmin || xmin>grect.xmax ||
       ymax<grect.ymin || ymin>grect.ymax)
     return false;

   return
     (x1>=grect.xmin && x1<=grect.xmax && y1>=grect.ymin && y1<=grect.ymax) ||
     (x2>=grect.xmin && x2<=grect.xmax && y2>=grect.ymin && y2<=grect.ymax) ||
     do_segments_intersect(grect.xmin, grect.ymin, grect.xmax, grect.ymax,
			   x1, y1, x2, y2) ||
     do_segments_intersect(grect.xmax, grect.ymin, grect.xmin, grect.ymax,
			   x1, y1, x2, y2);
}

bool
GMapPoly::is_projection_on_segment(int x, int y, int x1, int y1, int x2, int y2)
{
   int res1=(x-x1)*(x2-x1)+(y-y1)*(y2-y1);
   int res2=(x-x2)*(x2-x1)+(y-y2)*(y2-y1);
   return sign(res1)*sign(res2)<=0;
}

bool
GMapPoly::do_segments_intersect(int x11, int y11, int x12, int y12,
				int x21, int y21, int x22, int y22)
{
   int res11=(x11-x21)*(y22-y21)-(y11-y21)*(x22-x21);
   int res12=(x12-x21)*(y22-y21)-(y12-y21)*(x22-x21);
   int res21=(x21-x11)*(y12-y11)-(y21-y11)*(x12-x11);
   int res22=(x22-x11)*(y12-y11)-(y22-y11)*(x12-x11);
   if (!res11 && !res12)
   {
      // Segments are on the same line
      return
	 is_projection_on_segment(x11, y11, x21, y21, x22, y22) ||
	 is_projection_on_segment(x12, y12, x21, y21, x22, y22) ||
	 is_projection_on_segment(x21, y21, x11, y11, x12, y12) ||
	 is_projection_on_segment(x22, y22, x11, y11, x12, y12);
   }
   int sign1=sign(res11)*sign(res12);
   int sign2=sign(res21)*sign(res22);
   return sign1<=0 && sign2<=0;
}

bool
GMapPoly::are_segments_parallel(int x11, int y11, int x12, int y12,
				int x21, int y21, int x22, int y22)
{
   return (x12-x11)*(y22-y21)-(y12-y11)*(x22-x21)==0;
}

char const * const
GMapPoly::check_data(void)
{
  if ((open && points<2) || (!open && points<3))
    return error_too_few_points;
  for(int i=0;i<sides;i++)
    {
      for(int j=i+2;j<sides;j++)
	{
	  if (i != (j+1)%points )
	    if (do_segments_intersect(xx[i], yy[i], xx[i+1], yy[i+1],
				      xx[j], yy[j], xx[(j+1)%points], yy[(j+1)%points]))
	      return error_intersect;
	}
    }
  return "";
}

void
GMapPoly::optimize_data(void)
{
  // Removing segments of length zero
  int i;
  for(i=0;i<sides;i++)
    {
      while(xx[i]==xx[(i+1)%points] && yy[i]==yy[(i+1)%points])
	{
	  for(int k=(i+1)%points;k<points-1;k++)
	    {
	      xx[k]=xx[k+1]; yy[k]=yy[k+1];
	    }
	  points--; sides--;
	  if (!points) return;
	}
    }
  // Concatenating consequitive parallel segments
  for(i=0;i<sides;i++)
    {
      while(((open && i+1<sides) || !open) &&
	    are_segments_parallel(xx[i], yy[i],
				  xx[(i+1)%points], yy[(i+1)%points],
				  xx[(i+1)%points], yy[(i+1)%points],
				  xx[(i+2)%points], yy[(i+2)%points]))
	{
	  for(int k=(i+1)%points;k<points-1;k++)
	    {
	      xx[k]=xx[k+1]; yy[k]=yy[k+1];
	    }
	  points--; sides--;
	  if (!points) return;
	}
    }
}

bool
GMapPoly::gma_is_point_inside(const int xin, const int yin) const
{
   if (open)
     return false;

   int xfar=get_xmax()+(get_xmax()-get_xmin());

   int intersections=0;
   for(int i=0;i<points;i++)
   {
      int res1=yy[i]-yin;
      if (!res1) continue;
      int res2, isaved=i;
      while(!(res2=yy[(i+1)%points]-yin)) i++;
      if (isaved!=i)
      {
	 // Some points fell exactly on the line
	 if ((xx[(isaved+1)%points]-xin)*
	     (xx[i%points]-xin)<=0)
	 {
	    // Test point is exactly on the boundary
	    return true;
	 }
      }
      if ((res1<0 && res2>0) || (res1>0 && res2<0))
      {
	 int x1=xx[i%points], y1=yy[i%points];
	 int x2=xx[(i+1)%points], y2=yy[(i+1)%points];
	 int _res1=(xin-x1)*(y2-y1)-(yin-y1)*(x2-x1);
	 int _res2=(xfar-x1)*(y2-y1)-(yin-y1)*(x2-x1);
	 if (!_res1 || !_res2)
	 {
	    // The point is on this boundary
	    return true;
	 }
	 if (sign(_res1)*sign(_res2)<0) intersections++;
      }
   }
   return (intersections % 2)!=0;
}

int
GMapPoly::gma_get_xmin(void) const
{
   int x=xx[0];
   for(int i=1;i<points;i++)
      if (x>xx[i]) x=xx[i];
   return x;
}

int
GMapPoly::gma_get_xmax(void) const
{
   int x=xx[0];
   for(int i=1;i<points;i++)
      if (x<xx[i]) x=xx[i];
   return x+1;
}

int
GMapPoly::gma_get_ymin(void) const
{
   int y=yy[0];
   for(int i=1;i<points;i++)
      if (y>yy[i]) y=yy[i];
   return y;
}

int
GMapPoly::gma_get_ymax(void) const
{
   int y=yy[0];
   for(int i=1;i<points;i++)
      if (y<yy[i]) y=yy[i];
   return y+1;
}

void
GMapPoly::gma_move(int dx, int dy)
{
   for(int i=0;i<points;i++)
   {
      xx[i]+=dx; yy[i]+=dy;
   }
}

void
GMapPoly::gma_resize(int new_width, int new_height)
{
   int width=get_xmax()-get_xmin();
   int height=get_ymax()-get_ymin();
   int xmin=get_xmin(), ymin=get_ymin();
   for(int i=0;i<points;i++)
   {
      xx[i]=xmin+(xx[i]-xmin)*new_width/width;
      yy[i]=ymin+(yy[i]-ymin)*new_height/height;
   }
}

void
GMapPoly::gma_transform(const GRect & grect)
{
   int width=get_xmax()-get_xmin();
   int height=get_ymax()-get_ymin();
   int xmin=get_xmin(), ymin=get_ymin();
   for(int i=0;i<points;i++)
   {
      xx[i]=grect.xmin+(xx[i]-xmin)*grect.width()/width;
      yy[i]=grect.ymin+(yy[i]-ymin)*grect.height()/height;
   }
}

char const * const
GMapPoly::gma_check_object(void) const
{
   const char * str;
   str=(border_type!=NO_BORDER &&
        border_type!=SOLID_BORDER &&
        border_type!=XOR_BORDER) ? error_poly_border:
       ((hilite_color!=0xffffffff) ? error_poly_hilite:"");
   return str;
}

GMapPoly::GMapPoly(const int * _xx, const int * _yy, int _points, bool _open) :
   open(_open), points(_points)
{
   sides=points-(open!=0);
   
   xx.resize(points-1); yy.resize(points-1);
   for(int i=0;i<points;i++)
   {
      xx[i]=_xx[i]; yy[i]=_yy[i];
   }
   optimize_data();
   char const * const res=check_data();
   if (res[0])
     G_THROW(res);
}

int      
GMapPoly::add_vertex(int x, int y)
{
    points++;
    sides=points-(open!=0);

    xx.resize(points-1); yy.resize(points-1);
    xx[points-1] = x;
    yy[points-1] = y;

    return points;
}

void
GMapPoly::close_poly()
{
    open = false;
    sides=points;
}

GUTF8String
GMapPoly::gma_print(void)
{
   static const GUTF8String space(' ');
   GUTF8String res=GUTF8String('(')+POLY_TAG+space;
   for(int i=0;i<points;i++)
   {
      GUTF8String buffer;
      res+=buffer.format("%d %d ", xx[i], yy[i]);
   }
   res.setat(res.length()-1, ')');
   res+=space;
   return res;
}

/// Virtual function generating a list of defining coordinates
void GMapPoly::get_coords( GList<int> & CoordList ) const
{
  for(int i = 0 ; i < points ; i++)
  {
    CoordList.append( xx[i] );
    CoordList.append( yy[i] );
  }
}

void 
GMapPoly::map(GRectMapper &mapper)
{
    get_bound_rect();
    for(int i=0; i<points; i++)
    {
        mapper.map(xx[i], yy[i]);
    }
    clear_bounds();
}

void 
GMapPoly::unmap(GRectMapper &mapper)
{
    get_bound_rect();
    for(int i=0; i<points; i++)
    {
        mapper.unmap(xx[i], yy[i]);
    }
    clear_bounds();
}



/****************************************************************************
**************************** GMapOval definition ****************************
****************************************************************************/

void
GMapOval::gma_resize(int new_width, int new_height)
{
   xmax=xmin+new_width;
   ymax=ymin+new_height;
   initialize();
}

void
GMapOval::gma_transform(const GRect & grect)
{
   xmin=grect.xmin; ymin=grect.ymin;
   xmax=grect.xmax; ymax=grect.ymax;
   initialize();
}

bool
GMapOval::gma_is_point_inside(const int x, const int y) const
{
   return
      sqrt((double)((x-xf1)*(x-xf1)+(y-yf1)*(y-yf1))) +
      sqrt((double)((x-xf2)*(x-xf2)+(y-yf2)*(y-yf2))) <= 2*rmax;
}

char const * const
GMapOval::gma_check_object(void) const
{
   return (border_type!=NO_BORDER &&
       border_type!=SOLID_BORDER &&
       border_type!=XOR_BORDER)?error_oval_border:
      ((hilite_color!=0xffffffff) ? error_oval_hilite:"");
}

void
GMapOval::initialize(void)
{
   int xc=(xmax+xmin)/2;
   int yc=(ymax+ymin)/2;
   int f;
   
   a=(xmax-xmin)/2;
   b=(ymax-ymin)/2;
   if (a>b)
   {
      rmin=b; rmax=a;
      f=(int) sqrt((double)(rmax*rmax-rmin*rmin));
      xf1=xc+f; xf2=xc-f; yf1=yf2=yc;
   } else
   {
      rmin=a; rmax=b;
      f=(int) sqrt((double)(rmax*rmax-rmin*rmin));
      yf1=yc+f; yf2=yc-f; xf1=xf2=xc;
   }
}

GMapOval::GMapOval(const GRect & rect) : xmin(rect.xmin), ymin(rect.ymin),
   xmax(rect.xmax), ymax(rect.ymax)
{
   initialize();
}

GUTF8String
GMapOval::gma_print(void)
{
   GUTF8String buffer;
   return buffer.format("(%s %d %d %d %d) ",
	   OVAL_TAG, xmin, ymin, xmax-xmin, ymax-ymin);
}

void 
GMapOval::map(GRectMapper &mapper)
{
    get_bound_rect();
    GRect rect;
    rect.xmin = xmin;
    rect.xmax = xmax;
    rect.ymin = ymin;
    rect.ymax = ymax;
    mapper.map(rect);
    xmin = rect.xmin;
    ymin = rect.ymin;
    xmax = rect.xmax;
    ymax = rect.ymax;
    clear_bounds();
    initialize();
}

void 
GMapOval::unmap(GRectMapper &mapper)
{
    get_bound_rect();
    GRect rect;
    rect.xmin = xmin;
    rect.xmax = xmax;
    rect.ymin = ymin;
    rect.ymax = ymax;
    mapper.unmap(rect);
    xmin = rect.xmin;
    ymin = rect.ymin;
    xmax = rect.xmax;
    ymax = rect.ymax;
    clear_bounds();
    initialize();
}

GMapArea::GMapArea(void) : target("_self"), border_type(NO_BORDER),
   border_always_visible(false), border_color(0xff), border_width(1),
   hilite_color(0xffffffff), bounds_initialized(0) {}

GMapRect::GMapRect(void) : xmin(0), ymin(0), xmax(0), ymax(0) {}

GMapRect::GMapRect(const GRect & rect) : xmin(rect.xmin), ymin(rect.ymin),
   xmax(rect.xmax), ymax(rect.ymax) {}

GMapRect &
GMapRect::operator=(const GRect & rect)
{
   xmin=rect.xmin;
   xmax=rect.xmax;
   ymin=rect.ymin;
   ymax=rect.ymax;
   return *this;
}

void
GMapRect::gma_move(int dx, int dy)
{
   xmin+=dx;
   xmax+=dx;
   ymin+=dy;
   ymax+=dy;
}

bool
GMapRect::gma_is_point_inside(const int x, const int y) const
{
   return (x>=xmin)&&(x<xmax)&&(y>=ymin)&&(y<ymax);
}

GP<GMapArea>
GMapRect::get_copy(void) const { return new GMapRect(*this); }

GMapPoly::GMapPoly(void) : points(0), sides(0) {}

void
GMapPoly::move_vertex(int i, int x, int y)
{
   xx[i]=x; yy[i]=y;
   clear_bounds();
}

GP<GMapArea>
GMapPoly::get_copy(void) const { return new GMapPoly(*this); }

GMapOval::GMapOval(void) : xmin(0), ymin(0), xmax(0), ymax(0) {}

void
GMapOval::gma_move(int dx, int dy)
{
   xmin+=dx; xmax+=dx; ymin+=dy; ymax+=dy;
   xf1+=dx; yf1+=dy; xf2+=dx; yf2+=dy;
}

GP<GMapArea>
GMapOval::get_copy(void) const
{
  return new GMapOval(*this);
}

static GUTF8String
GMapArea2xmltag(const GMapArea &area,const GUTF8String &coords)
{
  GUTF8String retval("<AREA coords=\""
    +coords+"\" shape=\""+area.get_shape_name()+"\" "
    +"alt=\""+area.comment.toEscaped()+"\" ");
  if(area.url.length())
  {
    retval+="href=\""+area.url+"\" ";
  }else
  {
    retval+="nohref=\"nohref\" ";
  }
  if(area.target.length())
  {
    retval+="target=\""+area.target.toEscaped()+"\" ";
  }
  //  highlight
  if( area.hilite_color != GMapArea::NO_HILITE &&
      area.hilite_color != GMapArea::XOR_HILITE )
  {
    retval+=GUTF8String().format( "highlight=\"#%06X\" ", area.hilite_color );
  }
  const char *b_type="none";
  switch( area.border_type )
  {
  case GMapArea::NO_BORDER:
    b_type = "none";
    break;
  case GMapArea::XOR_BORDER:
    b_type = "xor";
    break;
  case GMapArea::SOLID_BORDER:
    b_type = "solid";
    break;
  case GMapArea::SHADOW_IN_BORDER:
    b_type = "shadowin";
    break;
  case GMapArea::SHADOW_OUT_BORDER:
    b_type = "shadowout";
    break;
  case GMapArea::SHADOW_EIN_BORDER:
    b_type = "etchedin";
    break;
  case GMapArea::SHADOW_EOUT_BORDER:
    b_type = "etchedout";
    break;
  }
  retval=retval+"bordertype=\""+b_type+"\" ";
  if( area.border_type != GMapArea::NO_BORDER)
  {
    retval+="bordercolor=\""+GUTF8String().format("#%06X",area.border_color)
      +"\" border=\""+GUTF8String(area.border_width)+"\" ";
  }
  if(area.border_always_visible )
    retval=retval+"visible=\"visible\" ";
  return retval+"/>\n";
}

GUTF8String
GMapRect::get_xmltag(const int height) const
{
  return GMapArea2xmltag( *this, GUTF8String(get_xmin())
    +","+GUTF8String(height-1-get_ymax())
    +","+GUTF8String(get_xmax())
    +","+GUTF8String(height-1-get_ymin()));
#if 0
  GUTF8String retval;
  return retval;
#endif
}

GUTF8String
GMapOval::get_xmltag(const int height) const
{ 
  return GMapArea2xmltag( *this, GUTF8String(get_xmin())
    +","+GUTF8String(height-1-get_ymax())
    +","+GUTF8String(get_xmax())
    +","+GUTF8String(height-1-get_ymin()));
#if 0
  GUTF8String retval;
  return retval;
#endif
}

GUTF8String
GMapPoly::get_xmltag(const int height) const
{
  GList<int> CoordList;
  get_coords(CoordList);
  GPosition pos=CoordList;
  GUTF8String retval;
  if(pos)
  {
    GUTF8String coords(CoordList[pos]);
    while(++pos)
    {
      coords+=","+GUTF8String(height-1-CoordList[pos]);
      if(! ++pos)
        break;
      coords+=","+GUTF8String(CoordList[pos]);
    }
    retval=GMapArea2xmltag( *this, coords);
  }
  return retval;
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

