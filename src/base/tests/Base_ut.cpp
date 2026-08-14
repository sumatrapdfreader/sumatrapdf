/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

struct TestFn0Data {
    int n = 0;
};

static void testFn0(TestFn0Data* d) {
    d->n++;
}

static void Func0Test() {
    TestFn0Data d;
    auto fn = MkFunc0(testFn0, &d);
    fn.Call();
    utassert(d.n == 1);
}

struct TestFn1Data {
    int p = 0;
};

static void testFn1(TestFn0Data* d0, TestFn1Data* d1) {
    d0->n = 5;
    d1->p = -8;
}

static void Func1Test() {
    TestFn0Data d0;
    TestFn1Data d1;
    auto fn = MkFunc1<TestFn0Data, TestFn1Data*>(testFn1, &d0);
    fn.Call(&d1);
    utassert(d0.n == 5);
    utassert(d1.p == -8);
}

static int gFn0VoidCalls = 0;

static void testFn0Void() {
    gFn0VoidCalls++;
}

// a Func0 can stand in for a Func1: Call() drops the argument. The conversion
// stores a void(void*) in a void(void*, T) slot, so this checks the dispatch
// actually lands in the right function rather than merely compiling.
static void Func1FromFunc0Test() {
    TestFn0Data d0;
    TestFn1Data d1{.p = 7};

    // the drops-argument flag lives in userData's low bit, so Func1 is still
    // two words - a bool member would have cost 8 more after padding
    utassert(sizeof(Func1<TestFn1Data*>) == 2 * sizeof(void*));

    Func1<TestFn1Data*> fn = MkFunc0(testFn0, &d0);
    utassert(fn.IsValid());
    fn.Call(&d1);
    utassert(d0.n == 1); // the Func0 ran
    utassert(d1.p == 7); // and never saw the argument

    fn.Call(nullptr); // dropped, so a null argument is harmless
    utassert(d0.n == 2);

    // the no-user-data flavour, where Func0 holds a void()
    gFn0VoidCalls = 0;
    Func1<TestFn1Data*> fnv = MkFunc0Void(testFn0Void);
    fnv.Call(&d1);
    utassert(gFn0VoidCalls == 1);
    utassert(d1.p == 7);

    // copies keep dropping the argument
    Func1<TestFn1Data*> copy = fn;
    copy.Call(&d1);
    utassert(d0.n == 3);
    utassert(d1.p == 7);

    // a real Func1 still gets its argument
    Func1<TestFn1Data*> real = MkFunc1<TestFn0Data, TestFn1Data*>(testFn1, &d0);
    real.Call(&d1);
    utassert(d0.n == 5);
    utassert(d1.p == -8);
}

static void GeomTest() {
    PointF ptD(12.4f, -13.6f);
    utassert(ptD.x == 12.4f && ptD.y == -13.6f);
    Point ptI = ToPoint(ptD);
    utassert(ptI.x == 12 && ptI.y == -13);

    SizeF szD(7.7f, -3.3f);
    utassert(szD.dx == 7.7f && szD.dy == -3.3f);
    Size szI = ToSize(szD);
    utassert(szI.dx == 8 && szI.dy == -3);
    szD = ToSizeFl(szI);
    utassert(SizeF(8, -3) == szD);

    utassert(!szD.IsEmpty() && !szI.IsEmpty());
    utassert(Size().IsEmpty() && SizeF().IsEmpty());

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

        Rect rx1(curr->x1s, curr->y1s, curr->x1e - curr->x1s, curr->y1e - curr->y1s);
        Rect rx2 = Rect::FromXY(curr->x2s, curr->y2s, curr->x2e, curr->y2e);
        Rect isect = rx1.Intersect(rx2);
        if (curr->intersect) {
            utassert(!isect.IsEmpty());
            utassert(isect.x == curr->i_xs && isect.y == curr->i_ys);
            utassert(isect.x + isect.dx == curr->i_xe && isect.y + isect.dy == curr->i_ye);
        } else {
            utassert(isect.IsEmpty());
        }
        Rect urect = rx1.Union(rx2);
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
        utassert(Rect::FromXY(curr->u_xs, curr->u_ys, curr->u_xe, curr->u_ye) == urect);

        utassert(!rx1.Contains(Point(-2, -2)));
        utassert(rx1.Contains(rx1.TL()));
        utassert(!rx1.Contains(Point(rx1.x, INT_MAX)));
        utassert(!rx1.Contains(Point(INT_MIN, rx1.y)));
    }
}

static int roundUpTestCases[] = {
    0, 0, 1, 8, 2, 8, 3, 8, 4, 8, 5, 8, 6, 8, 7, 8, 8, 8, 9, 16,
};

struct ListNode {
    struct ListNode* next = nullptr;
    int n = 0;
    ListNode() = default;
};

static void CheckListOrder(ListNode* root, int* seq) {
    ListNode* el = root;
    for (int n = *seq; n >= 0; n = *(++seq)) {
        utassert(el->n == n);
        el = el->next;
    }
    utassert(!el);
}

static void ListTest() {
    int n = 5;

    static int orderReverse[] = {5, 4, 3, 2, 1, -1};
    static int orderNormal[] = {1, 2, 3, 4, 5, -1};
    {
        ListNode* root = nullptr;
        for (int i = 1; i <= n; i++) {
            auto node = new ListNode();
            node->n = i;
            ListInsertFront(&root, node);
        }
        CheckListOrder(root, orderReverse);
        ListReverse(&root);
        CheckListOrder(root, orderNormal);
        ListDelete(root);
    }
    {
        ListNode* root = nullptr;
        for (int i = 1; i <= n; i++) {
            auto node = new ListNode();
            node->n = i;
            ListInsertEnd(&root, node);
        }
        CheckListOrder(root, orderNormal);
        ListDelete(root);
    }
}

static void ColorTest() {
    ParsedColor parsed;
    ParseColor(parsed, "#f2f2f2");
    utassert(parsed.parsedOk);
    utassert(parsed.col == MkRgb(0xf2, 0xf2, 0xf2));

    parsed = {};
    ParseColor(parsed, "#80f2f2f2");
    utassert(parsed.parsedOk);
    utassert(parsed.col == MkRgba(0xf2, 0xf2, 0xf2, 0x80));

    parsed = {};
    ParseColor(parsed, "#f2f2f");
    utassert(!parsed.parsedOk);
}

static void ArenaPtrCompressTest() {
    // Single-block round-trip
    {
        Arena* a = ArenaNew();
        utassert(a != nullptr);
        int* p = (int*)a->Push(sizeof(int) * 4, 8, true);
        p[0] = 11;
        p[1] = 22;
        u32 c = ArenaPtrCompress(a, p);
        utassert(c != 0);
        utassert(c >= (u32)kArenaHeaderSize);
        utassert(ArenaPtrUncompress(a, c) == p);
        utassert(ArenaPtrUncompress<int>(a, c) == p);
        utassert(ArenaPtrUncompress<int>(a, c)[1] == 22);
        utassert(ArenaPtrCompress(a, nullptr) == 0);
        utassert(ArenaPtrUncompress(a, 0) == nullptr);
        ArenaDelete(a);
    }

    // Multi-block chain: tiny reserve so a second Push allocates a new block
    {
        ArenaParams params = ArenaDefaultParams();
        params.reserveSize = 4 * 1024;
        params.commitSize = 4 * 1024;
        Arena* a = ArenaNew(params);
        utassert(a != nullptr);
        utassert(a->current == a);
        utassert(a->basePos == 0);

        // ArenaNew rounds the reserve up to a page, and a page is 16K on arm64
        // macOS, not 4K - so size the pushes from the block we actually got.
        // Two of these plus kArenaHeaderSize can't fit in one block.
        u64 half = a->reserved / 2;

        // Fill most of the first block (header is kArenaHeaderSize)
        void* p1 = a->Push(half, 8, true);
        utassert(p1 != nullptr);
        utassert(a->current == a);

        // Force a second arena block in the chain
        void* p2 = a->Push(half, 8, true);
        utassert(p2 != nullptr);
        utassert(a->current != a);
        utassert(a->current->prev == a);
        utassert(a->current->basePos == a->reserved);

        u32 c1 = ArenaPtrCompress(a, p1);
        u32 c2 = ArenaPtrCompress(a, p2);
        utassert(c1 != 0 && c2 != 0);
        utassert(c1 < a->reserved);  // still in the first block's range
        utassert(c2 >= a->reserved); // past the first block
        utassert(c2 > c1);

        utassert(ArenaPtrUncompress(a, c1) == p1);
        utassert(ArenaPtrUncompress(a, c2) == p2);
        utassert(ArenaPtrUncompress<char>(a, c2) == (char*)p2);

        // Write through uncompressed pointer in the second block
        char* s = ArenaPtrUncompress<char>(a, c2);
        s[0] = 'Z';
        utassert(((char*)p2)[0] == 'Z');

        // Another allocation on the second block still round-trips
        void* p3 = a->Push(64, 8, true);
        utassert(a->current != a);
        u32 c3 = ArenaPtrCompress(a, p3);
        utassert(ArenaPtrUncompress(a, c3) == p3);
        utassert(c3 > c2);

        ArenaDelete(a);
    }
}

void BaseUtilTest() {
    ListTest();
    Func0Test();
    Func1Test();
    Func1FromFunc0Test();
    ColorTest();
    ArenaPtrCompressTest();

    size_t n = dimof(roundUpTestCases) / 2;
    for (size_t i = 0; i < n; i++) {
        int v = roundUpTestCases[i * 2];
        int exp = roundUpTestCases[(i * 2) + 1];
        int got = RoundUp(v, 8);
        utassert(exp == got);
        void* got3 = RoundUp((void*)(uintptr_t)v, 8);
        utassert(got3 == (void*)(uintptr_t)exp);
    }

    utassert(RoundToPowerOf2(0) == 1);
    utassert(RoundToPowerOf2(1) == 1);
    utassert(RoundToPowerOf2(2) == 2);
    utassert(RoundToPowerOf2(3) == 4);
    utassert(RoundToPowerOf2(15) == 16);
    utassert(RoundToPowerOf2((1 << 13) + 1) == (1 << 14));
    utassert(RoundToPowerOf2((1 << 30) + 1) == -1); // overflow: no power of 2 fits in an int

    utassert(MurmurHash2(nullptr, 0) == 0);
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
