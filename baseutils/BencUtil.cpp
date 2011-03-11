/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#include "BencUtil.h"
#include "TStrUtil.h"

BencObj *BencObj::Decode(const char *bytes, size_t *len_out)
{
    size_t len;
    BencObj *result = BencString::Decode(bytes, &len);
    if (!result)
        result = BencInt::Decode(bytes, &len);
    if (!result)
        result = BencArray::Decode(bytes, &len);
    if (!result)
        result = BencDict::Decode(bytes, &len);

    // if the caller isn't interested in the amount of bytes
    // processed, verify that we've processed all of them
    if (result && !len_out && bytes[len] != '\0') {
        delete result;
        result = NULL;
    }

    if (result && len_out)
        *len_out = len;
    return result;
}

static const char *ParseBencInt(const char *bytes, int64_t& value)
{
    bool negative = *bytes == '-';
    if (negative)
        bytes++;
    if (!ChrIsDigit(*bytes) || *bytes == '0' && ChrIsDigit(*(bytes + 1)))
        return NULL;

    value = 0;
    for (; ChrIsDigit(*bytes); bytes++) {
        value = value * 10 + (*bytes - '0');
        if (value - (negative ? 1 : 0) < 0)
            return NULL;
    }
    if (negative)
        value *= -1;

    return bytes;
}

BencString::BencString(const char *value) : BencObj(BT_STRING)
{
    this->value = str_to_utf8(value);
}

BencString::BencString(const WCHAR *value) : BencObj(BT_STRING)
{
    this->value = wstr_to_utf8(value);
}

TCHAR *BencString::Value() const
{
    return utf8_to_tstr(value);
}

char *BencString::Encode()
{
    return str_printf("%" PRIuPTR ":%s", StrLen(value), value);
}

BencString *BencString::Decode(const char *bytes, size_t *len_out)
{
    if (!bytes || !ChrIsDigit(*bytes))
        return NULL;

    int64_t len;
    const char *start = ParseBencInt(bytes, len);
    if (!start || *start != ':' || len < 0)
        return NULL;

    start++;
    if (StrLen(start) < len)
        return NULL;

    if (len_out)
        *len_out = (start - bytes) + (size_t)len;
    ScopedMem<char> value(str_dupn(start, (size_t)len));
    return new BencString(value);
}

char *BencInt::Encode()
{
    return str_printf("i%" PRId64 "e", value);
}

BencInt *BencInt::Decode(const char *bytes, size_t *len_out)
{
    if (!bytes || *bytes != 'i')
        return NULL;

    int64_t value;
    const char *end = ParseBencInt(bytes + 1, value);
    if (!end || *end != 'e')
        return NULL;

    if (len_out)
        *len_out = (end - bytes) + 1;
    return new BencInt(value);
}

BencDict *BencArray::GetDict(size_t index) const {
    if (index < Length() && value[index]->Type() == BT_DICT)
        return static_cast<BencDict *>(value[index]);
    return NULL;
}

char *BencArray::Encode()
{
    Vec<char> bytes(256, 1);
    bytes.Append("l", 1);
    for (size_t i = 0; i < Length(); i++) {
        ScopedMem<char> objBytes(value[i]->Encode());
        bytes.Append(objBytes, StrLen(objBytes));
    }
    bytes.Append("e", 1);
    return bytes.StealData();
}

BencArray *BencArray::Decode(const char *bytes, size_t *len_out)
{
    if (!bytes || *bytes != 'l')
        return NULL;

    BencArray *list = new BencArray();
    size_t ix = 1;
    while (bytes[ix] != 'e') {
        size_t len;
        BencObj *obj = BencObj::Decode(bytes + ix, &len);
        if (!obj) {
            delete list;
            return NULL;
        }
        ix += len;
        list->Add(obj);
    }

    if (len_out)
        *len_out = ix + 1;
    return list;
}

char *BencDict::Encode()
{
    Vec<char> bytes(256, 1);
    bytes.Append("d", 1);
    for (size_t i = 0; i < Length(); i++) {
        ScopedMem<char> key(str_printf("%" PRIuPTR ":%s", StrLen(keys[i]), keys[i]));
        bytes.Append(key, StrLen(key));
        ScopedMem<char> objBytes(values[i]->Encode());
        bytes.Append(objBytes, StrLen(objBytes));
    }
    bytes.Append("e", 1);
    return bytes.StealData();
}

BencDict *BencDict::Decode(const char *bytes, size_t *len_out)
{
    if (!bytes || *bytes != 'd')
        return NULL;

    BencDict *dict = new BencDict();
    size_t ix = 1;
    while (bytes[ix] != 'e') {
        size_t len;
        BencString *key = BencString::Decode(bytes + ix, &len);
        if (!key || key->Type() != BT_STRING) {
            delete key;
            delete dict;
            return NULL;
        }
        ix += len;
        BencObj *obj = BencObj::Decode(bytes + ix, &len);
        if (!obj) {
            delete key;
            delete dict;
            return NULL;
        }
        ix += len;
        dict->Add(key->RawValue(), obj);
        delete key;
    }

    if (len_out)
        *len_out = ix + 1;
    return dict;
}

#ifndef NDEBUG
#include "FileUtil.h"

static void assert_serialized(BencObj *obj, const char *dataOrig)
{
    ScopedMem<char> data(obj->Encode());
    assert(data);
    assert(str_eq(data, dataOrig));
}

static struct {
    const char *    benc;
    bool            valid;
    int64_t         value;
} gTestInt[] = {
    { NULL, false },
    { "", false },
    { "a", false },
    { "0", false },
    { "i", false },
    { "ie", false },
    { "i0", false },
    { "i1", false },
    { "i23", false },
    { "i-", false },
    { "i-e", false },
    { "i-0e", false },
    { "i23f", false },
    { "i2-3e", false },
    { "i23-e", false },
    { "i041e", false },
    { "i9223372036854775808e", false },
    { "i-9223372036854775809e", false },

    { "i0e", true, 0 },
    { "i1e", true, 1 },
    { "i9823e", true, 9823 },
    { "i-1e", true, -1 },
    { "i-53e", true, -53 },
    { "i123e", true, 123 },
    { "i2147483647e", true, INT_MAX },
    { "i2147483648e", true, (int64_t)INT_MAX + 1 },
    { "i-2147483648e", true, INT_MIN },
    { "i-2147483649e", true, (int64_t)INT_MIN - 1 },
    { "i9223372036854775807e", true, _I64_MAX },
    { "i-9223372036854775808e", true, _I64_MIN },
};

static void test_parse_int()
{
    for (int i = 0; i < dimof(gTestInt); i++) {
        BencObj *obj = BencObj::Decode(gTestInt[i].benc);
        if (gTestInt[i].valid) {
            assert(obj);
            assert(obj->Type() == BT_INT);
            assert(static_cast<BencInt *>(obj)->Value() == gTestInt[i].value);
            assert_serialized(obj, gTestInt[i].benc);
            delete obj;
        } else {
            assert(!obj);
        }
    }
}

static struct {
    const char *    benc;
    char *          value;
} gTestStr[] = {
    { NULL, NULL },
    { "", NULL },
    { "0", NULL },
    { "1234", NULL },
    { "a", NULL },
    { ":", NULL },
    { ":z", NULL },
    { "1:ab", NULL },
    { "3:ab", NULL },
    { "-2:ab", NULL },
    { "2e:ab", NULL },

    { "0:", "" },
    { "1:a", "a" },
    { "2::a", ":a" },
    { "4:spam", "spam" },
    { "4:i23e", "i23e" },
};

static void test_parse_str()
{
    for (int i = 0; i < dimof(gTestStr); i++) {
        BencObj *obj = BencObj::Decode(gTestStr[i].benc);
        if (gTestStr[i].value) {
            assert(obj);
            assert(obj->Type() == BT_STRING);
            assert(str_eq(static_cast<BencString *>(obj)->RawValue(), gTestStr[i].value));
            assert_serialized(obj, gTestStr[i].benc);
            delete obj;
        } else {
            assert(!obj);
        }
    }
}

static void _test_parse_array_ok(const char *benc, size_t expectedLen)
{
    BencObj *obj = BencObj::Decode(benc);
    assert(obj);
    assert(obj->Type() == BT_ARRAY);
    assert(static_cast<BencArray *>(obj)->Length() == expectedLen);
    assert_serialized(obj, benc);
    delete obj;
}

static void test_parse_array()
{   
    BencObj *obj;

    obj = BencObj::Decode("l");
    assert(!obj);
    obj = BencObj::Decode("l123");
    assert(!obj);
    obj = BencObj::Decode("li12e");
    assert(!obj);
    obj = BencObj::Decode("l2:ie");
    assert(!obj);

    _test_parse_array_ok("le", 0);
    _test_parse_array_ok("li35ee", 1);
    _test_parse_array_ok("llleee", 1);
    _test_parse_array_ok("li35ei-23e2:abe", 3);
    _test_parse_array_ok("li42e2:teldeedee", 4);
}

static void _test_parse_dict_ok(const char *benc, size_t expectedLen)
{
    BencObj *obj = BencObj::Decode(benc);
    assert(obj);
    assert(obj->Type() == BT_DICT);
    assert(static_cast<BencDict *>(obj)->Length() == expectedLen);
    assert_serialized(obj, benc);
    delete obj;
}

static void test_parse_dict()
{   
    BencObj *obj;

    obj = BencObj::Decode("d");
    assert(!obj);
    obj = BencObj::Decode("d123");
    assert(!obj);
    obj = BencObj::Decode("di12e");
    assert(!obj);
    obj = BencObj::Decode("di12e2:ale");
    assert(!obj);

    _test_parse_dict_ok("de", 0);
    _test_parse_dict_ok("d2:hai35ee", 1);
    _test_parse_dict_ok("d3:rum1:a4:borg3:leee", 2);
    _test_parse_dict_ok("d3:keyi35e1:Zi-23e2:ablee", 3);
}

#define ITERATION_COUNT 128

static void test_array_append()
{
    BencArray *array = new BencArray();
    for (size_t i = 1; i <= ITERATION_COUNT; i++) {
        array->Add(i);
        assert(array->Length() == i);
    }
    array->Add(new BencDict());
    for (size_t i = 1; i <= ITERATION_COUNT; i++) {
        BencInt *obj = array->GetInt(i - 1);
        assert(obj && obj->Type() == BT_INT);
        assert(obj->Value() == i);
        assert(!array->GetString(i - 1));
        assert(!array->GetArray(i - 1));
        assert(!array->GetDict(i - 1));
    }
    assert(!array->GetInt(ITERATION_COUNT));
    assert(array->GetDict(ITERATION_COUNT));
    delete array;
}

static void test_dict_insert()
{
    char key[8];

    /* test insertion in ascending order */
    BencDict *dict = new BencDict();
    for (size_t i = 1; i <= ITERATION_COUNT; i++) {
        str_printf_s(key, dimof(key), "%04u", i);
        assert(StrLen(key) == 4);
        dict->Add(key, i);
        assert(dict->Length() == i);
        assert(dict->GetInt(key));
        assert(!dict->GetString(key));
        assert(!dict->GetArray(key));
        assert(!dict->GetDict(key));
    }
    BencInt *intObj = dict->GetInt("0123");
    assert(intObj && intObj->Value() == 123);
    delete dict;

    /* test insertion in descending order */
    dict = new BencDict();
    for (size_t i = ITERATION_COUNT; i > 0; i--) {
        BencObj *obj = new BencInt(i);
        str_printf_s(key, dimof(key), "%04u", i);
        assert(StrLen(key) == 4);
        dict->Add(key, obj);
        assert(dict->Length() == ITERATION_COUNT + 1 - i);
        assert(dict->GetInt(key));
    }
    intObj = dict->GetInt("0123");
    assert(intObj && intObj->Value() == 123);
    delete dict;

    /* TODO: test insertion in random order */
}

void u_benc_all(void)
{
    test_array_append();
    test_dict_insert();
    test_parse_int();
    test_parse_str();
    test_parse_array();
    test_parse_dict();
}

#endif
