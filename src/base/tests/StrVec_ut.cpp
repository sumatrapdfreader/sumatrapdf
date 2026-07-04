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

static Str strs[] = {"foo", "bar", "Blast", {}, "this is a large string, my friend"};
// order in strs
static int unsortedOrder[] = {0, 1, 2, 3, 4};
static int sortedOrder[]{3, 2, 1, 0, 4};
static int sortedNoCaseOrder[]{3, 1, 2, 0, 4};

static void StrVecTest1_1(StrVec* v) {
    Str s = "lolda";
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
    v->SetAt(3, nullptr);
    utassert(!v->At(3));
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
    v->Append("foo");
    v->Append("bar");
    Str s = Join(v);
    utassert(len(*v) == 2);
    utassert(str::Eq("foobar", s));

    s = Join(v, ";");
    utassert(len(*v) == 2);
    utassert(str::Eq("foo;bar", s));

    v->Append(nullptr);
    utassert(len(*v) == 3);

    v->Append("glee");
    s = JoinTemp(v, "_ _");
    utassert(len(*v) == 4);
    utassert(str::Eq("foo_ _bar_ _glee", s));

    StrVecCheckIter(v, nullptr);
}

static void StrVecTest2_2(StrVec* v) {
    Sort(v);
    Str strsSorted[] = {{}, "bar", "foo", "glee"};
    StrVecCheckIter(v, strsSorted);

    auto s = Join(v, "++");
    utassert(len(*v) == 4);
    utassert(str::Eq("bar++foo++glee", s));

    s = Join(v);
    utassert(str::Eq("barfooglee", s));
}

static void StrVecTest2_3(StrVec* v2) {
    int n = Split(v2, "a,b,,c,", ",");
    utassert(n == 5 && v2->Find("c") == 3);
    utassert(v2->Find("") == 2);
    utassert(v2->Find("", 3) == 4);
    utassert(v2->Find("", 5) == -1);
    utassert(v2->Find("B") == -1 && v2->FindI("B") == 1);
    TempStr joined = JoinTemp(v2, ";");
    utassert(str::Eq(joined, "a;b;;c;"));
    TestRemoveAt(v2);
}

static void StrVecTest2_4(StrVec* v2) {
    int n = Split(v2, "a,b,,c,", ",", true);
    utassert(n == 3 && v2->Find("c") == 2);
    TempStr joined = JoinTemp(v2, ";");
    utassert(str::Eq(joined, "a;b;c"));
    StrVecCheckIter(v2, nullptr);

    TestRemoveAt(v2);
}

static void StrVecTest2_5(StrVec* v2) {
    int n = Split(v2, "a,b,,c,d", ",", true, 3);
    Str s = JoinTemp(v2, "__");
    utassert(n == 3);
    utassert(str::Eq(s, "a__b__c,d"));

    v2->Reset();
    n = Split(v2, "a,b,,c,d", ",", false, 3);
    s = JoinTemp(v2, "__");
    utassert(n == 3);
    // TODO: fix me
    utassert(str::Eq(s, "a__b__,c,d"));

    v2->Reset();
    n = Split(v2, "a,b,,c,d", ",", true, 1);
    utassert(n == 1);
    s = v2->At(0);
    utassert(str::Eq(s, "a,b,,c,d"));

    // max 0 is turned into 1
    v2->Reset();
    n = Split(v2, "a,b,,c,d", ",", true, 0);
    s = v2->At(0);
    utassert(str::Eq(s, "a,b,,c,d"));
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
        utassert(str::Eq(v2[2], "foo"));
        v2.Append("nobar");
        utassert(str::Eq(v2[4], "nobar"));
        v2 = v;
        utassert(len(v2) == 4);
        // copies should be same values but at different addresses
        utassert(v2[1].s != v[1].s);
        utassert(str::Eq(v2[1], v[1]));
        s = v2[2];
        utassert(str::Eq(s, "foo"));
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
    v->Append("one");
    v->Append("two");
    v->Append("One");
    utassert(len(*v) == 3);
    utassert(str::Eq(v->At(0), "one"));
    utassert(str::EqI(v->At(2), "one"));
    utassert(v->Find("One") == 2);
    utassert(v->FindI("One") == 0);
    utassert(v->Find("Two") == -1);
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
    auto s = "new value of string, should be large to get results faster";
    // StrVec: tests adding where can allocate new value inside a page
    v->SetAt(idx, s);
    utassert(str::Eq(s, v->At(idx)));
    v->SetAt(idx, nullptr);
    utassert(!v->At(idx));
    v->SetAt(idx, "");
    utassert(str::Eq("", v->At(idx)));
    // StrVec: force allocating in side strings
    // first page is 256 bytes so this should force allocation in sideStrings
    int n = 256 / len(s);
    for (int i = 0; i < n; i++) {
        v->SetAt(idx, s);
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
        v->SetAt(i, s);
    }
    for (auto it = v->begin(); it != v->end(); it++) {
        s2 = *it;
        utassert(str::Eq(s, s2));
    }
    s3 = "hello";
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
    Str s = "first";
    v->InsertAt(0, s);
    auto s2 = v->At(0);
    utassert(str::Eq(s, s2));
    s = strs[0];
    s2 = v->At(1);
    utassert(str::Eq(s2, s));
    s = "middle";
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
    Split(v, " CmdCreateAnnotHighlight   #00ff00 openEdit", " ", true, 2);
    utassert(len(*v) == 2);
    Str s = v->At(0);
    utassert(str::Eq(s, "CmdCreateAnnotHighlight"));
    s = v->At(1);
    utassert(str::Eq(s, "#00ff00 openEdit"));
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
    Split(v, "", " ", true, 2);
    utassert(len(*v) == 1);
    Str s = v->At(0);
    utassert(!s || s.s[0] == 0);
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
        utassert(str::Eq(got.s, exp));
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

#if 0
static void CheckSortOrder(StrVec* v, StrLessFunc lessFn = nullptr) {
    int n = v->Size();
    if (n < 2) {
        return;
    }
    if (lessFn == nullptr) {
        lessFn = StrLess;
    }
    for (int i = 1; i < n; i++) {
        const char* prev = v->At(i - 1);
        const char* cur = v->At(i);
        utassert(lessFn(prev, cur) == true);
    }
}
#endif

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
}
