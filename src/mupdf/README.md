# Our additions to mupdf

Files here and in `src/shared/mupdf` are ours, not mupdf's: they are compiled into the `mupdf` project
(`mupdf_files()` in `premake5.files.lua`, the `mupdf` lib in
`cmd/deps-build-defs.ts` for ninja) but live outside the vendored `ext/mupdf`
tree so that they are not mistaken for upstream code and do not need an
`ext/patches/` entry. Both directories are on the `mupdf` project's include path,
so the tools we patch inside `ext/mupdf` include them by bare name
(`#include "pkcs7-windows.h"`); our own code under `src/` uses
`#include "mupdf/pkcs7-windows.h"`.

- `mupdf_load_system_font.c` — loads the fonts installed in Windows for mupdf's
  font fallback, plus the plain-malloc harfbuzz allocator wrappers
- `noto_sumatra.[ch]` — compiled instead of mupdf's `noto.c`: the built-in fonts
  are not linked into the binary but fetched by file name through
  `fz_set_builtin_font_loader()`. SumatraPDF tries the `fonts\` entries of
  `IDR_EMBEDDED_PAK` (staged by `cmd/pack-embedded-prebuild.cmd`; wasm and
  `SUMATRA_NO_EMBED_FONTS=1` leave them out), then a cache directory, then the
  URL in `fonts_map.c` (`bun cmd/upload-fonts.ts`)
- `pkcs7-windows.[ch]` — PDF signature verification and signing on the Win32
  CryptoAPI instead of OpenSSL (patch `0003` makes mupdf's `pdfsign` / `murun`
  call it)

- `load-jxl.cpp` / `load-jxl.h` - JPEG XL metadata and pixel decoding through
  jxldec for MuPDF images and PDF `/JXLDecode` streams
- `zxing-eci.cpp` (in `src/shared/mupdf`) - builds zxing-cpp's `ECI.cpp` under a
  name that does not collide with zint's `eci.c` in the `zxing` lib (mupdf's
  `eci_dedup.cpp`)

_Changes_ to mupdf itself still go into `ext/mupdf` in place and get recorded as
a patch in `ext/patches/` (see its README). Put code here only when it is
entirely ours: a whole new file that mupdf's build does not know about. Shared
additions live in `src/shared/mupdf`.

## OFD prototype

`src/shared/mupdf/ofd.c` registers an OFD document handler with each app's
MuPDF context. It reads ZIP/XML and builds native display lists, without
Java, PDFium, or PDF conversion. It supports text, basic paths, images,
background/foreground templates, inherited draw parameters, and annotation
appearances. Text extraction uses the same display list as rendering.
Embedded glyph mappings retain the logical Unicode text. Text and path fills
and strokes support tiling patterns and axial/radial gradients. Clip areas, composite units,
blend modes, and line styles are rendered through MuPDF. Missing fonts use
substitutes; a broken image/font directory can be recovered when the
document contains exactly one matching member.
SES signature appearances can contain raster images or an embedded OFD;
both are fitted to the stamp's page and boundary, including clipped strips
for seals spanning pages. Signature validity is not checked.
Seal artwork uses Multiply blending so opaque white backgrounds do not
cover the underlying page.

This is an initial implementation. Mesh gradients, indexed colors, arc paths,
page-local resources, and nested templates remain unsupported. Only the
first document in a package is opened. Signatures are not verified; visible
stamp artwork does not establish signature validity. Outlines, attachment
navigation, and OFD editing are not implemented.

Run `bun tests/ofd.ts`, `bun tests/ofd-features.ts`, `bun tests/ofd-seals.ts`,
and `bun tests/ofd-path-fill.ts` for generated fixtures. With private samples
in ignored `demo/`, run `bun tests/ad-hoc-ofd-corpus.ts --no-build` to check
first-page rendering and export two previews. Sample contents are not
included in the repository. Windows debug is the initial validation target;
ng's other platforms remain unverified.
