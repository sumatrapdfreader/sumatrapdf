/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// thumbnails are 150px high and have a ratio of sqrt(2) : 1
constexpr int kThumbnailDx = 212;
constexpr int kThumbnailDy = 150;

struct Pixmap;
struct FileState;

Pixmap* LoadThumbnail(FileState* fs);
bool HasThumbnail(FileState* fs);
void SetThumbnail(FileState* fs, Pixmap* bmp);
void SaveThumbnail(FileState* fs);
void RemoveThumbnail(FileState* fs);

TempStr GetThumbnailCacheDirTemp();
TempStr GetThumbnailPathTemp(Str filePath);
void DeleteThumbnailForFile(Str path);
void EmptyThumbnailCacheDirectory();
