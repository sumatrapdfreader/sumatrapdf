/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

void CalcMD5Digest(const void* data, size_t dataSize, u8 digest[16]);
void CalcSHA1Digest(const void* data, size_t dataSize, u8 digest[20]);
void CalcSHA2Digest(const void* data, size_t dataSize, u8 digest[32]);

bool VerifySHA1Signature(const void* data, size_t dataLen, const char* hexSignature, const void* pubkey,
                         size_t pubkeyLen);
