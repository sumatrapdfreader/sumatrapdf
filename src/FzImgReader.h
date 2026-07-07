/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// TODO: not a great place for this
constexpr size_t kFzStoreUnlimited = 0;
constexpr size_t kFzStoreDefault = 256 << 20;
struct fz_context;
fz_context* fz_new_context_windows(size_t maxStore = kFzStoreUnlimited);
void fz_drop_context_windows(fz_context* ctx);

struct Pixmap;
struct RenderedBitmap;
Pixmap* PixmapFromDataFz(Str);
// single (first-frame) Pixmap; caller owns it (FreePixmap)
Pixmap* PixmapFromData(Str);
// one Pixmap per frame (multi-page TIFF / animated GIF yield >1); caller owns each
Vec<Pixmap*> PixmapsFromData(Str);
RenderedBitmap* LoadRenderedBitmap(Str path);
