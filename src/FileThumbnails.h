/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// thumbnails are 150px high and have a ratio of sqrt(2) : 1
#define THUMBNAIL_DX 212
#define THUMBNAIL_DY 150

void CleanUpThumbnailCache(const FileHistory& fileHistory);

bool LoadThumbnail(FileState& ds);
bool HasThumbnail(FileState& ds);
// takes ownership of bmp
void SetThumbnail(FileState* ds, RenderedBitmap* bmp);
void SaveThumbnail(FileState& ds);
void RemoveThumbnail(FileState& ds);
