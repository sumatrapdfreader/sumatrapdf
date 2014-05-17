/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef CryptoUtil_h
#define CryptoUtil_h

void CalcMD5Digest(const unsigned char *data, size_t byteCount, unsigned char digest[16]);
void CalcSHA2Digest(const unsigned char *data, size_t byteCount, unsigned char digest[32]);

void CalcMD5DigestWin(const void *data, size_t byteCount, unsigned char digest[16]);
void CalcSha1DigestWin(const void *data, size_t byteCount, unsigned char digest[32]);

#endif
