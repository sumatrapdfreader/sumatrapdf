# Our additions to mupdf

Files here are ours, not mupdf's: they are compiled into the `mupdf` project
(`mupdf_files()` in `premake5.files.lua`, the `mupdf` lib in
`cmd/deps-build-defs.ts` for ninja) but live outside the vendored `ext/mupdf`
tree so that they are not mistaken for upstream code and do not need an
`ext/patches/` entry. This directory is on the `mupdf` project's include path,
so the tools we patch inside `ext/mupdf` include them by bare name
(`#include "pkcs7-windows.h"`); our own code under `src/` uses
`#include "mupdf/pkcs7-windows.h"`.

- `mupdf_load_system_font.c` — loads the fonts installed in Windows for mupdf's
  font fallback, plus the plain-malloc harfbuzz allocator wrappers
- `pkcs7-windows.[ch]` — PDF signature verification and signing on the Win32
  CryptoAPI instead of OpenSSL (patch `0003` makes mupdf's `pdfsign` / `murun`
  call it)

_Changes_ to mupdf itself still go into `ext/mupdf` in place and get recorded as
a patch in `ext/patches/` (see its README). Put code here only when it is
entirely ours: a whole new file that mupdf's build does not know about.
