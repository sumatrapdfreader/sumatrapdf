# Our additions to mupdf

Files here are ours, not mupdf's: they are compiled into the `mupdf` project
(`mupdf_files()` in `premake5.files.lua`, the `mupdf` lib in
`cmd/deps-build-defs.ts` for ninja) but live outside the vendored `ext/mupdf`
tree so that they are not mistaken for upstream code and do not need an
`ext/patches/` entry. This directory is on the `mupdf` project's include path,
so the tools we patch inside `ext/mupdf` include them by bare name
(`#include "pkcs7-windows.h"`); our own code under `src/` uses
`#include "mupdf/pkcs7-windows.h"`.

- `mupdf_load_system_font.c` - loads installed fonts through the Windows font
  directory or fontconfig on Linux for mupdf's font fallback, plus the
  plain-malloc harfbuzz allocator wrappers
- `noto_sumatra.[ch]` — compiled instead of mupdf's `noto.c`: the built-in fonts
  are not linked into the binary but fetched by file name through
  `fz_set_builtin_font_loader()`. SumatraPDF tries the `fonts\` entries of
  `IDR_EMBEDDED_PAK` (staged by `cmd/pack-embedded-prebuild.cmd`; wasm and
  `SUMATRA_NO_EMBED_FONTS=1` leave them out), then a cache directory, then the
  URL in `fonts_map.c` (`bun cmd/upload-fonts.ts`)
- `pkcs7-windows.[ch]` — PDF signature verification and signing on the Win32
  CryptoAPI instead of OpenSSL (patch `0003` makes mupdf's `pdfsign` / `murun`
  call it)

- `pkcs7_mac.[ch]` — PDF signing with a .pfx through the macOS Security
  framework (ng only; no verification yet)

- `load-jxl.cpp` / `load-jxl.h` - JPEG XL metadata and pixel decoding through
  jxldec for MuPDF images and PDF `/JXLDecode` streams

_Changes_ to mupdf itself still go into `ext/mupdf` in place and get recorded as
a patch in `ext/patches/` (see its README). Put code here only when it is
entirely ours: a whole new file that mupdf's build does not know about.
