/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// a file's bytes as rows of offset, hex and text; reads only what is shown,
// so the file's size doesn't matter. Each app draws it (HexView.cpp)
struct HexView {
    FILE* f = nullptr;
    Str path;
    u64 size = 0;
    // offset of the first row shown; a multiple of perRow
    u64 top = 0;
    int rows = 40;
    int perRow = 16;
    u8* cache = nullptr;
    u64 cacheAt = 0;
    int cacheLen = 0;
    // a byte gone to with "go to offset"
    bool hasMark = false;
    u64 mark = 0;
    // fraction of a row left over from smooth (touchpad) wheel scrolling
    float wheelRem = 0;
};

constexpr int kHexMinPerRow = 8;
constexpr int kHexMaxPerRow = 256;
constexpr int kHexCacheBytes = 256 * 1024;

HexView* HexViewOpen(Str path);
void HexViewFree(HexView*);
int HexBytesAt(HexView*, u64 at, const u8** out, int want);
int HexBytesShown(HexView*, const u8** out);
void HexSetTop(HexView*, u64 top);
void HexScrollRows(HexView*, i64 rows);
void HexScrollPages(HexView*, int pages);
bool HexViewGoTo(HexView*, u64 offset);
u64 HexLastTop(HexView*);
u64 HexClampTop(u64 top, u64 size, int rows, int perRow);
int HexPerRowFor(float width, float charW, float gap, int offsetDigits);
int HexOffsetDigits(u64 size);
TempStr HexOffsetTemp(u64 offset, u64 size);
TempStr HexBytesTemp(const u8* bytes, int n, int perRow);
TempStr HexCharsTemp(const u8* bytes, int n);
TempStr HexColumnsHeaderTemp(int perRow);
TempStr HexPositionTemp(HexView*, int shown);
bool HexParseOffset(Str text, u64* out);
