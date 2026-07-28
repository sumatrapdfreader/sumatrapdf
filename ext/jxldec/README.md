# jxldec

Vendored JPEG XL decoder from https://github.com/kjk/jxldec
(`dist/` amalgamation: `jxl.c` + `jxl.h`).

Read-only, in-memory, plain C. Replaces the older libjxl + highway + skcms stack
for decode.

To update: copy `dist/jxl.c` and `dist/jxl.h` from the upstream repo and bump
`ext/versions.txt`.
