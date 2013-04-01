/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "VarintGob.h"

// if set to 1, dumps on to the debugger code that can be copied
// to util.py (test_gob()), to verify C and python generate
// the same encoded data
#define GEN_PYTHON_TESTS 0

static void GenPythonIntTest(int64_t val, uint8_t *d, int dLen)
{
#if GEN_PYTHON_TESTS == 1
    str::Str<char> s;
    s.AppendFmt("  assert gob_varint_encode(%I64d) == ", val);
    int n;
    for (int i = 0; i < dLen; i++) {
        n = (int)d[i];
        s.AppendFmt("chr(%d)", n);
        if (i < dLen - 1)
            s.Append(" + ");
    }
    plogf("%s", s.Get());
#else
    (void)val;
    (void)d;
    (void)dLen;
#endif
}

static void GenPythonUIntTest(uint64_t val, uint8_t *d, int dLen)
{
#if GEN_PYTHON_TESTS == 1
    str::Str<char> s;
    s.AppendFmt("  assert gob_uvarint_encode(%I64u) == ", val);
    int n;
    for (int i = 0; i < dLen; i++) {
        n = (int)d[i];
        s.AppendFmt("chr(%d)", n);
        if (i < dLen - 1)
            s.Append(" + ");
    }
    plogf("%s", s.Get());
#else
    (void)val;
    (void)d;
    (void)dLen;
#endif
}

static void GobEncodingTest()
{
    uint8_t buf[2048];
    int64_t intVals[] = {
        0, 1, 0x7f, 0x80, 0x81, 0xfe, 0xff, 0x100, 0x1234, 0x12345, 0x123456,
        0x1234567, 0x12345678, 0x7fffffff, -1, -2, -255, -256, -257, -0x1234,
        -0x12345, -0x123456, -0x124567, -0x1245678
    };
    uint64_t uintVals[] = {
        0, 1, 0x7f, 0x80, 0x81, 0xfe, 0xff, 0x100, 0x1234, 0x12345, 0x123456,
        0x1234567, 0x12345678, 0x7fffffff, 0x80000000, 0x80000001, 0xfffffffe,
        0xffffffff
    };
    int n, dLen, n2;
    uint8_t *d;
    int64_t val, expVal;
    uint64_t uval, expUval;

    d = buf; dLen = dimof(buf);
    for (int i = 0; i < dimof(intVals); i++) {
        val = intVals[i];
        n = GobVarintEncode(val, d, dLen);
        assert(n >= 1);
        GenPythonIntTest(val, d, n);
        n2 = VarintGobDecode(d, n, &expVal);
        assert(n == n2);
        assert(val == expVal);
        d += n;
        dLen -= n;
        assert(dLen > 0);
    }
    dLen = (d - buf);
    d = buf;
    for (int i = 0; i < dimof(intVals); i++) {
        expVal = intVals[i];
        n = VarintGobDecode(d, dLen, &val);
        assert(0 != n);
        assert(val == expVal);
        d += n;
        dLen -= n;
    }
    assert(0 == dLen);

    d = buf; dLen = dimof(buf);
    for (int i = 0; i < dimof(uintVals); i++) {
        uval = uintVals[i];
        n = GobUVarintEncode(uval, d, dLen);
        assert(n >= 1);
        GenPythonUIntTest(uval, d, n);
        n2 = UVarintGobDecode(d, n, &expUval);
        assert(n == n2);
        assert(uval == expUval);
        d += n;
        dLen -= n;
        assert(dLen > 0);
    }
    dLen = (d - buf);
    d = buf;
    for (int i = 0; i < dimof(uintVals); i++) {
        expUval = uintVals[i];
        n = UVarintGobDecode(d, dLen, &uval);
        assert(0 != n);
        assert(uval == expUval);
        d += n;
        dLen -= n;
    }
    assert(0 == dLen);
}

static void VarintGobTest()
{
    GobEncodingTest();
}
