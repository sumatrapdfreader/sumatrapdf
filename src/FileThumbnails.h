/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// thumbnails are 150px high and have a ratio of sqrt(2) : 1
constexpr int kThumbnailDx = 212;
constexpr int kThumbnailDy = 150;

bool LoadThumbnail(FileState* ds);
bool HasThumbnail(FileState* ds);
void SetThumbnail(FileState* ds, RenderedBitmap* bmp);
void SaveThumbnail(FileState* ds);
void RemoveThumbnail(FileState* ds);

TempStr GetThumbnailCacheDirTemp();
char* GetThumbnailPathTemp(const char* filePath);
void DeleteThumbnailForFile(const char* path);
void DeleteThumbnailCacheDirectory();
