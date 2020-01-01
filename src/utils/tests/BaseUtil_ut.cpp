/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

static void GeomTest() {
    PointD ptD(12.4, -13.6);
    utassert(ptD.x == 12.4 && ptD.y == -13.6);
    PointI ptI = ptD.ToInt();
    utassert(ptI.x == 12 && ptI.y == -14);
    ptD = ptI.Convert<double>();
    utassert(PointD(12, -14) == ptD);
    utassert(PointD(12.4, -13.6) != ptD);

    SizeD szD(7.7, -3.3);
    utassert(szD.dx == 7.7 && szD.dy == -3.3);
    SizeI szI = szD.ToInt();
    utassert(szI.dx == 8 && szI.dy == -3);
    szD = szI.Convert<double>();
    utassert(SizeD(8, -3) == szD);

    utassert(!szD.IsEmpty() && !szI.IsEmpty());
    utassert(SizeI().IsEmpty() && SizeD().IsEmpty());

    struct SRIData {
        int x1s, x1e, y1s, y1e;
        int x2s, x2e, y2s, y2e;
        bool intersect;
        int i_xs, i_xe, i_ys, i_ye;
        int u_xs, u_xe, u_ys, u_ye;
    } testData[] = {
        {0, 10, 0, 10, 0, 10, 0, 10, true, 0, 10, 0, 10, 0, 10, 0, 10},  /* complete intersect */
        {0, 10, 0, 10, 20, 30, 20, 30, false, 0, 0, 0, 0, 0, 30, 0, 30}, /* no intersect */
        {0, 10, 0, 10, 5, 15, 0, 10, true, 5, 10, 0, 10, 0, 15, 0, 10},  /* { | } | */
        {0, 10, 0, 10, 5, 7, 0, 10, true, 5, 7, 0, 10, 0, 10, 0, 10},    /* { | | } */
        {0, 10, 0, 10, 5, 7, 5, 7, true, 5, 7, 5, 7, 0, 10, 0, 10},
        {0, 10, 0, 10, 5, 15, 5, 15, true, 5, 10, 5, 10, 0, 15, 0, 15},
    };

    for (size_t i = 0; i < dimof(testData); i++) {
        struct SRIData* curr = &testData[i];

        RectI rx1(curr->x1s, curr->y1s, curr->x1e - curr->x1s, curr->y1e - curr->y1s);
        RectI rx2 = RectI::FromXY(curr->x2s, curr->y2s, curr->x2e, curr->y2e);
        RectI isect = rx1.Intersect(rx2);
        if (curr->intersect) {
            utassert(!isect.IsEmpty());
            utassert(isect.x == curr->i_xs && isect.y == curr->i_ys);
            utassert(isect.x + isect.dx == curr->i_xe && isect.y + isect.dy == curr->i_ye);
        } else {
            utassert(isect.IsEmpty());
        }
        RectI urect = rx1.Union(rx2);
        utassert(urect.x == curr->u_xs && urect.y == curr->u_ys);
        utassert(urect.x + urect.dx == curr->u_xe && urect.y + urect.dy == curr->u_ye);

        /* if we swap rectangles, the results should be the same */
        std::swap(rx1, rx2);
        isect = rx1.Intersect(rx2);
        if (curr->intersect) {
            utassert(!isect.IsEmpty());
            utassert(isect.x == curr->i_xs && isect.y == curr->i_ys);
            utassert(isect.x + isect.dx == curr->i_xe && isect.y + isect.dy == curr->i_ye);
        } else {
            utassert(isect.IsEmpty());
        }
        urect = rx1.Union(rx2);
        utassert(RectI::FromXY(curr->u_xs, curr->u_ys, curr->u_xe, curr->u_ye) == urect);

        utassert(!rx1.Contains(PointI(-2, -2)));
        utassert(rx1.Contains(rx1.TL()));
        utassert(!rx1.Contains(PointI(rx1.x, INT_MAX)));
        utassert(!rx1.Contains(PointI(INT_MIN, rx1.y)));
    }
}

void BaseUtilTest() {
    utassert(RoundToPowerOf2(0) == 1);
    utassert(RoundToPowerOf2(1) == 1);
    utassert(RoundToPowerOf2(2) == 2);
    utassert(RoundToPowerOf2(3) == 4);
    utassert(RoundToPowerOf2(15) == 16);
    utassert(RoundToPowerOf2((1 << 13) + 1) == (1 << 14));
    utassert(RoundToPowerOf2((size_t)-42) == (size_t)-1);

    utassert(MurmurHash2(nullptr, 0) == 0x342CE6C);
    utassert(MurmurHash2("test", 4) != MurmurHash2("Test", 4));

    utassert(addOverflows<u8>(255, 1));
    utassert(addOverflows<u8>(255, 2));
    utassert(addOverflows<u8>(255, 255));
    utassert(!addOverflows<u8>(254, 1));
    utassert(!addOverflows<u8>(127, 1));
    utassert(!addOverflows<u8>(127, 127));
    utassert(!addOverflows<u8>(127, 128));
    utassert(addOverflows<u8>(127, 129));
    utassert(addOverflows<u8>(127, 255));
    GeomTest();
}
