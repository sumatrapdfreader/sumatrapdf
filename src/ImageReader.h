/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// MuPDF context with our lock/log plumbing (used on all platforms; name is historical).
constexpr size_t kFzStoreUnlimited = 0;
constexpr size_t kFzStoreDefault = 256 << 20;
// Don't fully decode images whose uncompressed BGRA would exceed this. A
// 39137x22279 JPEG is ~3.5GB; 3.5.2 opened those via GDI+ without a full-res
// pixmap. We keep the encoded bytes and decode at display scale instead.
constexpr i64 kMaxDecodedPixmapBytes = 512LL * 1024 * 1024;
struct fz_context;
fz_context* fz_new_context_windows(size_t maxStore = kFzStoreUnlimited);
void fz_drop_context_windows(fz_context* ctx);

struct Pixmap;
struct RenderedBitmap;

bool ImageDecodedPixmapWouldBeHuge(Str);

Pixmap* PixmapFromDataFz(Str);

Pixmap* PixmapFromData(Str);

bool DecodeJpegToCmyk(Str jpeg, int& w, int& h, int& stride, Vec<u8>& samples);

Vec<Pixmap*> PixmapsFromData(Str);

Size ImageSizeFromData(Str);

RenderedBitmap* LoadRenderedBitmap(Str path);
