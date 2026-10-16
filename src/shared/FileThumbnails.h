/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// the frame a thumbnail is made for, a sqrt(2) : 1 rectangle either way:
// ThumbnailsType legacy puts the top of the page in the landscape one, full
// the whole page in the portrait one
constexpr int kThumbnailLegacyDx = 212;
constexpr int kThumbnailLegacyDy = 150;
constexpr int kThumbnailFullDx = 150;
constexpr int kThumbnailFullDy = 212;

struct Pixmap;
struct FileState;
struct RenderedBitmap;
using OnBitmapRendered = Func1<RenderedBitmap*>;

bool ThumbnailsAreFullPage();
Size ThumbnailFrameSize();
Size ThumbnailDrawSize(Size thumb, Size frame);
bool ThumbnailSizeMatchesType(Size sz);
Size PngSizeFromData(Str data);
void OnThumbnailsTypeChanged();
void RegenerateMissingThumbnails();

Pixmap* LoadThumbnail(FileState* fs);
bool HasThumbnail(FileState* fs);
void SetThumbnail(FileState* fs, Pixmap* bmp);
void SaveThumbnail(FileState* fs);
void RemoveThumbnail(FileState* fs);

TempStr GetThumbnailCacheDirTemp();
TempStr GetThumbnailPathTemp(Str filePath);
void DeleteThumbnailForFile(Str path);
void EmptyThumbnailCacheDirectory();
void HomePageThumbnailChanged(FileState* fs);
void CreateChmThumbnail(Str path, const Size& size, const OnBitmapRendered* saveThumbnail);
