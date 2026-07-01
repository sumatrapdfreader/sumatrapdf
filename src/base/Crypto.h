/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

void CalcMD5Digest(Str data, u8 digest[16]);
void CalcSHA1Digest(Str data, u8 digest[20]);
void CalcSHA2Digest(Str data, u8 digest[32]);

bool VerifySHA1Signature(Str data, Str hexSignature, Str pubkey);

// extracts the content (e.g. PDF) from a PKCS#7 / .p7m wrapper using Win32 crypto APIs
Str ExtractP7m(Str d);
