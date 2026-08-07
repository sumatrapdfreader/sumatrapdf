/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// MuPDF context with our lock/log plumbing (used on all platforms; name is historical).
constexpr size_t kFzStoreUnlimited = 0;
constexpr size_t kFzStoreDefault = 256 << 20;
struct fz_context;
fz_context* fz_new_context_windows(size_t maxStore = kFzStoreUnlimited);
void fz_drop_context_windows(fz_context* ctx);

struct Pixmap;
struct RenderedBitmap;

Pixmap* PixmapFromDataFz(Str);

Pixmap* PixmapFromData(Str);

Vec<Pixmap*> PixmapsFromData(Str);

Size ImageSizeFromData(Str);

RenderedBitmap* LoadRenderedBitmap(Str path);
