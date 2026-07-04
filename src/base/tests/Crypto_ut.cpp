/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Crypto.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

static bool TestDigestMD5(Str data, Str verify) {
    u8 digest[16];
    CalcMD5Digest(data, digest);
    TempStr hash = str::MemToHexTemp(Str((const char*)digest, dimofi(digest)));
    return str::Eq(hash, verify);
}

static bool TestDigestSHA1(Str data, Str verify) {
    u8 digest[20];
    CalcSHA1Digest(data, digest);
    TempStr hash = str::MemToHexTemp(Str((const char*)digest, dimofi(digest)));
    return str::Eq(hash, verify);
}

static bool TestDigestSHA2(Str data, Str verify) {
    u8 digest[32];
    CalcSHA2Digest(data, digest);
    TempStr hash = str::MemToHexTemp(Str((const char*)digest, dimofi(digest)));
    return str::Eq(hash, verify);
}

void CryptoUtilTest() {
    utassert(TestDigestMD5("", "d41d8cd98f00b204e9800998ecf8427e"));
    utassert(TestDigestMD5("The quick brown fox jumps over the lazy dog", "9e107d9d372bb6826bd81d3542a419d6"));
    utassert(TestDigestMD5("The quick brown fox jumps over the lazy dog.", "e4d909c290d0fb1ca068ffaddf22cbd0"));

    utassert(TestDigestSHA1("", "da39a3ee5e6b4b0d3255bfef95601890afd80709"));
    utassert(TestDigestSHA1("The quick brown fox jumps over the lazy dog", "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"));
    utassert(TestDigestSHA1("The quick brown fox jumps over the lazy cog", "de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3"));

    utassert(TestDigestSHA2("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    utassert(TestDigestSHA2("The quick brown fox jumps over the lazy dog",
                            "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"));
    utassert(TestDigestSHA2("The quick brown fox jumps over the lazy dog.",
                            "ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c"));

    // basic sanity for p7m extractor (no real p7m data, just ensure no crash and empty on bad input)
    utassert(len(ExtractP7m(Str())) == 0);
    utassert(len(ExtractP7m(Str((char*)"not a p7m", 9))) == 0);
    utassert(len(ExtractP7m(Str((char*)"%PDF-1.4", 8))) == 0);
}