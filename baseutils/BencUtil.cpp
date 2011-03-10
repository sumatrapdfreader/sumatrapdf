/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BencUtil.h"
#include "TStrUtil.h"

BencObj *BencObj::Decode(const char *bytes, size_t *len_out, bool topLevel)
{
    size_t len;
    BencObj *result = BencString::Decode(bytes, &len);
    if (!result)
        result = BencInt::Decode(bytes, &len);
    if (!result)
        result = BencArray::Decode(bytes, &len);
    if (!result)
        result = BencDict::Decode(bytes, &len);

    if (result && topLevel && bytes[len] != '\0') {
        delete result;
        result = NULL;
    }

    if (result && len_out)
        *len_out = len;
    return result;
}

BencString::BencString(const char *value) : BencObj(BT_STRING)
{
    this->value = str_to_multibyte(value, CP_UTF8);
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
    return str_printf("%u:%s", StrLen(value), value);
}

BencString *BencString::Decode(const char *bytes, size_t *len_out)
{
    if (!bytes || !isdigit(*bytes))
        return NULL;

    size_t len;
    if (sscanf(bytes, "%ud:", &len) != 1)
        return NULL;
    const char *start = str_find_char(bytes, ':');
    if (!start)
        return NULL;

    start++;
    if (StrLen(start) < len)
        return NULL;

    if (len_out)
        *len_out = (start - bytes) + len;
    ScopedMem<char> value(str_dupn(start, len));
    return new BencString(value);
}

char *BencInt::Encode()
{
    return str_printf("i%I64de", value);
}

BencInt *BencInt::Decode(const char *bytes, size_t *len_out)
{
    if (!bytes || *bytes != 'i')
        return NULL;

    int64_t value;
    if (sscanf(bytes, "i%I64de", &value) != 1)
        return NULL;
    const char *end = str_find_char(bytes, 'e');
    if (!end)
        return NULL;
    if (str_startswith(bytes, "i-0"))
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
        BencObj *obj = BencObj::Decode(bytes + ix, &len, false);
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
        ScopedMem<char> key(str_printf("%d:%s", StrLen(keys[i]), keys[i]));
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
        BencObj *obj = BencObj::Decode(bytes + ix, &len, false);
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

#define SENTINEL_VAL (const char*)-1

struct {
    const char *    txt;
    bool            valid;
    int64_t         value;
} g_int_data[] = {
    { NULL, false, 0 },
    { "", false, 0 },
    { "a", false, 0 },
    { "0", false, 0 },
    { "i", false, 0 },
    { "ie", false, 0 },
    { "i0", false, 0 },
    { "i1", false, 0 },
    { "i23", false, 0 },
    { "i-", false, 0 },
    { "i-e", false, 0 },
    { "i-0e", false, 0 },

    { "i0e", true, 0 },
    { "i1e", true, 1 },
    { "i9823e", true, 9823 },
    { "i-1e", true, -1 },
    { "i-53e", true, -53 },
    { "i123e", true, 123 },
    { SENTINEL_VAL, false, 0 }
};

static void test_parse_int()
{
    for (int i = 0; g_int_data[i].txt != SENTINEL_VAL; i++) {
        const char * txt = g_int_data[i].txt;
        BencObj *obj = BencObj::Decode(txt);
        if (g_int_data[i].valid) {
            assert(obj);
            assert(obj->Type() == BT_INT);
            assert(static_cast<BencInt *>(obj)->Value() == g_int_data[i].value);
            assert_serialized(obj, txt);
            delete obj;
        } else {
            assert(!obj);
        }
    }
}

struct {
    const char *    txt;
    bool            valid;
    char *          value;
} g_str_data[] = {
    { NULL, false, NULL },
    { "", false, NULL },
    { "0", false, NULL },
    { "1234", false, NULL },
    { "a", false, NULL },
    { ":", false, NULL },
    { ":z", false, NULL },
    { "1:ab", false, NULL },
    { "3:ab", false, NULL },

    { "0:", true, "" },
    { "1:a", true, "a" },
    { "4:spam", true, "spam" },
    { SENTINEL_VAL, false, 0 }
};

static void test_parse_str()
{
    for (int i = 0; g_str_data[i].txt != SENTINEL_VAL; i++) {
        const char *txt = g_str_data[i].txt;
        BencObj *obj = BencObj::Decode(txt);
        if (g_str_data[i].valid) {
            assert(obj);
            assert(obj->Type() == BT_STRING);
            assert(str_eq(static_cast<BencString *>(obj)->RawValue(), g_str_data[i].value));
            assert_serialized(obj, txt);
            delete obj;
        } else {
            assert(!obj);
        }
    }
}

static void _test_parse_array_ok(const char *txt, size_t expectedLen)
{
    BencObj *obj = BencObj::Decode(txt);
    assert(obj);
    assert(obj->Type() == BT_ARRAY);
    assert(static_cast<BencArray *>(obj)->Length() == expectedLen);
    assert_serialized(obj, txt);
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

    _test_parse_array_ok("le", 0);
    _test_parse_array_ok("li35ee", 1);
    _test_parse_array_ok("llleee", 1);
    _test_parse_array_ok("li35ei-23e2:abe", 3);
}

static void _test_parse_dict_ok(const char *txt, size_t expectedLen)
{
    BencObj *obj = BencObj::Decode(txt);
    assert(obj);
    assert(obj->Type() == BT_DICT);
    assert(static_cast<BencDict *>(obj)->Length() == expectedLen);
    assert_serialized(obj, txt);
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
        BencObj *obj = new BencInt(i);
        array->Add(obj);
        assert(array->Length() == i);
    }
    for (size_t i = 1; i <= ITERATION_COUNT; i++) {
        BencInt *obj = array->GetInt(i - 1);
        assert(obj->Type() == BT_INT);
        assert(obj->Value() == i);
    }
    delete array;
}

static void test_dict_insert()
{
    char key[8];

    /* test insertion in ascending order */
    BencDict *dict = new BencDict();
    for (size_t i = 1; i <= ITERATION_COUNT; i++) {
        BencObj *obj = new BencInt(i);
        str_printf_s(key, dimof(key), "%04u", i);
        assert(StrLen(key) == 4);
        dict->Add(key, obj);
        assert(dict->Length() == i);
        assert(dict->GetInt(key));
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
