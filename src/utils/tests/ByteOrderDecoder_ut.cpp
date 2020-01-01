/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ByteOrderDecoder.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

#define ABC "abc"
void ByteOrderTests() {
    unsigned char d1[] = {0x00, 0x01,
                          0x00,                               // to skip
                          0x01, 0x00, 0xff, 0xfe, 0x00, 0x00, // to skip
                          0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0xff,
                          0xff, 0xff, 0xfe, 0x02, 0x00, 'a',  'b',  'c'};

    {
        uint16_t vu16;
        uint32_t vu32;
        char b[3];
        ByteOrderDecoder d(d1, sizeof(d1), ByteOrderDecoder::LittleEndian);
        utassert(0 == d.Offset());
        vu16 = d.UInt16();
        utassert(2 == d.Offset());
        utassert(vu16 == 0x100);
        d.Skip(1);
        utassert(3 == d.Offset());
        vu16 = d.UInt16();
        utassert(5 == d.Offset());
        utassert(vu16 == 0x1);
        vu16 = d.UInt16();
        utassert(7 == d.Offset());
        utassert(vu16 == 0xfeff);
        d.Skip(2);
        utassert(9 == d.Offset());
        d.Unskip(4);
        utassert(5 == d.Offset());
        vu32 = d.UInt32();
        utassert(vu32 == 0xfeff);

        vu32 = d.UInt32();
        utassert(13 == d.Offset());
        utassert(vu32 == 0x1000000);
        vu32 = d.UInt32();
        utassert(17 == d.Offset());
        utassert(vu32 == 1);
        vu32 = d.UInt32();
        utassert(21 == d.Offset());
        utassert(vu32 == 0xfeffffff);

        vu16 = d.UInt16();
        utassert(vu16 == 0x02);
        utassert(23 == d.Offset());

        d.Bytes(b, 3);
        utassert(memeq(ABC, b, 3));
        utassert(26 == d.Offset());
    }

    {
        uint16_t vu16;
        uint32_t vu32;
        char b[3];
        ByteOrderDecoder d(d1, sizeof(d1), ByteOrderDecoder::BigEndian);
        vu16 = d.UInt16();
        utassert(vu16 == 1);
        d.Skip(1);
        vu16 = d.UInt16();
        utassert(vu16 == 0x100);
        vu16 = d.UInt16();
        utassert(vu16 == 0xfffe);
        d.Skip(2);

        vu32 = d.UInt32();
        utassert(vu32 == 1);
        vu32 = d.UInt32();
        utassert(vu32 == 0x1000000);
        vu32 = d.UInt32();
        utassert(vu32 == 0xfffffffe);

        vu16 = d.UInt16();
        utassert(vu16 == 0x200);
        d.Bytes(b, 3);
        utassert(memeq(ABC, b, 3));
        utassert(26 == d.Offset());
    }

    {
        int16_t v16;
        int32_t v32;
        char b[3];
        ByteOrderDecoder d(d1, sizeof(d1), ByteOrderDecoder::LittleEndian);
        v16 = d.Int16();
        utassert(v16 == 0x100);
        d.Skip(1);
        v16 = d.Int16();
        utassert(v16 == 0x1);
        v16 = d.Int16();
        utassert(v16 == -257);
        d.Skip(2);

        v32 = d.Int32();
        utassert(v32 == 0x1000000);
        v32 = d.Int32();
        utassert(v32 == 1);
        v32 = d.Int32();
        utassert(v32 == -16777217);

        v16 = d.Int16();
        utassert(v16 == 0x2);
        d.Bytes(b, 3);
        utassert(memeq(ABC, b, 3));
        utassert(26 == d.Offset());
    }

    {
        int16_t v16;
        int32_t v32;
        char b[3];
        ByteOrderDecoder d(d1, sizeof(d1), ByteOrderDecoder::BigEndian);
        v16 = d.Int16();
        utassert(v16 == 0x1);
        d.Skip(1);
        v16 = d.Int16();
        utassert(v16 == 0x100);
        v16 = d.Int16();
        utassert(v16 == -2);
        d.Skip(2);

        v32 = d.Int32();
        utassert(v32 == 1);
        v32 = d.Int32();
        utassert(v32 == 0x1000000);
        v32 = d.Int32();
        utassert(v32 == -2);

        v16 = d.Int16();
        utassert(v16 == 0x200);
        d.Bytes(b, 3);
        utassert(memeq(ABC, b, 3));
        utassert(26 == d.Offset());
    }
}

#undef ABC
