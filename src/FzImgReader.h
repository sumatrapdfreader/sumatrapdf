/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// TODO: not a great place for this
constexpr size_t kFzStoreUnlimited = 0;
constexpr size_t kFzStoreDefault = 256 << 20;
struct fz_context;
fz_context* fz_new_context_windows(size_t maxStore = kFzStoreUnlimited);
void fz_drop_context_windows(fz_context* ctx);

Gdiplus::Bitmap* FzImageFromData(const ByteSlice&);

Gdiplus::Bitmap* BitmapFromData(const ByteSlice&);
RenderedBitmap* LoadRenderedBitmap(const char* path);
