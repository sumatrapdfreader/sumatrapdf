/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// logical size of the laser pointer cursor. The dot itself is a small part of
// it, the rest is the glow fading out to fully transparent
constexpr int kLaserPointerCursorSize = 32;

void LaserPointerDotPixels(u32* pixels, int size);

#if OS_WIN
HCURSOR GetLaserPointerCursor();
void DeleteLaserPointerCursor();
#endif
