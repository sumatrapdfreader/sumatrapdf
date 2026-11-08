/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"

#include "HexViewCommon.h"

static constexpr const char* kHexDigits = "0123456789ABCDEF";

static bool HexSeek(FILE* f, u64 at) {
#if OS_WIN
    return _fseeki64(f, (i64)at, SEEK_SET) == 0;
#else
    return fseeko(f, (off_t)at, SEEK_SET) == 0;
#endif
}

static u64 HexFileSize(FILE* f) {
#if OS_WIN
    if (_fseeki64(f, 0, SEEK_END) != 0) {
        return 0;
    }
    i64 size = _ftelli64(f);
#else
    if (fseeko(f, 0, SEEK_END) != 0) {
        return 0;
    }
    i64 size = (i64)ftello(f);
#endif
    return size > 0 ? (u64)size : 0;
}

static int HexFileRead(FILE* f, u64 at, u8* buf, int cap) {
    if (!HexSeek(f, at)) {
        return 0;
    }
    return (int)fread(buf, 1, (size_t)cap, f);
}

HexView* HexViewOpen(Str path) {
    FILE* f = file::OpenFILE(path);
    if (!f) {
        return nullptr;
    }
    auto hex = new HexView();
    hex->f = f;
    hex->path = str::Dup(path);
    hex->size = HexFileSize(f);
    hex->cache = AllocArray<u8>(kHexCacheBytes);
    return hex;
}

void HexViewFree(HexView* hex) {
    if (!hex) {
        return;
    }
    fclose(hex->f);
    str::Free(hex->path);
    free(hex->cache);
    delete hex;
}

// bytes at `at`, from the piece of the file around it that is kept in memory
int HexBytesAt(HexView* hex, u64 at, const u8** out, int want) {
    bool inCache = at >= hex->cacheAt && at + (u64)want <= hex->cacheAt + (u64)hex->cacheLen;
    if (!inCache) {
        // the file can change under us: re-read its size when re-filling
        hex->size = HexFileSize(hex->f);
        u64 start = at > kHexCacheBytes / 4 ? at - kHexCacheBytes / 4 : 0;
        hex->cacheAt = start;
        hex->cacheLen = HexFileRead(hex->f, start, hex->cache, kHexCacheBytes);
    }
    if (at < hex->cacheAt || at >= hex->cacheAt + (u64)hex->cacheLen) {
        return 0;
    }
    int have = (int)(hex->cacheAt + (u64)hex->cacheLen - at);
    *out = hex->cache + (at - hex->cacheAt);
    return std::min(have, want);
}

// the bytes of the rows on screen
int HexBytesShown(HexView* hex, const u8** out) {
    hex->top = HexClampTop(hex->top, hex->size, hex->rows, hex->perRow);
    u64 want = (u64)hex->rows * (u64)hex->perRow;
    int n = HexBytesAt(hex, hex->top, out, (int)std::min(want, (u64)kHexCacheBytes / 2));
    // reading may have found a different size
    hex->top = HexClampTop(hex->top, hex->size, hex->rows, hex->perRow);
    return n;
}

// the top a file of size bytes can start at, so that the window's rows are full when they can be
u64 HexClampTop(u64 top, u64 size, int rows, int perRow) {
    u64 total = (size + (u64)perRow - 1) / (u64)perRow;
    u64 lastTopRow = total > (u64)rows ? total - (u64)rows : 0;
    u64 row = top / (u64)perRow;
    return std::min(row, lastTopRow) * (u64)perRow;
}

u64 HexLastTop(HexView* hex) {
    return HexClampTop(hex->size, hex->size, hex->rows, hex->perRow);
}

void HexSetTop(HexView* hex, u64 top) {
    hex->top = HexClampTop(top, hex->size, hex->rows, hex->perRow);
}

void HexScrollRows(HexView* hex, i64 rows) {
    u64 by = (u64)(rows < 0 ? -rows : rows) * (u64)hex->perRow;
    if (rows < 0) {
        HexSetTop(hex, hex->top > by ? hex->top - by : 0);
        return;
    }
    u64 next = hex->top + by;
    HexSetTop(hex, next < hex->top ? hex->size : next);
}

void HexScrollPages(HexView* hex, int pages) {
    int page = std::max(hex->rows - 1, 1);
    HexScrollRows(hex, (i64)pages * page);
}

bool HexViewGoTo(HexView* hex, u64 offset) {
    if (offset >= hex->size && !(offset == 0 && hex->size == 0)) {
        return false;
    }
    hex->hasMark = true;
    hex->mark = offset;
    HexSetTop(hex, offset);
    return true;
}

// as many bytes as fit in width, in multiples of 4: offset, gap, "XX " per byte, gap, one char per byte
int HexPerRowFor(float width, float charW, float gap, int offsetDigits) {
    if (charW <= 0) {
        return 16;
    }
    float room = width - 2 * gap - (float)(offsetDigits - 1) * charW;
    int n = (int)(room / (4 * charW));
    n = n / 4 * 4;
    return std::clamp(n, kHexMinPerRow, kHexMaxPerRow);
}

static void AppendHexDigits(str::Builder& out, u64 v, int digits) {
    for (int i = digits - 1; i >= 0; i--) {
        out.AppendChar(kHexDigits[(v >> (i * 4)) & 15]);
    }
}

static int HexDigitsOf(u64 v) {
    int digits = 1;
    while (digits < 16 && (v >> (digits * 4)) != 0) {
        digits++;
    }
    return digits;
}

// the digits of the largest offset in a file of size bytes
int HexOffsetDigits(u64 size) {
    return HexDigitsOf(size > 0 ? size - 1 : 0);
}

TempStr HexOffsetTemp(u64 offset, u64 size) {
    str::Builder out(GetTempArena());
    AppendHexDigits(out, offset, std::max(HexDigitsOf(offset), HexOffsetDigits(size)));
    return out.TakeStr();
}

// "41 42 00" for n of perRow bytes, padded with spaces to a full row
TempStr HexBytesTemp(const u8* bytes, int n, int perRow) {
    str::Builder out(GetTempArena());
    for (int i = 0; i < perRow; i++) {
        if (i > 0) {
            out.AppendChar(' ');
        }
        if (i < n) {
            AppendHexDigits(out, bytes[i], 2);
        } else {
            out.Append(StrL("  "));
        }
    }
    return out.TakeStr();
}

// printable ASCII as is, everything else as '.'
TempStr HexCharsTemp(const u8* bytes, int n) {
    str::Builder out(GetTempArena());
    for (int i = 0; i < n; i++) {
        out.AppendChar(bytes[i] >= 32 && bytes[i] < 127 ? (char)bytes[i] : '.');
    }
    return out.TakeStr();
}

// "00 01 02 ..": the column of each byte in a row
TempStr HexColumnsHeaderTemp(int perRow) {
    str::Builder out(GetTempArena());
    for (int i = 0; i < perRow; i++) {
        if (i > 0) {
            out.AppendChar(' ');
        }
        AppendHexDigits(out, (u64)i, 2);
    }
    return out.TakeStr();
}

// "100 - 1FF of 4.5 MB": what's on screen, for the status line
TempStr HexPositionTemp(HexView* hex, int shown) {
    u64 last = hex->top + (u64)shown;
    TempStr from = HexOffsetTemp(hex->top, hex->size);
    TempStr to = HexOffsetTemp(last, hex->size);
    return fmt("%s - %s of %s", from, to, FormatFileSizeTemp(hex->size));
}

static char HexFoldAscii(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
}

// 0x1F, 1Fh and 1f are hex, 31 is decimal
bool HexParseOffset(Str text, u64* out) {
    int start = 0;
    int end = len(text);
    while (start < end && text.s[start] == ' ') {
        start++;
    }
    while (end > start && text.s[end - 1] == ' ') {
        end--;
    }
    bool hex = false;
    if (end - start > 2 && text.s[start] == '0' && (text.s[start + 1] == 'x' || text.s[start + 1] == 'X')) {
        start += 2;
        hex = true;
    } else if (end - start > 1 && (text.s[end - 1] == 'h' || text.s[end - 1] == 'H')) {
        end--;
        hex = true;
    }
    if (start >= end) {
        return false;
    }
    for (int i = start; i < end && !hex; i++) {
        char c = HexFoldAscii(text.s[i]);
        hex = c >= 'a' && c <= 'f';
    }
    u64 v = 0;
    u64 base = hex ? 16 : 10;
    for (int i = start; i < end; i++) {
        char c = HexFoldAscii(text.s[i]);
        u64 d = 99;
        if (c >= '0' && c <= '9') {
            d = (u64)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = (u64)(c - 'a' + 10);
        }
        if (d >= base || v > (~(u64)0 - d) / base) {
            return false;
        }
        v = v * base + d;
    }
    *out = v;
    return true;
}
