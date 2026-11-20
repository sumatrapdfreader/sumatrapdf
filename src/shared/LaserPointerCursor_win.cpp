/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "Settings.h"
#include "Theme.h"
#include "AppSettings.h"
#include "LaserPointerCursor.h"

#include "SumatraLog.h"

// The laser pointer: a cursor drawn at run time, the same in orig and ng.

static HCURSOR gCursorLaserPointer = nullptr;

static int gCursorLaserPointerSize = 0;

// The hotspot is the center of the dot, unlike an arrow's tip.
static HCURSOR CreateLaserPointerCursor(int size) {
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbmpColor = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbmpColor || !bits) {
        DeleteObject(hbmpColor);
        return nullptr;
    }

    LaserPointerDotPixels((u32*)bits, size);

    // 32bpp cursors carry their own alpha, but CreateIconIndirect still wants a
    // mask bitmap; an all-zero AND mask leaves the color bitmap in charge
    int maskBytesPerRow = ((size + 15) / 16) * 2;
    u8* maskBits = AllocArray<u8>(maskBytesPerRow * size);
    HBITMAP hbmpMask = CreateBitmap(size, size, 1, 1, maskBits);
    HCURSOR res = nullptr;
    if (hbmpMask) {
        ICONINFO ii{};
        ii.fIcon = FALSE;
        ii.xHotspot = (DWORD)(size / 2);
        ii.yHotspot = (DWORD)(size / 2);
        ii.hbmMask = hbmpMask;
        ii.hbmColor = hbmpColor;
        res = (HCURSOR)CreateIconIndirect(&ii);
        DeleteObject(hbmpMask);
    }
    DeleteObject(hbmpColor);
    free(maskBits);
    return res;
}

// the cursor is sized for the DPI of the window it's shown in, so it's
// re-created when the canvas moves to a monitor with a different scaling
HCURSOR GetLaserPointerCursor() {
    int size = DpiScale(kLaserPointerCursorSize);
    if (gCursorLaserPointer && (gCursorLaserPointerSize == size)) {
        return gCursorLaserPointer;
    }
    HCURSOR cur = CreateLaserPointerCursor(size);
    if (!cur) {
        // a cursor of the wrong size beats no cursor at all
        return gCursorLaserPointer;
    }
    if (gCursorLaserPointer) {
        DestroyCursor(gCursorLaserPointer);
    }
    gCursorLaserPointer = cur;
    gCursorLaserPointerSize = size;
    return cur;
}

void DeleteLaserPointerCursor() {
    if (gCursorLaserPointer) {
        DestroyCursor(gCursorLaserPointer);
        gCursorLaserPointer = nullptr;
        gCursorLaserPointerSize = 0;
    }
}
