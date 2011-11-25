/** \file xutils.h
    \brief misc X Window System utility functions

    CoolReader Engine


    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

#ifndef __X_UTILS_H_INCLUDED__
#define __X_UTILS_H_INCLUDED__

#ifdef LINUX

#include "lvfnt.h"
#include "lvdrawbuf.h"

/**
    \brief RGB offscreen image for X Window System
*/
class MyXImage
{
private:
    XImage * _img;
public:
    /// creates image buffer of specified size
    MyXImage( int dx, int dy );
    ~MyXImage();
    /// returns scanline pointer (pixel is 32bit unsigned)
    unsigned * getScanLine( int y );
    /// fills buffer with specified color
    void fill( unsigned pixel );
    /// returns XImage object
    XImage * getXImage()
    {
        return _img;
    }
};



/// draw gray bitmap buffer to X drawable
void DrawBuf2Drawable(Display *display, Drawable d, GC gc, int x, int y, LVDrawBuf * buf, unsigned * palette, int scale=1 );

#endif

#endif
