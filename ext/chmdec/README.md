# chmdec

Vendored CHM / ITSS archive reader from https://github.com/kjk/chmdec
(the `dist/` amalgamation: a single `chm.c` with the LZX decompressor inlined,
plus `chm.h`).

Read-only, in-memory only, plain C with no dependencies. Replaces the older
LGPL CHMLib fork and the previous `ext/libchm` vendor; MIT licensed (see
`LICENSE.md`).

To update: see `docs/upgrade-chmdec.md` (copy `dist/chm.c` and `dist/chm.h`
from the upstream repo).
