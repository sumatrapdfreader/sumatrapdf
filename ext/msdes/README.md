# msdes

D3DES (V5.09) by Richard Outerbridge: a portable implementation of the Data
Encryption Standard. Vendored from calibre
(`src/calibre/utils/msdes/{des.c,d3des.h,spr.h}`), which uses it to decrypt
Microsoft Reader (.lit) files.

License: the code in `des.c` and `d3des.h` is public domain. The `SP1`-`SP8`
tables in `des.c` are from openclit's `spr.h` and are GPLv2 or later, so msdes as
a whole is GPL.

Our changes: the key schedule is a caller-owned `DesKey` instead of a global, so
books loading at the same time don't overwrite each other's key. Removed the
unused double/triple DES code and the unused standard tables. Merged `spr.h` into
`des.c`.

Used by `src/shared/LitDoc.cpp` to unseal the DRM1 ("sealed") encryption present
in every .lit file: `deskey(&dk, key, DE1)` + `des(&dk, in, out)` per 8-byte
block (ECB).

Do not clang-format; keep edits minimal.
