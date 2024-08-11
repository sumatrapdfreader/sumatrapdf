/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

// must be last due to assert() over-write
#include "utils/UtAssert.h"

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

static const char* strings[] = {"s1", "string", "another one", "and one more"};

static void PoolAllocatorStringsTest(PoolAllocator& a, int nRounds) {
    a.Reset();

    int nStrings = (int)dimof(strings);
    for (int i = 0; i < nRounds; i++) {
        for (int j = 0; j < nStrings; j++) {
            const char* s = strings[j];
            char* got = str::Dup(&a, s);
            utassert(str::Eq(s, got));
        }
    }

    int nTotal = nStrings * nRounds;
    for (int i = 0; i < nTotal; i++) {
        const char* exp = strings[i % nStrings];

        void* d = a.At(i);
        char* got = (char*)d;
        utassert(str::Eq(exp, got));
    }
}

static void PoolAllocatorTest() {
    PoolAllocator a;
    PoolAllocatorStringsTest(a, 2048);
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

void BaseUtilTest() {
    ListTest();
    Func0Test();
    Func1Test();

    PoolAllocatorTest();

    size_t n = dimof(roundUpTestCases) / 2;
    for (size_t i = 0; i < n; i++) {
        int v = roundUpTestCases[i * 2];
        int exp = roundUpTestCases[i * 2 + 1];
        int got = RoundUp(v, 8);
        utassert(exp == got);
        size_t got2 = RoundUp((size_t)v, (size_t)8);
        utassert(got2 == (size_t)exp);
        char* got3 = RoundUp((char*)(uintptr_t)v, (int)8);
        utassert(got3 == (char*)(uintptr_t)exp);
    }

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
