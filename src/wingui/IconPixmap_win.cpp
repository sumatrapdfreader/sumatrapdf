/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/Win.h"

#include "wingui/IconPixmap.h"

struct IconPixmapCacheEntry {
    IconPixmapCacheEntry* next;
    int iconIdx;
    Pixmap* pixmap; // owned
};

// gIconCache is the head of a singly-linked list of entries; a handful of icons
// are ever cached, so a walk is as good as anything and new entries just go in
// at the front.
//
// The cache only ever holds icons of one image list: the app has a single
// toolbar image list, and it is destroyed and rebuilt whole on a theme or DPI
// change. Seeing a different HIMAGELIST (or icon size) therefore means the old
// pixmaps are stale, so the cache is dropped rather than grown.
static HIMAGELIST gIconCacheHiml = nullptr;
static Size gIconCacheIconSize;
static IconPixmapCacheEntry* gIconCache = nullptr;

void ClearIconPixmapCache() {
    IconPixmapCacheEntry* e = gIconCache;
    gIconCache = nullptr;
    while (e) {
        IconPixmapCacheEntry* next = e->next;
        FreePixmap(e->pixmap);
        delete e;
        e = next;
    }
    gIconCacheHiml = nullptr;
    gIconCacheIconSize = {};
}

// draw the icon into a DIB section, so the pixels are premultiplied BGRA that
// AlphaBlend can use directly. Going through an HICON (rather than
// ImageList_Draw) is what preserves the alpha channel
static Pixmap* RenderIconToPixmap(HIMAGELIST himl, int iconIdx, Size sz) {
    Pixmap* px = AllocPixmapDIB(sz.dx, sz.dy);
    if (!px) {
        return nullptr;
    }
    memset(px->data, 0, (size_t)px->stride * (size_t)sz.dy);
    px->premultiplied = true;
    HICON icon = ImageList_GetIcon(himl, iconIdx, ILD_TRANSPARENT);
    if (!icon) {
        return px;
    }
    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);
    ReleaseDC(nullptr, screenDC);
    if (memDC) {
        HGDIOBJ prev = SelectObject(memDC, px->hbmp);
        DrawIconEx(memDC, 0, 0, icon, sz.dx, sz.dy, 0, nullptr, DI_NORMAL);
        GdiFlush();
        if (prev) {
            SelectObject(memDC, prev);
        }
        DeleteDC(memDC);
    }
    DestroyIcon(icon);
    return px;
}

Pixmap* IconPixmapRender(HIMAGELIST himl, int iconIdx) {
    if (!himl) {
        return nullptr;
    }
    Size sz;
    ImageList_GetIconSize(himl, &sz.dx, &sz.dy);
    if (sz.IsEmpty()) {
        return nullptr;
    }
    return RenderIconToPixmap(himl, iconIdx, sz);
}

Pixmap* IconPixmapFromImageList(HIMAGELIST himl, int iconIdx) {
    if (!himl) {
        return nullptr;
    }
    Size sz2;
    ImageList_GetIconSize(himl, &sz2.dx, &sz2.dy);
    if (sz2.IsEmpty()) {
        return nullptr;
    }
    if (himl != gIconCacheHiml || sz2 != gIconCacheIconSize) {
        ClearIconPixmapCache();
        gIconCacheHiml = himl;
        gIconCacheIconSize = sz2;
    }
    for (IconPixmapCacheEntry* e = gIconCache; e; e = e->next) {
        if (e->iconIdx == iconIdx) {
            return e->pixmap;
        }
    }
    Pixmap* px = RenderIconToPixmap(himl, iconIdx, sz2);
    if (px) {
        auto* e = new IconPixmapCacheEntry{gIconCache, iconIdx, px};
        gIconCache = e;
    }
    return px;
}
