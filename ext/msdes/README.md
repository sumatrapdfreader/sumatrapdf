# msdes

D3DES (V5.09) by Richard Outerbridge: a portable, public-domain implementation
of the Data Encryption Standard. Vendored from calibre
(`src/calibre/utils/msdes/{des.c,d3des.h,spr.h}`), which uses it to decrypt
Microsoft Reader (.lit) files; the code itself is unmodified public-domain
d3des, not GPL.

Used by `src/LitDoc.cpp` to unseal the DRM1 ("sealed") encryption present in
every .lit file: `deskey(key, DE1)` + `des(in, out)` per 8-byte block (ECB).

Do not clang-format; keep edits minimal.
