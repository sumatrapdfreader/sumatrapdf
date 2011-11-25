/** \file w32utils.h
    \brief misc windows utility functions

    CoolReader Engine


    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

#ifndef __W32_UTILS_H_INCLUDED__
#define __W32_UTILS_H_INCLUDED__

#if !defined(__SYMBIAN32__) && defined(_WIN32)

#include "lvfnt.h"
#include "lvdrawbuf.h"

extern "C" {
#include <windows.h>
}

/// draw gray bitmap buffer to Windows device context
void DrawBuf2DC(HDC dc, int x, int y, LVDrawBuf * buf, COLORREF * palette, int scale=1 );
/// save gray bitmap to .BMP file
void SaveBitmapToFile( const char * fname, LVGrayDrawBuf * bmp );


#endif

#endif
