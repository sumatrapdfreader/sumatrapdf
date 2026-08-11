/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// The VirtWnd controls take Pixmaps so they don't depend on a Windows imaging
// API, but our icons live in HIMAGELISTs. Converting one on every paint would
// be wasteful, so an icon is rendered into a DIB-backed Pixmap once and kept
// until the image list it came from is replaced (theme or DPI change).

struct Pixmap;

Pixmap* IconPixmapFromImageList(HIMAGELIST, int iconIdx);
// same, but the caller owns the result (FreePixmap()) and the shared cache is
// left alone. For icons that come from an image list of your own, which the
// cache (one image list at a time) would otherwise thrash
Pixmap* IconPixmapRender(HIMAGELIST, int iconIdx);
void ClearIconPixmapCache();
