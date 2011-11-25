/** \file lvrend.h
    \brief DOM document rendering (formatting) functions

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

#ifndef __LV_REND_H_INCLUDED__
#define __LV_REND_H_INCLUDED__

#include "lvtinydom.h"

/// returns true if styles are identical
bool isSameFontStyle( css_style_rec_t * style1, css_style_rec_t * style2 );
/// removes format data from node
void freeFormatData( ldomNode * node );
/// returns best suitable font for style
LVFontRef getFont( css_style_rec_t * style );
/// initializes format data for node
void initFormatData( ldomNode * node );
/// initializes rendering method for node
int initRendMethod( ldomNode * node, bool recurseChildren, bool allowAutoboxing );
/// converts style to text formatting API flags
int styleToTextFmtFlags( const css_style_ref_t & style, int oldflags );
/// renders block as single text formatter object
void renderFinalBlock( ldomNode * node, LFormattedText * txform, RenderRectAccessor * fmt, int & flags, int ident, int line_h );
/// renders block which contains subblocks
int renderBlockElement( LVRendPageContext & context, ldomNode * node, int x, int y, int width );
/// renders table element
int renderTable( LVRendPageContext & context, ldomNode * element, int x, int y, int width );
/// sets node style
void setNodeStyle( ldomNode * node, css_style_ref_t parent_style, LVFontRef parent_font );

/// draws formatted document to drawing buffer
void DrawDocument( LVDrawBuf & drawbuf, ldomNode * node, int x0, int y0, int dx, int dy, int doc_x, int doc_y, int page_height, ldomMarkedRangeList * marks );

#define STYLE_FONT_EMBOLD_MODE_NORMAL 0
#define STYLE_FONT_EMBOLD_MODE_EMBOLD 300

/// set global document font style embolden mode (0=off, 300=on)
void LVRendSetFontEmbolden( int addWidth=STYLE_FONT_EMBOLD_MODE_EMBOLD );
/// get global document font style embolden mode
int LVRendGetFontEmbolden();

#endif
