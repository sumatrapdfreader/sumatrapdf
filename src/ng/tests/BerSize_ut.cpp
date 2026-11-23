/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// how much of a signature's zero-padded /Contents is the CMS message:
// DER, or BER with indefinite lengths as some signers write it

#include "base/Base.h"

#include "mupdf/ber_size.h"

#include "base/tests/UtAssert.h"

static size_t BerSize(const u8* p, size_t n) {
    return ber_element_size(p, n);
}

void BerSize_UnitTests() {
    // DER, short and long lengths; trailing zero padding isn't part of it
    const u8 shortDer[] = {0x30, 0x03, 0x02, 0x01, 0x05, 0x00, 0x00, 0x00};
    utassert(BerSize(shortDer, sizeof(shortDer)) == 5);
    u8 longDer[2 + 2 + 200 + 4] = {0x04, 0x81, 200};
    utassert(BerSize(longDer, 2 + 1 + 200) == 203);

    // indefinite: children up to the 00 00 end marker, nested too
    const u8 indef[] = {0x30, 0x80, 0x02, 0x01, 0x05, 0x30, 0x80, 0x04, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    utassert(BerSize(indef, sizeof(indef)) == 13);

    // malformed: truncated, no end marker, an indefinite primitive
    const u8 truncated[] = {0x30, 0x05, 0x02, 0x01};
    utassert(BerSize(truncated, sizeof(truncated)) == 0);
    const u8 noEnd[] = {0x30, 0x80, 0x02, 0x01, 0x05};
    utassert(BerSize(noEnd, sizeof(noEnd)) == 0);
    const u8 indefPrim[] = {0x04, 0x80, 0x00, 0x00};
    utassert(BerSize(indefPrim, sizeof(indefPrim)) == 0);

    // deep nesting of indefinite elements is bounded, not a stack overflow
    constexpr int kDepth = 1000;
    u8* deep = AllocArray<u8>(kDepth * 4);
    for (int i = 0; i < kDepth; i++) {
        deep[i * 2] = 0x30;
        deep[(i * 2) + 1] = 0x80;
    }
    utassert(BerSize(deep, kDepth * 4) == 0);
    free(deep);
}
