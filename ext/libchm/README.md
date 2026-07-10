# libchm

Vendored CHM / ITSS archive reader from https://github.com/kjk/libchm
(the `dist/` amalgamation: a single `chm.c` with the LZX decompressor inlined,
plus `chm.h`).

Read-only, in-memory only, plain C with no dependencies. Replaces the older
LGPL CHMLib fork; MIT licensed (see `LICENSE.md`).

To update: copy `dist/chm.c` and `dist/chm.h` from the upstream repo.
