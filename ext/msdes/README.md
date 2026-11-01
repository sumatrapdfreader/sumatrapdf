# msdes

D3DES (V5.09) by Richard Outerbridge: a portable, public-domain implementation
of the Data Encryption Standard. Vendored from calibre
(`src/calibre/utils/msdes/{des.c,d3des.h,spr.h}`), which uses it to decrypt
Microsoft Reader (.lit) files; the code itself is public-domain d3des, not GPL.

Our change: the key schedule is a caller-owned `DesKey` instead of a global, so
books loading at the same time don't overwrite each other's key.

Used by `src/shared/LitDoc.cpp` to unseal the DRM1 ("sealed") encryption present
in every .lit file: `deskey(&dk, key, DE1)` + `des(&dk, in, out)` per 8-byte
block (ECB).

Do not clang-format; keep edits minimal.
