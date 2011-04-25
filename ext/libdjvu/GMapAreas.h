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

#ifndef _GMAPAREAS_H
#define _GMAPAREAS_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


#include "GSmartPointer.h"
#include "GContainer.h"
#include "GString.h"
#include "GRect.h"
#include "GURL.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


/** @name GMapAreas.h

    Files #"GMapAreas.h"# and #"GMapAreas.cpp"# implement base objects
    used by the plugin to display and manage hyperlinks and highlighted
    areas inside a \Ref{DjVuImage} page.

    The currently supported areas can be rectangular (\Ref{GMapRect}),
    elliptical (\Ref{GMapOval}) and polygonal (\Ref{GMapPoly}). Every
    map area besides the definition of its shape contains information
    about display style and optional {\bf URL}, which it may refer to.
    If this {\bf URL} is not empty then the map area will work like a
    hyperlink.

    The classes also implement some useful functions to ease geometry
    manipulations

    @memo Definition of base map area classes
    @author Andrei Erofeev <eaf@geocities.com>
*/
//@{


// ---------- GMAPAREA ---------

/** This is the base object for all map areas. It defines some standard
    interface to access the geometrical properties of the areas and
    describes the area itsef:
    \begin{itemize}
       \item #url# If the optional #URL# is specified, the map area will
             also work as a hyperlink meaning that if you click it with
	     your mouse pointer, the browser will be advised to load
	     the page referenced by the #URL#.
       \item #target# Defines where the specified #URL# should be loaded
       \item #comment# This is a string displayed in a status line or in
             a popup window when the mouse pointer moves over the hyperlink
	     area
       \item #border_type#, #border_color# and #border_width# describes
             how the area border should be drawn
       \item #area_color# describes how the area should be highlighted.
    \end{itemize}

    The map areas can be displayed using two different techniques, which
    can be combined together:
    \begin{itemize}
       \item Visible border. The border of a map area can be drawn in several
             different ways (like #XOR_BORDER# or #SHADOW_IN_BORDER#).
	     It can be made always visible, or appearing only when the
	     mouse pointer moves over the map area.
       \item Highlighted contents. Contents of rectangular map areas can
             also be highlighted with some given color.
    \end{itemize}
*/

class GMapArea : public GPEnabled
{
protected:
   GMapArea(void);
public:
//      // Default creator.
//   static GP<GMapArea> create(void) {return new GMapArea();}

      /// Virtual destructor.
   virtual ~GMapArea();

   static const char MAPAREA_TAG [];
   static const char RECT_TAG [];
   static const char POLY_TAG [];
   static const char OVAL_TAG [];
   static const char NO_BORDER_TAG [];
   static const char XOR_BORDER_TAG [];
   static const char SOLID_BORDER_TAG [];
   static const char SHADOW_IN_BORDER_TAG [];
   static const char SHADOW_OUT_BORDER_TAG [];
   static const char SHADOW_EIN_BORDER_TAG [];
   static const char SHADOW_EOUT_BORDER_TAG [];
   static const char BORDER_AVIS_TAG [];
   static const char HILITE_TAG [];
   static const char URL_TAG [];
   static const char TARGET_SELF [];

   enum BorderType { NO_BORDER=0, XOR_BORDER=1, SOLID_BORDER=2,
		     SHADOW_IN_BORDER=3, SHADOW_OUT_BORDER=4,
		     SHADOW_EIN_BORDER=5, SHADOW_EOUT_BORDER=6 };

   enum Special_Hilite_Color{ NO_HILITE=0xFFFFFFFF, XOR_HILITE=0xFF000000};

   // Enumeration for reporting the type of map area. "MapUnknown" is reported
   // for objects of type GMapArea (there shouldn't be any).
   enum MapAreaType { UNKNOWN, RECT, OVAL, POLY };

      /** Optional URL which this map area can be associated with.
	  If it's not empty then clicking this map area with the mouse
	  will make the browser load the HTML page referenced by
	  this #url#.  Note: This may also be a relative URL, so the
          GURL class is not used. */
   GUTF8String	url;
      /** The target for the #URL#. Standard targets are:
	  \begin{itemize}
	     \item #_blank# - Load the link in a new blank window
	     \item #_self# - Load the link into the plugin window
	     \item #_top# - Load the link into the top-level frame
	  \end{itemize} */
   GUTF8String	target;
      /** Comment (displayed in a status line or as a popup hint when
	  the mouse pointer moves over the map area */
   GUTF8String	comment;
      /** Border type. Defines how the map area border should be drawn
	  \begin{itemize}
	     \item #NO_BORDER# - No border drawn
	     \item #XOR_BORDER# - The border is drawn using XOR method.
	     \item #SOLID_BORDER# - The border is drawn as a solid line
	           of a given color.
	     \item #SHADOW_IN_BORDER# - Supported for \Ref{GMapRect} only.
	     	   The map area area looks as if it was "pushed-in".
	     \item #SHADOW_OUT_BORDER# - The opposite of #SHADOW_OUT_BORDER#
	     \item #SHADOW_EIN_BORDER# - Also for \Ref{GMapRect} only.
	     	   Is translated as "shadow etched in"
	     \item #SHADOW_EOUT_BORDER# - The opposite of #SHADOW_EIN_BORDER#.
	  \end{itemize} */
   BorderType	border_type;
      /** If #TRUE#, the border will be made always visible. Otherwise
	  it will be drawn when the mouse moves over the map area. */
   bool		border_always_visible;
      /// Border color (when relevant) in #0x00RRGGBB# format
   unsigned long int	border_color;
      /// Border width in pixels
   int		border_width;
      /** Specified a color for highlighting the internal area of the map
	  area. Will work with rectangular map areas only. The color is
	  specified in \#00RRGGBB format. A special value of \#FFFFFFFF disables
          highlighting and \#FF000000 is for XOR highlighting. */
   unsigned long int	hilite_color;

      /// Returns 1 if the given point is inside the hyperlink area
   bool		is_point_inside(int x, int y) const;

      /// Returns xmin of the bounding rectangle
   int		get_xmin(void) const;
      /// Returns ymin of the bounding rectangle
   int		get_ymin(void) const;
      /** Returns xmax of the bounding rectangle. In other words, if #X# is
	  a coordinate of the last point in the right direction, the
	  function will return #X+1# */
   int		get_xmax(void) const;
      /** Returns xmax of the bounding rectangle. In other words, if #Y# is
	  a coordinate of the last point in the top direction, the
	  function will return #Y+1# */
   int		get_ymax(void) const;
      /// Returns the hyperlink bounding rectangle
   GRect	get_bound_rect(void) const;
      /** Moves the hyperlink along the given vector. Is used by the
	  hyperlinks editor. */
   void		move(int dx, int dy);
      /** Resizes the hyperlink to fit new bounding rectangle while
	  keeping the (xmin, ymin) points at rest. */
   void		resize(int new_width, int new_height);
      /** Transforms the hyperlink to be within the specified rectangle */
   void		transform(const GRect & grect);
      /** Checks if the object is OK. Especially useful with \Ref{GMapPoly}
	  where edges may intersect. If there is a problem it returns a
	  string describing it. */
   char const *	const check_object(void);
      /** Stores the contents of the hyperlink object in a lisp-like format
	  for saving into #ANTa# chunk (see \Ref{DjVuAnno}) */
   GUTF8String	print(void);

   virtual GUTF8String get_xmltag(const int height) const=0;

      /// Virtual function returning the shape type.
   virtual MapAreaType const get_shape_type( void ) const { return UNKNOWN; };
      /// Virtual function returning the shape name.
   virtual char const * const	get_shape_name(void) const=0;
      /// Virtual function generating a copy of this object
   virtual GP<GMapArea>	get_copy(void) const=0;
      /// Virtual function generating a list of defining coordinates
      /// (default are the opposite corners of the enclosing rectangle)
   virtual void get_coords( GList<int> & CoordList ) const;
   /// Virtual function maps maparea from one area to another using mapper
   virtual void map(GRectMapper &mapper)=0;
   /// Virtual function unmaps maparea from one area to another using mapper
   virtual void unmap(GRectMapper &mapper)=0;

protected:
   virtual int		gma_get_xmin(void) const=0;
   virtual int		gma_get_ymin(void) const=0;
   virtual int		gma_get_xmax(void) const=0;
   virtual int		gma_get_ymax(void) const=0;
   virtual void		gma_move(int dx, int dy)=0;
   virtual void		gma_resize(int new_width, int new_height)=0;
   virtual void		gma_transform(const GRect & grect)=0;
   virtual bool		gma_is_point_inside(const int x, const int y) const=0;
   virtual char const * const	gma_check_object(void) const=0;
   virtual GUTF8String	gma_print(void)=0;
   
   void		clear_bounds(void) { bounds_initialized=0; }
private:
   int		xmin, xmax, ymin, ymax;
   bool		bounds_initialized;

   void		initialize_bounds(void);
};

// ---------- GMAPRECT ---------

/** Implements rectangular map areas. This is the only kind of map areas
    supporting #SHADOW_IN_BORDER#, #SHADOW_OUT_BORDER#, #SHADOW_EIN_BORDER#
    and #SHADOW_EOUT_BORDER# types of border and area highlighting. */

class GMapRect: public GMapArea
{
protected:
   GMapRect(void);
   GMapRect(const GRect & rect);
public:
   /// Default creator.
   static GP<GMapRect> create(void) {return new GMapRect();}
   /// Create with the specified GRect.
   static GP<GMapRect> create(const GRect &rect) {return new GMapRect(rect);}

   virtual ~GMapRect();

      /// Returns the width of the rectangle
   int		get_width(void) const { return xmax-xmin; }
      /// Returns the height of the rectangle
   int		get_height(void) const { return ymax-ymin; }

      /// Changes the #GMapRect#'s geometry
   GMapRect & operator=(const GRect & rect);

      /// Returns \Ref{GRect} describing the map area's rectangle
   operator GRect(void);
   
   virtual GUTF8String get_xmltag(const int height) const;
      /// Returns MapRect
   virtual MapAreaType const get_shape_type( void ) const { return RECT; };
      /// Returns #"rect"#
   virtual char const * const	get_shape_name(void) const;
      /// Returns a copy of the rectangle
   virtual GP<GMapArea>	get_copy(void) const;
      /// Virtual function maps rectangle from one area to another using mapper
   virtual void map(GRectMapper &mapper);
      /// Virtual function unmaps rectangle from one area to another using mapper
   virtual void unmap(GRectMapper &mapper);
protected:
   int			xmin, ymin, xmax, ymax;
   virtual int		gma_get_xmin(void) const;
   virtual int		gma_get_ymin(void) const;
   virtual int		gma_get_xmax(void) const;
   virtual int		gma_get_ymax(void) const;
   virtual void		gma_move(int dx, int dy);
   virtual void		gma_resize(int new_width, int new_height);
   virtual void		gma_transform(const GRect & grect);
   virtual bool		gma_is_point_inside(const int x, const int y) const;
   virtual char const * const gma_check_object(void) const;
   virtual GUTF8String	gma_print(void);
};

// ---------- GMAPPOLY ---------

/** Implements polygonal map areas. The only supported types of border
    are #NO_BORDER#, #XOR_BORDER# and #SOLID_BORDER#. Its contents can not
    be highlighted either. It's worth mentioning here that despite its
    name the polygon may be open, which basically makes it a broken line.
    This very specific mode is used by the hyperlink editor when creating
    the polygonal hyperlink. */

class GMapPoly : public GMapArea
{
protected:
   GMapPoly(void);
   GMapPoly(const int * xx, const int * yy, int points, bool open=false);
public:
   /// Default creator
   static GP<GMapPoly> create(void) {return new GMapPoly();}

   /// Create from specified coordinates.
   static GP<GMapPoly> create(
     const int xx[], const int yy[], const int points, const bool open=false)
   {return new GMapPoly(xx,yy,points,open);}

   /// Virtual destructor.
   virtual ~GMapPoly();

      /// Returns 1 if side #side# crosses the specified rectangle #rect#.
   bool		does_side_cross_rect(const GRect & grect, int side);

      /// Returns the number of vertices in the polygon
   int		get_points_num(void) const;

      /// Returns the number sides in the polygon
   int		get_sides_num(void) const;

      /// Returns x coordinate of vertex number #i#
   int		get_x(int i) const;
   
      /// Returns y coordinate of vertex number #i#
   int		get_y(int i) const;

      /// Moves vertex #i# to position (#x#, #y#)
   void		move_vertex(int i, int x, int y);

      /// Adds a new vertex and returns number of vertices in the polygon
   int      add_vertex(int x, int y);

      /// Closes the polygon if it is not closed
   void     close_poly();
      /// Optimizes the polygon 
   void		optimize_data(void);
      /// Checks validity of the polygon 
   char const * const	check_data(void);

   virtual GUTF8String get_xmltag(const int height) const;
      /// Returns MapPoly
   virtual MapAreaType const get_shape_type( void ) const { return POLY; };
      /// Returns #"poly"# all the time
   virtual char const * const 	get_shape_name(void) const;
      /// Returns a copy of the polygon
   virtual GP<GMapArea>	get_copy(void) const;
      /// Virtual function generating a list of defining coordinates
   void get_coords( GList<int> & CoordList ) const;
      /// Virtual function maps polygon from one area to another using mapper
   virtual void map(GRectMapper &mapper);
   /// Virtual function unmaps polygon from one area to another using mapper
   virtual void unmap(GRectMapper &mapper);
protected:
   virtual int		gma_get_xmin(void) const;
   virtual int		gma_get_ymin(void) const;
   virtual int		gma_get_xmax(void) const;
   virtual int		gma_get_ymax(void) const;
   virtual void		gma_move(int dx, int dy);
   virtual void		gma_resize(int new_width, int new_height);
   virtual void		gma_transform(const GRect & grect);
   virtual bool		gma_is_point_inside(const int x, const int y) const;
   virtual char const * const gma_check_object(void) const;
   virtual GUTF8String	gma_print(void);
private:
   bool		open;
   int		points, sides;
   GTArray<int>	xx, yy;
   static int	sign(int x);
   static bool	is_projection_on_segment(int x, int y, int x1, int y1, int x2, int y2);
   static bool	do_segments_intersect(int x11, int y11, int x12, int y12,
				      int x21, int y21, int x22, int y22);
   static bool	are_segments_parallel(int x11, int y11, int x12, int y12,
				      int x21, int y21, int x22, int y22);
};

// ---------- GMAPOVAL ---------

/** Implements elliptical map areas. The only supported types of border
    are #NO_BORDER#, #XOR_BORDER# and #SOLID_BORDER#. Its contents can not
    be highlighted either. */

class GMapOval: public GMapArea
{
protected:
   GMapOval(void);
   GMapOval(const GRect & rect);
public:
   /// Default creator.
   static GP<GMapOval> create(void) {return new GMapOval();}

   /// Create from the specified GRect.
   static GP<GMapOval> create(const GRect &rect) {return new GMapOval(rect);}

   /// Virtual destructor. 
   virtual ~GMapOval();

      /// Returns (xmax-xmin)/2
   int		get_a(void) const;
      /// Returns (ymax-ymin)/2
   int		get_b(void) const;
      /// Returns the lesser of \Ref{get_a}() and \Ref{get_b}()
   int		get_rmin(void) const;
      /// Returns the greater of \Ref{get_a}() and \Ref{get_b}()
   int		get_rmax(void) const;

   virtual GUTF8String get_xmltag(const int height) const;
      /// Returns MapOval
   virtual MapAreaType const get_shape_type( void ) const { return OVAL; };
      /// Returns #"oval"#
   virtual char const * const get_shape_name(void) const;
      /// Returns a copy of the oval
   virtual GP<GMapArea>	get_copy(void) const;
      /// Virtual function maps oval from one area to another using mapper
   virtual void map(GRectMapper &mapper);
      /// Virtual function unmaps oval from one area to another using mapper
   virtual void unmap(GRectMapper &mapper);
protected:
   virtual int		gma_get_xmin(void) const;
   virtual int		gma_get_ymin(void) const;
   virtual int		gma_get_xmax(void) const;
   virtual int		gma_get_ymax(void) const;
   virtual void		gma_move(int dx, int dy);
   virtual void		gma_resize(int new_width, int new_height);
   virtual void		gma_transform(const GRect & grect);
   virtual bool		gma_is_point_inside(const int x, const int y) const;
   virtual char const * const	gma_check_object(void) const;
   virtual GUTF8String	gma_print(void);
private:
   int		rmax, rmin;
   int		a, b;
   int		xf1, yf1, xf2, yf2;
   int		xmin, ymin, xmax, ymax;
   
   void		initialize(void);
};

inline
GMapRect::operator GRect(void)
{
  return GRect(xmin, ymin, xmax-xmin, ymax-ymin);
}

inline int
GMapRect::gma_get_xmin(void) const { return xmin; }

inline int
GMapRect::gma_get_ymin(void) const { return ymin; }

inline int
GMapRect::gma_get_xmax(void) const { return xmax; }

inline int
GMapRect::gma_get_ymax(void) const { return ymax; }

inline char const * const
GMapRect::gma_check_object(void)  const{ return ""; }

inline char const * const 
GMapRect::get_shape_name(void) const { return RECT_TAG; }

inline int
GMapPoly::get_points_num(void) const { return points; }

inline int
GMapPoly::get_sides_num(void) const { return sides; }

inline int
GMapPoly::get_x(int i) const { return xx[i]; }

inline int
GMapPoly::get_y(int i) const { return yy[i]; }

inline char const * const
GMapPoly::get_shape_name(void) const { return POLY_TAG; }

inline int
GMapOval::get_a(void) const { return a; }

inline int
GMapOval::get_b(void) const { return b; }

inline int
GMapOval::get_rmin(void) const { return rmin; }

inline int
GMapOval::get_rmax(void) const { return rmax; }

inline int
GMapOval::gma_get_xmin(void) const { return xmin; }

inline int
GMapOval::gma_get_ymin(void) const { return ymin; }

inline int
GMapOval::gma_get_xmax(void) const { return xmax; }

inline int
GMapOval::gma_get_ymax(void) const { return ymax; }

inline char const * const
GMapOval::get_shape_name(void) const { return OVAL_TAG; }

//@}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
