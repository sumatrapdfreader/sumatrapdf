/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by AnnotPlacementCursors_win.cpp and orig's AnnotPlacement.cpp ---

extern HCURSOR gCursorTextAnnotationPlacement;
extern HCURSOR gCursorInkAnnotationPlacement;
HCURSOR GetTextAnnotationPlacementCursor();
HCURSOR GetInkAnnotationPlacementCursor();
void DeleteAnnotationPlacementCursors();

// implemented by each app
HCURSOR CreateSvgPlacementCursor(const char* icon, int dx, int dy, Color color, DWORD hotspotX, DWORD hotspotY);
