/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

void CalcMD5Digest(Str data, u8 digest[16]);
void CalcSHA1Digest(Str data, u8 digest[20]);
void CalcSHA2Digest(Str data, u8 digest[32]);

// SHA-1 of data given in pieces: Sha1Update() any number of times, then
// Sha1Finish(), which frees the hasher
struct Sha1Hasher;
Sha1Hasher* NewSha1Hasher();
void Sha1Update(Sha1Hasher*, Str data);
void Sha1Finish(Sha1Hasher*, u8 digest[20]);

Str ExtractP7m(Str d);

bool IsPEFileSigned(Str filePath);
TempStr GetExecutableSignerTemp(Str exePath);
