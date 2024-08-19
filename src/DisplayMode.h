/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

bool IsSingle(DisplayMode mode);
bool IsContinuous(DisplayMode mode);
bool IsFacing(DisplayMode mode);
bool IsBookView(DisplayMode mode);
bool IsValidZoom(float zoomLevel);

const char* DisplayModeToString(DisplayMode mode);
DisplayMode DisplayModeFromString(const char* s, DisplayMode defVal);
float ZoomFromString(const char* s, float defVal);
void ZoomToString(char** dst, float zoom, FileState* stateForIssue2140);
bool MaybeGetNextZoomByIncrement(float* currZoomInOut, float towardsLevel);
float* GetDefaultZoomLevels(int* nZoomLevelsOut);
