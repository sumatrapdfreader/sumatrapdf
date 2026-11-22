/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// the "<sha1> <size> <mtime>" form FileState.ContentSha1 stores

#include "base/Base.h"

#include "ContentHash.h"

#include "base/tests/UtAssert.h"

void ContentHash_UnitTests() {
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
