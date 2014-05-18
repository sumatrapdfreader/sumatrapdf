/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Thumbnails_h
#define Thumbnails_h

#include "DisplayState.h"
#include "FileHistory.h"

// thumbnails are 150px high and have a ratio of sqrt(2) : 1
#define THUMBNAIL_DX        212
#define THUMBNAIL_DY        150

void    CleanUpThumbnailCache(FileHistory& fileHistory);

bool    LoadThumbnail(DisplayState& ds);
bool    HasThumbnail(DisplayState& ds);
void    SaveThumbnail(DisplayState& ds);
void    RemoveThumbnail(DisplayState& ds);

#endif
