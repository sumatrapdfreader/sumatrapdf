/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Crypto.h"

#if OS_DARWIN
#include <CommonCrypto/CommonDigest.h>

void CalcMD5Digest(Str data, u8 digest[16]) {
    CC_MD5(data.s, (CC_LONG)data.len, digest);
}

void CalcSHA1Digest(Str data, u8 digest[20]) {
    CC_SHA1(data.s, (CC_LONG)data.len, digest);
}

void CalcSHA2Digest(Str data, u8 digest[32]) {
    CC_SHA256(data.s, (CC_LONG)data.len, digest);
}

#else
#include <openssl/md5.h>
#include <openssl/sha.h>

void CalcMD5Digest(Str data, u8 digest[16]) {
    MD5((const u8*)data.s, (size_t)data.len, digest);
}

void CalcSHA1Digest(Str data, u8 digest[20]) {
    SHA1((const u8*)data.s, (size_t)data.len, digest);
}

void CalcSHA2Digest(Str data, u8 digest[32]) {
    SHA256((const u8*)data.s, (size_t)data.len, digest);
}

#endif

bool VerifySHA1Signature(Str, Str, Str) {
    return false;
}

Str ExtractP7m(Str) {
    return {};
}
