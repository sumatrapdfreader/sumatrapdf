/** \file lvbmpbuf.h
    \brief Gray bitmap buffer

    CoolReader Engine C-compatible API

    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

#ifndef __LVBMPBUF_H_INCLUDED__
#define __LVBMPBUF_H_INCLUDED__

#include "lvfnt.h"
//#include "lvtextfm.h"

/** \brief Bitmap buffer to draw in (C API) */
typedef struct
tag_draw_buf 
{
    int height;
    int bitsPerPixel;
    int bytesPerRow;
    unsigned char * data;
} draw_buf_t;


/** \brief Init drawing structure using existing buffer (C API)*/
void lvdrawbufInit( draw_buf_t * buf, int bitsPerPixel, int width, int height, unsigned char * data );

/** \brief Init drawing structure and allocate new buffer (C API)*/
void lvdrawbufAlloc( draw_buf_t * buf, int bitsPerPixel, int width, int height );

/** \brief Free buffer allocated by lvdrawbufAlloc (C API) */
void lvdrawbufFree( draw_buf_t * buf );

/** \brief Fill the whole buffer with specified data (C API) */
void lvdrawbufFill( draw_buf_t * buf, unsigned char pixel );

/** \brief Fill rectangle with specified data (C API) */
void lvdrawbufFillRect( draw_buf_t * buf, int x0, int y0, int x1, int y1, unsigned char pixel );

/** \brief Draw bitmap into buffer of the same bit depth (logical OR) (C API)
   
   \param x, y are coordinates where to draw
   \param bitmap is src bitmap data
   \[param numRows is number of rows of source bitmap
   \param bytesPerRow is number of bytes per row of source bitmap
*/
void lvdrawbufDraw( draw_buf_t * buf, int x, int y, const unsigned char * bitmap, int numRows, int bytesPerRow );

/** \brief Draw text string into buffer (logical OR)
   \param x, y are coordinates where to draw
   \param text is string to draw
   \param len is number of chars from text to draw
*/
void lvdrawbufDrawText( draw_buf_t * buf, int x, int y, const lvfont_handle pfont, const lChar16 * text, int len, lChar16 def_char );


#endif
