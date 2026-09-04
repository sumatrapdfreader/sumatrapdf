/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

static void ValidateSize(StrVec* v) {
    int size1 = v->size;
    int size2 = 0;
    auto page = v->first;
    while (page) {
        size2 += StrVecPageSize(page);
        page = StrVecPageNext(page);
    }
    utassert(size1 == size2);
}

static void ValidateAtStr(StrVec* v, int idx, Str s) {
    Str sp = v->At(idx);
    ReportIf(!str::Eq(s, sp));
    ReportIf(s.len != sp.len);
}

static void strEq(Str s1, Str s2) {
    bool ok = str::Eq(s1, s2);
    utassert(ok);
}

static void TestRemoveFromStart(StrVec* v) {
    while (!v->IsEmpty()) {
        Str s = v->At(0);
        bool ok = v->Remove(s);
        utassert(ok);
    }
}

static int randIdx(StrVec* v) {
    int n = len(*v);
    int idx = rand() % n;
    return idx;
}

static void TestRandomRemove(StrVec* v) {
    int idx;
    while (!v->IsEmpty()) {
        idx = randIdx(v);
        Str s = v->At(idx);
        bool ok = v->Remove(s);
        utassert(ok);
    }
}

static void TestFind(const StrVec* v) {
    int n = len(*v);
    Str s, s2;
    for (int i = 0; i < n; i++) {
        s = v->At(i);
        int i2 = v->Find(s);
        if (i != i2) {
            s2 = v->At(i2);
            utassert(str::Eq(s, s2));
        }
        i2 = v->FindI(s);
        if (i != i2) {
            s2 = v->At(i2);
            utassert(str::EqI(s, s2));
        }
    }
}

static void TestRemoveAt(StrVec* v) {
    TestFind(v);
    StrVec* v2 = new StrVec(*v);
    StrVec* v3 = new StrVec(*v);
    while (len(*v) > 0) {
        int n = len(*v);
        int idx = len(*v) / 2;
        Str exp = v->At(idx);
        Str got;
        if (n % 2 == 0) {
            got = v->RemoveAt(idx);
        } else {
            got = v->RemoveAtFast(idx);
        }
        utassert(str::Eq(exp, got));
        utassert(len(*v) == n - 1);
    }

    TestRandomRemove(v2);
    delete v2;

    TestRemoveFromStart(v3);
    delete v3;
}

static void StrVecCheckIter(StrVec* v, Str* strings, int start = 0) {
    TestFind(v);

    int i = 0;
    for (Str s : *v) {
        if (i < start) {
            i++;
            continue;
        }
        Str s2 = v->At(i);
        utassert(str::Eq(s, s2));
        if (strings) {
            Str s3 = strings[i - start];
            utassert(str::Eq(s, s3));
        }
        i++;
    }
    if (!strings) {
        return;
    }

    // test iterator + operator
    auto it = v->begin() + start;
    auto end = v->end();
    i = 0;
    for (; it != end; it++, i++) {
        Str s = *it;
        Str s2 = strings[i];
        utassert(str::Eq(s, s2));
    }
}

static void AppendStrings(StrVec* v, Str* strings, int nStrings) {
    int initialSize = len(*v);
    for (int i = 0; i < nStrings; i++) {
        v->Append(strings[i]);
        utassert(len(*v) == initialSize + i + 1);
    }
    StrVecCheckIter(v, strings, initialSize);
}

static Str strs[] = {StrL("foo"), StrL("bar"), StrL("Blast"), {}, StrL("this is a large string, my friend")};
// order in strs
static int unsortedOrder[] = {0, 1, 2, 3, 4};
static int sortedOrder[]{3, 2, 1, 0, 4};
static int sortedNoCaseOrder[]{3, 1, 2, 0, 4};

static void StrVecTest1_1(StrVec* v) {
    Str s = StrL("lolda");
    v->InsertAt(0, s);
    utassert(len(*v) == 1);
    utassert(str::Eq(v->At(0), s));
    TestRandomRemove(v);
}

static void StrVecTest1_2(StrVec* v) {
    utassert(len(*v) == 0);
    int n = dimofi(strs);
    AppendStrings(v, strs, n);
    StrVecCheckIter(v, strs, 0);
}

static void StrVecTest1_3(StrVec* v) {
    int n = len(*v);
    // allocate a bunch to test allocating
    Str str = strs[4];
    for (int i = 0; i < 1024; i++) {
        v->Append(str);
    }
    utassert(len(*v) == 1024 + n);

    for (int i = 0; i < n; i++) {
        auto got = v->At(i);
        auto exp = strs[unsortedOrder[i]];
        strEq(got, exp);
    }

    for (int i = 0; i < 1024; i++) {
        auto got = v->At(i + n);
        strEq(got, str);
    }
}

static void StrVecTest1_4(StrVec* v) {
    v->SetAt(3, Str());
    utassert(len(v->At(3)) == 0);
    TestRemoveAt(v);
}

struct Data1 {
    u16 n;
};

struct Data2 {
    char b;
    void* p;
    i64 n;
};

static void StrVecTest1() {
    {
        StrVec v;
        StrVecTest1_1(&v);
    }
    {
        StrVecWithData<Data1> v;
        StrVecTest1_1(&v);
    }

    {
        StrVecWithData<Data1> v;
        StrVecTest1_2(&v);
    }
    StrVec v;
    StrVecTest1_2(&v);

    StrVecWithData<Data1> vd;
    StrVecTest1_2(&vd);

    StrVec sortedView = v;
    Sort(&sortedView);

    int n = dimofi(strs);
    for (int i = 0; i < n; i++) {
        Str got = sortedView[i];
        auto exp = strs[sortedOrder[i]];
        strEq(got, exp);
    }

    StrVecTest1_3(&v);
    StrVecTest1_3(&vd);

    SortNoCase(&sortedView);

    for (int i = 0; i < n; i++) {
        auto got = sortedView[i];
        auto exp = strs[sortedNoCaseOrder[i]];
        strEq(got, exp);
    }
    TestRandomRemove(&sortedView);

    Sort(&v);
    for (int i = 0; i < n; i++) {
        Str got = v[i];
        auto exp = strs[sortedOrder[i]];
        strEq(got, exp);
    }
    StrVecCheckIter(&v, nullptr);
    SortNoCase(&v);
    for (int i = 0; i < n; i++) {
        Str got = v[i];
        auto exp = strs[sortedNoCaseOrder[i]];
        strEq(got, exp);
    }
    StrVecTest1_4(&v);
    StrVecTest1_4(&vd);
}

static void StrVecTest2_1(StrVec* v) {
    v->Append(StrL("foo"));
    v->Append(StrL("bar"));
    Str s = Join(v);
    utassert(len(*v) == 2);
    utassert(str::Eq(StrL("foobar"), s));

    s = Join(v, StrL(";"));
    utassert(len(*v) == 2);
    utassert(str::Eq(StrL("foo;bar"), s));

    v->Append(Str());
    utassert(len(*v) == 3);

    v->Append(StrL("glee"));
    s = JoinTemp(v, StrL("_ _"));
    utassert(len(*v) == 4);
    utassert(str::Eq(StrL("foo_ _bar_ _glee"), s));

    StrVecCheckIter(v, nullptr);
}

static void StrVecTest2_2(StrVec* v) {
    Sort(v);
    Str strsSorted[] = {{}, StrL("bar"), StrL("foo"), StrL("glee")};
    StrVecCheckIter(v, strsSorted);

    auto s = Join(v, StrL("++"));
    utassert(len(*v) == 4);
    utassert(str::Eq(StrL("bar++foo++glee"), s));

    s = Join(v);
    utassert(str::Eq(StrL("barfooglee"), s));
}

static void StrVecTest2_3(StrVec* v2) {
    int n = Split(v2, StrL("a,b,,c,"), StrL(","));
    utassert(n == 5 && v2->Find(StrL("c")) == 3);
    utassert(v2->Find(StrL("")) == 2);
    utassert(v2->Find(StrL(""), 3) == 4);
    utassert(v2->Find(StrL(""), 5) == -1);
    utassert(v2->Find(StrL("B")) == -1 && v2->FindI(StrL("B")) == 1);
    TempStr joined = JoinTemp(v2, StrL(";"));
    utassert(str::Eq(joined, StrL("a;b;;c;")));
    TestRemoveAt(v2);
}

static void StrVecTest2_4(StrVec* v2) {
    int n = Split(v2, StrL("a,b,,c,"), StrL(","), true);
    utassert(n == 3 && v2->Find(StrL("c")) == 2);
    TempStr joined = JoinTemp(v2, StrL(";"));
    utassert(str::Eq(joined, StrL("a;b;c")));
    StrVecCheckIter(v2, nullptr);

    TestRemoveAt(v2);
}

static void StrVecTest2_5(StrVec* v2) {
    int n = Split(v2, StrL("a,b,,c,d"), StrL(","), true, 3);
    Str s = JoinTemp(v2, StrL("__"));
    utassert(n == 3);
    utassert(str::Eq(s, StrL("a__b__c,d")));

    v2->Reset();
    n = Split(v2, StrL("a,b,,c,d"), StrL(","), false, 3);
    s = JoinTemp(v2, StrL("__"));
    utassert(n == 3);
    // TODO: fix me
    utassert(str::Eq(s, StrL("a__b__,c,d")));

    v2->Reset();
    n = Split(v2, StrL("a,b,,c,d"), StrL(","), true, 1);
    utassert(n == 1);
    s = v2->At(0);
    utassert(str::Eq(s, StrL("a,b,,c,d")));

    // max 0 is turned into 1
    v2->Reset();
    n = Split(v2, StrL("a,b,,c,d"), StrL(","), true, 0);
    s = v2->At(0);
    utassert(str::Eq(s, StrL("a,b,,c,d")));
}

static void StrVecTest2() {
    Str s;

    StrVec v;
    StrVecTest2_1(&v);
    StrVecTest2_2(&v);
    {
        StrVecWithData<Data1> vd;
        StrVecTest2_1(&vd);
        StrVecTest2_2(&vd);
    }

    {
        StrVec v2(v);
        utassert(str::Eq(v2[2], StrL("foo")));
        v2.Append(StrL("nobar"));
        utassert(str::Eq(v2[4], StrL("nobar")));
        v2 = v;
        utassert(len(v2) == 4);
        // copies should be same values but at different addresses
        utassert(v2[1].s != v[1].s);
        utassert(str::Eq(v2[1], v[1]));
        s = v2[2];
        utassert(str::Eq(s, StrL("foo")));
        TestRemoveAt(&v2);
    }

    {
        StrVec v2;
        StrVecTest2_3(&v2);
        StrVecWithData<Data1> vd;
        StrVecTest2_3(&vd);
    }

    {
        StrVec v2;
        StrVecTest2_4(&v2);
        StrVecWithData<Data1> vd;
        StrVecTest2_4(&vd);
    }
    {
        StrVec v2;
        StrVecTest2_5(&v2);
        StrVecWithData<Data1> vd;
        StrVecTest2_5(&vd);
    }

    TestRemoveAt(&v);
}

static void StrVecTest3_1(StrVec* v) {
    utassert(len(*v) == 0);
    v->Append(StrL("one"));
    v->Append(StrL("two"));
    v->Append(StrL("One"));
    utassert(len(*v) == 3);
    utassert(str::Eq(v->At(0), StrL("one")));
    utassert(str::EqI(v->At(2), StrL("one")));
    utassert(v->Find(StrL("One")) == 2);
    utassert(v->FindI(StrL("One")) == 0);
    utassert(v->Find(StrL("Two")) == -1);
    StrVecCheckIter(v, nullptr);
}

static void StrVecTest3() {
    {
        StrVec v;
        StrVecTest3_1(&v);
        TestRemoveAt(&v);
    }
    {
        StrVecWithData<Data1> v;
        StrVecTest3_1(&v);
        TestRemoveAt(&v);
    }
}

static void StrVecTest4_1(StrVec* v) {
    AppendStrings(v, strs, dimofi(strs));

    int idx = 2;

    utassert(str::Eq(strs[idx], v->At(idx)));
    Str s = StrL("new value of string, should be large to get results faster");
    // StrVec: tests adding where can allocate new value inside a page
    v->SetAt(idx, Str(s));
    utassert(str::Eq(s, v->At(idx)));
    v->SetAt(idx, Str());
    utassert(len(v->At(idx)) == 0);
    v->SetAt(idx, StrL(""));
    utassert(str::Eq(StrL(""), v->At(idx)));
    // StrVec: force allocating in side strings
    // first page is 256 bytes so this should force allocation in sideStrings
    int n = 256 / len(s);
    for (int i = 0; i < n; i++) {
        v->SetAt(idx, Str(s));
    }
    utassert(str::Eq(s, v->At(idx)));

    auto prevAtIdx = strs[idx];
    defer {
        strs[idx] = prevAtIdx;
    };
    strs[idx] = s;
    StrVecCheckIter(v, strs);

    auto s2 = v->RemoveAt(idx);
    utassert(str::Eq(s, s2));

    // should be replaced  by next value
    s2 = v->At(idx);
    Str s3 = strs[idx + 1];
    utassert(str::Eq(s2, s3));

    // StrVec: test multiple side strings
    n = len(*v);
    for (int i = 0; i < n; i++) {
        v->SetAt(i, Str(s));
    }
    for (auto it = v->begin(); it != v->end(); it++) {
        s2 = *it;
        utassert(str::Eq(s, s2));
    }
    s3 = StrL("hello");
    v->SetAt(n / 2, s3);
    s2 = v->At(n / 2);
    utassert(str::Eq(s3, s2));
    while (len(*v) > 0) {
        n = len(*v);
        s2 = v->At(0);
        if (n % 2 == 0) {
            s3 = v->RemoveAtFast(0);
        } else {
            s3 = v->RemoveAt(0);
        }
        utassert(str::Eq(s2, s3));
    }
}

static void StrVecTest4() {
    {
        StrVec v;
        StrVecTest4_1(&v);
    }
    {
        StrVecWithData<Data1> v;
        StrVecTest4_1(&v);
    }
}

static void StrVecTest5_1(StrVec* v) {
    AppendStrings(v, strs, dimofi(strs));
    Str s = StrL("first");
    v->InsertAt(0, s);
    auto s2 = v->At(0);
    utassert(str::Eq(s, s2));
    s = strs[0];
    s2 = v->At(1);
    utassert(str::Eq(s2, s));
    s = StrL("middle");
    v->InsertAt(3, s);
    s2 = v->At(3);
    utassert(str::Eq(s2, s));
}

static void StrVecTest5() {
    {
        StrVec v;
        StrVecTest5_1(&v);
    }
    {
        StrVecWithData<Data1> v;
        StrVecTest5_1(&v);
    }
}

static void StrVecTest6_1(StrVec* v) {
    Split(v, StrL(" CmdCreateAnnotHighlight   #00ff00 openEdit"), StrL(" "), true, 2);
    utassert(len(*v) == 2);
    Str s = v->At(0);
    utassert(str::Eq(s, StrL("CmdCreateAnnotHighlight")));
    s = v->At(1);
    utassert(str::Eq(s, StrL("#00ff00 openEdit")));
}

static void StrVecTest6() {
    {
        StrVec v;
        StrVecTest6_1(&v);
    }
    {
        StrVecWithData<Data1> v;
        StrVecTest6_1(&v);
    }
}

static void StrVecTest7_1(StrVec* v) {
    Split(v, StrL(""), StrL(" "), true, 2);
    utassert(len(*v) == 1);
    Str s = v->At(0);
    utassert(len(s) == 0 || s.s[0] == 0);
}

static void StrVecTest7() {
    {
        StrVec v;
        StrVecTest7_1(&v);
    }
    {
        StrVecWithData<Data1> v;
        StrVecTest7_1(&v);
    }
}

static StrVec* stringsForNum;
static constexpr int kMaxStringN = 1000;

static Str StrForN(int n) {
    ReportIf(n > kMaxStringN);
    if (!stringsForNum) {
        stringsForNum = new StrVec();
        for (int i = 0; i < kMaxStringN + 1; i++) {
            TempStr s = fmt("%d", i);
            stringsForNum->Append(s);
        }
    }
    return stringsForNum->At(n);
}

template <typename T>
static void InsertRandData(StrVecWithData<T>* v) {
    for (int i = 0; i < kMaxStringN; i++) {
        Str s = StrForN(i);
        T data{};
        data.n = (decltype(data.n))i;
        v->Append(s, data);
        T* d = v->AtData(i);
        utassert(d->n == i);
    }
}

template <typename T>
static void validateStringMatchesData(StrVecWithData<T>* v) {
    int nStrings = len(*v);
    Str got;
    Str exp;
    T* d;
    int n;
    for (int i = 0; i < nStrings; i++) {
        d = v->AtData(i);
        n = (int)d->n;
        got = v->At(i);
        exp = StrForN(n);
        utassert(str::Eq(got, exp));
    }
}

template <typename T>
static void InsertRandData2(StrVecWithData<T>* v) {
    Str got;
    for (int i = 0; i < kMaxStringN; i++) {
        int op = rand() % 12;
        if (op <= 5) {
            T data{};
            data.n = (decltype(data.n))i;
            Str s = StrForN(i);
            int idx = v->Append(s, data);
            ValidateAtStr(v, idx, s);
            T* d = v->AtData(idx);
            utassert(d->n == i);
        } else if (op <= 7) {
            if (!v->IsEmpty()) {
                int idx = randIdx(v);
                Str s = StrForN(idx);
                v->InsertAt(idx, s);
                ValidateAtStr(v, idx, s);
                T* d = v->AtData(idx);
                d->n = (decltype(d->n))idx;
            }
        } else if (op <= 9) {
            if (!v->IsEmpty()) {
                int idx = randIdx(v);
                Str s = StrForN(idx);
                v->SetAt(idx, s);
                ValidateAtStr(v, idx, s);
                T* d = v->AtData(idx);
                d->n = (decltype(d->n))idx;
            }
        } else if (op == 10) {
            if (!v->IsEmpty()) {
                int idx = randIdx(v);
                v->RemoveAt(idx);
            }
        } else {
            if (!v->IsEmpty()) {
                int idx = randIdx(v);
                v->RemoveAtFast(idx);
            }
        }
    }
    ValidateSize(v);
    validateStringMatchesData(v);
}

static void InsertRandData3(StrVec* v) {
    for (int i = 0; i < kMaxStringN; i++) {
        Str s = StrForN(i);
        int op = rand() % 12;
        if (op <= 5) {
            v->Append(s);
        } else if (op <= 7) {
            if (!v->IsEmpty()) {
                int idx = randIdx(v);
                v->InsertAt(idx, s);
            }
        } else if (op <= 9) {
            if (!v->IsEmpty()) {
                int idx = randIdx(v);
                v->SetAt(idx, s);
            }
        } else if (op == 10) {
            if (!v->IsEmpty()) {
                int idx = randIdx(v);
                v->RemoveAt(idx);
            }
        } else {
            if (!v->IsEmpty()) {
                int idx = randIdx(v);
                v->RemoveAtFast(idx);
            }
        }
    }
    ValidateSize(v);
}

template <typename T>
static void RemoveRandData(StrVecWithData<T>* v) {
    int idx;
    while (len(*v) > 0) {
        idx = randIdx(v);
        Str got = v->At(idx);
        T* d = v->AtData(idx);
        int n = (int)d->n;
        Str exp = StrForN(n);
        utassert(str::Eq(got, exp));
        int op = idx % 3;
        int sizeExp = len(*v) - 1;
        if (op == 0) {
            bool ok = v->Remove(got);
            utassert(ok);
        } else if (op == 1) {
            v->RemoveAt(idx);
        } else {
            v->RemoveAtFast(idx);
        }
        utassert(len(*v) == sizeExp);
    }
}

static void StrVecTest8() {
    {
        StrVecWithData<Data1> v;
        InsertRandData2<Data1>(&v);
        RemoveRandData<Data1>(&v);
    }
    {
        StrVecWithData<Data2> v;
        InsertRandData2<Data2>(&v);
        RemoveRandData<Data2>(&v);
    }
    {
        StrVecWithData<Data1> v;
        InsertRandData<Data1>(&v);
        RemoveRandData<Data1>(&v);
    }
    {
        StrVecWithData<Data2> v;
        InsertRandData<Data2>(&v);
        RemoveRandData<Data2>(&v);
    }
    {
        StrVec v;
        InsertRandData3(&v);
        TestRemoveAt(&v);
    }
}

static void StrVecTest9() {
    utassert(!StrLess(Str("abz", 2), Str("abq", 2)));
    utassert(StrLess(Str("abz", 2), Str("abz", 3)));
    utassert(!StrLessNoCase(Str("ABz", 2), Str("abq", 2)));
    utassert(StrLessNoCase(Str("ABz", 2), Str("ABZ", 3)));
}

// Find/FindI with an out-of-range startAt must return -1, not crash
static void StrVecTestFindStartAt() {
    StrVec v;
    utassert(v.Find(StrL("x")) == -1);
    utassert(v.Find(StrL("x"), 1) == -1);
    utassert(v.FindI(StrL("x"), 5) == -1);
    v.Append(StrL("a"));
    v.Append(StrL("b"));
    utassert(v.Find(StrL("b"), 1) == 1);
    utassert(v.Find(StrL("a"), 1) == -1);
    utassert(v.Find(StrL("a"), 2) == -1);
    utassert(v.Find(StrL("a"), 3) == -1);
    utassert(v.FindI(StrL("A"), 17) == -1);
    utassert(v.Find(StrL("a"), -1) == -1);
}

// mutations on a SortIndex()-sorted vec must target the logical element,
// not whatever physically sits at that slot
static void StrVecTestSortedMutation() {
    StrVecWithData<Data1> v; // dataSize != 0 => Sort() uses SortIndex
    const char* strings[] = {"c", "a", "d", "b"};
    for (int i = 0; i < 4; i++) {
        Data1 d{};
        d.n = (u16)i;
        v.Append(Str(strings[i]), d);
    }

    Sort(&v); // logical order: a b c d
    Str removed = v.RemoveAt(0);
    utassert(str::Eq(removed, StrL("a")));
    utassert(len(v) == 3);
    utassert(!v.Contains(StrL("a")));
    utassert(v.Contains(StrL("b")) && v.Contains(StrL("c")) && v.Contains(StrL("d")));

    Sort(&v); // b c d
    bool ok = v.Remove(StrL("c"));
    utassert(ok);
    utassert(!v.Contains(StrL("c")));
    utassert(v.Contains(StrL("b")) && v.Contains(StrL("d")));

    Sort(&v);               // b d
    v.SetAt(1, StrL("zz")); // logical idx 1 is "d"
    utassert(v.Contains(StrL("b")) && v.Contains(StrL("zz")));
    utassert(!v.Contains(StrL("d")));

    Sort(&v); // b zz
    removed = v.RemoveAtFast(0);
    utassert(str::Eq(removed, StrL("b")));
    utassert(len(v) == 1);
    utassert(str::Eq(v.At(0), StrL("zz")));
}

// a copy of a SortIndex()-sorted vec must preserve the logical (sorted) order
static void StrVecTestSortedCopy() {
    StrVecWithData<Data1> v;
    const char* strings[] = {"c", "a", "b"};
    for (int i = 0; i < 3; i++) {
        Data1 d{};
        d.n = (u16)i;
        v.Append(Str(strings[i]), d);
    }
    Sort(&v); // a b c

    StrVecWithData<Data1> v2 = v;
    utassert(len(v2) == 3);
    for (int i = 0; i < 3; i++) {
        strEq(v2.At(i), v.At(i));
        utassert(v2.AtData(i)->n == v.AtData(i)->n);
    }

    StrVecWithData<Data1> v3;
    Data1 d{};
    d.n = 77;
    v3.Append(StrL("x"), d);
    v3 = v;
    utassert(len(v3) == 3);
    for (int i = 0; i < 3; i++) {
        strEq(v3.At(i), v.At(i));
        utassert(v3.AtData(i)->n == v.AtData(i)->n);
    }
}

// Split() into a non-empty vec must behave the same as into a fresh one:
// the "add trailing empty string" rule depends on what this call added,
// not on the pre-existing size of the vec
static void StrVecTestSplitNonEmpty() {
    StrVec v;
    v.Append(StrL("existing"));
    int n = Split(&v, StrL(""), StrL(" "), true);
    utassert(n == 1);
    utassert(len(v) == 2);
    utassert(len(v.At(1)) == 0);

    v.Reset();
    v.Append(StrL("existing"));
    n = Split(&v, StrL(",,"), StrL(","), true);
    utassert(n == 1);
    utassert(len(v) == 2);
    utassert(len(v.At(1)) == 0);
}

// SetAt/InsertAt with a string pointing into the same vec must survive
// the page compaction fallback (which frees the old pages)
static void StrVecTestSetAtSelfRef() {
    char buf[300];
    memset(buf, 'x', sizeof(buf));
    Str big(buf, 299);

    StrVec v;
    v.Append(big); // fills first page almost completely
    v.Append(StrL("small"));
    v.SetAt(1, v.At(0)); // no room => compaction; arg points into freed pages
    Str got = v.At(1);
    utassert(len(got) == 299);
    utassert(memcmp(got.s, buf, 299) == 0);

    v.Reset();
    v.Append(big);
    v.Append(StrL("small"));
    v.InsertAt(1, v.At(0));
    utassert(len(v) == 3);
    got = v.At(1);
    utassert(len(got) == 299);
    utassert(memcmp(got.s, buf, 299) == 0);
}

// sorting must use the stored string lengths, so strings with embedded NUL
// compare by their full content
static void StrVecTestSortEmbeddedNul() {
    StrVec v; // dataSize == 0 => Sort() uses SortNoData
    v.Append(Str("a\0c", 3));
    v.Append(Str("a\0b", 3));
    v.Append(Str("a", 1));
    Sort(&v);
    utassert(len(v.At(0)) == 1);
    Str s = v.At(1);
    utassert(len(s) == 3 && memcmp(s.s, "a\0b", 3) == 0);
    s = v.At(2);
    utassert(len(s) == 3 && memcmp(s.s, "a\0c", 3) == 0);
}

// iterator operator+ must return an advanced copy, not mutate in place
static void StrVecTestIterPlus() {
    StrVec v;
    v.Append(StrL("a"));
    v.Append(StrL("b"));
    v.Append(StrL("c"));
    auto it = v.begin();
    auto it2 = it + 2;
    utassert(it.idx == 0);
    strEq(*it, StrL("a"));
    utassert(it2.idx == 2);
    strEq(*it2, StrL("c"));
}

// appends across many pages, with a compaction in the middle
// (exercises tail-page tracking in Append)
static void StrVecTestManyAppend() {
    StrVec v;
    int n = 3000;
    for (int i = 0; i < n; i++) {
        TempStr s = fmt("s%d", i);
        v.Append(s);
    }
    utassert(len(v) == n);
    ValidateSize(&v);
    strEq(v.At(0), StrL("s0"));
    strEq(v.At(n - 1), StrL("s2999"));
    // force compaction, then append again
    v.SetAt(0, StrL("this is a much longer replacement string than the original was"));
    v.Append(StrL("after-compact"));
    ValidateSize(&v);
    strEq(v.At(len(v) - 1), StrL("after-compact"));
    utassert(v.Find(StrL("s1500")) >= 0);
}

void StrVecTest() {
    StrVecTest9();
    StrVecTest8();
    StrVecTest1();
    StrVecTest2();
    StrVecTest3();
    StrVecTest4();
    StrVecTest5();
    StrVecTest6();
    StrVecTest7();
    StrVecTestFindStartAt();
    StrVecTestSortedMutation();
    StrVecTestSortedCopy();
    StrVecTestSplitNonEmpty();
    StrVecTestSetAtSelfRef();
    StrVecTestSortEmbeddedNul();
    StrVecTestIterPlus();
    StrVecTestManyAppend();
}
