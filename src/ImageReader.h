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

// Decode via MuPDF (JPEG / JPEG2000 currently).
Pixmap* PixmapFromDataFz(Str);

// Decode image bytes to a single (first-frame) Pixmap. Caller owns it (FreePixmap).
// Windows: JPEG→turbo, WebP→libwebp, JXL→libjxl; HEIC/AVIF→heicdec then WIC in
// Debug, WIC then heicdec in Release; else TGA/GDI+/WIC. POSIX: MuPDF for now.
Pixmap* PixmapFromData(Str);

// One Pixmap per frame (multi-page TIFF / animated GIF yield >1); caller owns each.
Vec<Pixmap*> PixmapsFromData(Str);

// Cheap size probe (header parse, else full decode). Returns empty Size on failure.
Size ImageSizeFromData(Str);

// Load path into a RenderedBitmap (Windows); nullptr on POSIX for now.
RenderedBitmap* LoadRenderedBitmap(Str path);
