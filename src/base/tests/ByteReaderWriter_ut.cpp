/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ByteReaderWriter.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

#define kAbc "abc"
void ByteOrderTests() {
    u8 d1[] = {0x00, 0x01,
               0x00,                               // to skip
               0x01, 0x00, 0xff, 0xfe, 0x00, 0x00, // to skip
               0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xfe, 0x02, 0x00, 'a', 'b', 'c'};

    {
        u16 vu16;
        u32 vu32;
        char b[3];
        ByteReader d(d1, sizeof(d1));
        utassert(0 == d.Offset());
        vu16 = d.UInt16LE();
        utassert(2 == d.Offset());
        utassert(vu16 == 0x100);
        d.Skip(1);
        utassert(3 == d.Offset());
        vu16 = d.UInt16LE();
        utassert(5 == d.Offset());
        utassert(vu16 == 0x1);
        vu16 = d.UInt16LE();
        utassert(7 == d.Offset());
        utassert(vu16 == 0xfeff);
        d.Skip(2);
        utassert(9 == d.Offset());
        d.Unskip(4);
        utassert(5 == d.Offset());
        vu32 = d.UInt32LE();
        utassert(vu32 == 0xfeff);

        vu32 = d.UInt32LE();
        utassert(13 == d.Offset());
        utassert(vu32 == 0x1000000);
        vu32 = d.UInt32LE();
        utassert(17 == d.Offset());
        utassert(vu32 == 1);
        vu32 = d.UInt32LE();
        utassert(21 == d.Offset());
        utassert(vu32 == 0xfeffffff);

        vu16 = d.UInt16LE();
        utassert(vu16 == 0x02);
        utassert(23 == d.Offset());

        d.Bytes(b, 3);
        utassert(MemEq(kAbc, b, 3));
        utassert(26 == d.Offset());
    }

    {
        u16 vu16;
        u32 vu32;
        char b[3];
        ByteReader d(d1, sizeof(d1));
        vu16 = d.UInt16BE();
        utassert(vu16 == 1);
        d.Skip(1);
        vu16 = d.UInt16BE();
        utassert(vu16 == 0x100);
        vu16 = d.UInt16BE();
        utassert(vu16 == 0xfffe);
        d.Skip(2);

        vu32 = d.UInt32BE();
        utassert(vu32 == 1);
        vu32 = d.UInt32BE();
        utassert(vu32 == 0x1000000);
        vu32 = d.UInt32BE();
        utassert(vu32 == 0xfffffffe);

        vu16 = d.UInt16BE();
        utassert(vu16 == 0x200);
        d.Bytes(b, 3);
        utassert(MemEq(kAbc, b, 3));
        utassert(26 == d.Offset());
    }

    {
        // sizeHint must allocate; setting cap without els used to drop writes
        ByteWriterLE wr(64);
        wr.Write8x2('I', 'I');
        wr.Write16(42);
        Str s = wr.AsByteSlice();
        utassert(len(s) == 4 && s.s);
        utassert(s.s[0] == 'I' && s.s[1] == 'I');
        utassert((u8)s.s[2] == 42 && s.s[3] == 0);
    }
}

#undef kAbc
