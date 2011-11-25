/** \file s32utils.h
    \brief misc symbian utility functions

    CoolReader Engine


    (c) torque, 2006
    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

#ifndef __S32_UTILS_H_INCLUDED__
#define __S32_UTILS_H_INCLUDED__

#ifdef __SYMBIAN32__

#include "lvfnt.h"
#include "lvdrawbuf.h"

#include <e32base.h>
#include <w32std.h>

/// draw gray bitmap buffer to Windows device context
void DrawBuf2DC(CWindowGc &dc, int x, int y, LVDrawBuf * buf, unsigned long * palette, int scale=1 );


#endif

#endif

