// lvfonttest.h

#ifndef __LVFONTTEST_H_INCLUDED__
#define __LVFONTTEST_H_INCLUDED__

#include "../../include/lvfnt.h"
#include "../../include/lvfntman.h"
#include "../../include/lvbmpbuf.h"

void DrawBuf2DC(CDC & dc, int x, int y, draw_buf_t * buf, COLORREF * palette, int scale=1 );


#endif
