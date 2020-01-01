/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/CryptoUtil.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

static bool TestDigestMD5(const char* data, size_t size, const char* verify) {
    unsigned char digest[16];
    CalcMD5Digest((const unsigned char*)data, size, digest);
    AutoFree hash(_MemToHex(&digest));
    return str::Eq(hash, verify);
}

static bool TestDigestSHA1(const char* data, size_t size, const char* verify) {
    unsigned char digest[20];
    CalcSHA1Digest((const unsigned char*)data, size, digest);
    AutoFree hash(_MemToHex(&digest));
    return str::Eq(hash, verify);
}

static bool TestDigestSHA2(const char* data, size_t size, const char* verify) {
    unsigned char digest[32];
    CalcSHA2Digest((const unsigned char*)data, size, digest);
    AutoFree hash(_MemToHex(&digest));
    return str::Eq(hash, verify);
}

void CryptoUtilTest() {
    utassert(TestDigestMD5("", 0, "d41d8cd98f00b204e9800998ecf8427e"));
    utassert(TestDigestMD5("The quick brown fox jumps over the lazy dog", 43, "9e107d9d372bb6826bd81d3542a419d6"));
    utassert(TestDigestMD5("The quick brown fox jumps over the lazy dog.", 44, "e4d909c290d0fb1ca068ffaddf22cbd0"));

    utassert(TestDigestSHA1("", 0, "da39a3ee5e6b4b0d3255bfef95601890afd80709"));
    utassert(
        TestDigestSHA1("The quick brown fox jumps over the lazy dog", 43, "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"));
    utassert(
        TestDigestSHA1("The quick brown fox jumps over the lazy cog", 43, "de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3"));

    utassert(TestDigestSHA2("", 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    utassert(TestDigestSHA2("The quick brown fox jumps over the lazy dog", 43,
                            "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"));
    utassert(TestDigestSHA2("The quick brown fox jumps over the lazy dog.", 44,
                            "ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c"));
}
