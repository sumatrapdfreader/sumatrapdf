/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

bool IsSingle(DisplayMode mode);
bool IsContinuous(DisplayMode mode);
bool IsFacing(DisplayMode mode);
bool IsBookView(DisplayMode mode);
bool IsValidZoom(float zoomLevel);

Str DisplayModeToString(DisplayMode mode);
DisplayMode DisplayModeFromString(Str s, DisplayMode defVal);
float ZoomFromString(Str s, float defVal);
void ZoomToString(Str* dst, float zoom, FileState* fileState);
bool MaybeGetNextZoomByIncrement(float* currZoomInOut, float towardsLevel);
float* GetDefaultZoomLevels(int* nZoomLevelsOut);
