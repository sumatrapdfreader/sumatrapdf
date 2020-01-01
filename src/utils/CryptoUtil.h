/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

void CalcMD5Digest(const unsigned char* data, size_t byteCount, unsigned char digest[16]);
void CalcSHA1Digest(const unsigned char* data, size_t byteCount, unsigned char digest[20]);
void CalcSHA2Digest(const unsigned char* data, size_t byteCount, unsigned char digest[32]);

void CalcMD5DigestWin(const void* data, size_t byteCount, unsigned char digest[16]);
void CalcSha1DigestWin(const void* data, size_t byteCount, unsigned char digest[20]);
void CalcSha2DigestWin(const void* data, size_t byteCount, unsigned char digest[32]);

bool VerifySHA1Signature(const void* data, size_t dataLen, const char* hexSignature, const void* pubkey,
                         size_t pubkeyLen);
