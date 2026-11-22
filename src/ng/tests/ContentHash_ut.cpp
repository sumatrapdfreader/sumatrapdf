/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// the "<sha1> <size> <mtime>" form FileState.ContentSha1 stores

#include "base/Base.h"
#include "base/Crypto.h"

#include "ContentHash.h"

#include "base/tests/UtAssert.h"

// Sha1Update in pieces of every size gives the one-shot digest
static void Sha1InPieces() {
    char buf[300];
    for (int i = 0; i < dimofi(buf); i++) {
        buf[i] = (char)(i * 7);
    }
    for (int total : {0, 1, 55, 56, 63, 64, 65, 128, 300}) {
        u8 want[20];
        CalcSHA1Digest(Str(buf, total), want);
        for (int piece : {1, 3, 64, 100}) {
            Sha1Hasher* h = NewSha1Hasher();
            for (int off = 0; off < total; off += piece) {
                int n = std::min(piece, total - off);
                Sha1Update(h, Str(buf + off, n));
            }
            u8 got[20];
            Sha1Finish(h, got);
            utassert(memcmp(want, got, sizeof(got)) == 0);
        }
    }
}

void ContentHash_UnitTests() {
    Sha1InPieces();
    ContentHash h;
    Str s = StrL("da39a3ee5e6b4b0d3255bfef95601890afd80709 1234 133000000000000000");
    utassert(ParseContentHash(s, h));
    utassert(h.size == 1234 && h.mtime == 133000000000000000ULL);
    utassert(h.sha1[0] == 0xda && h.sha1[19] == 0x09);
    utassert(str::Eq(FormatContentHashTemp(h), s));

    ContentHash bad;
    utassert(!ParseContentHash(StrL(""), bad));
    utassert(!ParseContentHash(StrL("da39a3ee 1 2"), bad));
    utassert(!ParseContentHash(StrL("da39a3ee5e6b4b0d3255bfef95601890afd80709 12"), bad));
    utassert(!ParseContentHash(StrL("da39a3ee5e6b4b0d3255bfef95601890afd80709 -1 2"), bad));
}
