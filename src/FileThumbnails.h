/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// thumbnails are 150px high and have a ratio of sqrt(2) : 1
constexpr int kThumbnailDx = 212;
constexpr int kThumbnailDy = 150;

RenderedBitmap* LoadThumbnail(FileState* fs);
bool HasThumbnail(FileState* fs);
void SetThumbnail(FileState* fs, RenderedBitmap* bmp);
void SaveThumbnail(FileState* fs);
void RemoveThumbnail(FileState* fs);

TempStr GetThumbnailCacheDirTemp();
char* GetThumbnailPathTemp(const char* filePath);
void DeleteThumbnailForFile(const char* path);
void DeleteThumbnailCacheDirectory();
