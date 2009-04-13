/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#include "base_util.h"
#include "file_util.h"
#include "benc_util.h"
#include "str_util.h"

static void test_parse_torrent(const char *fileName)
{
    uint64_t fileSize;
    size_t  lenOut;
    char *data = file_read_all(fileName, &fileSize);
    assert(data);
    if (!data)
        goto Exit;

    benc_obj * bobj = benc_obj_from_data(data, (size_t)fileSize);
    assert(bobj);
    if (!bobj)
        goto Exit;

    char *data2 = benc_obj_to_data(bobj, &lenOut);
    assert(lenOut == (size_t)fileSize);
    assert(0 == memcmp(data, data2, lenOut));
    benc_obj_delete(bobj);
    free(data2);
Exit:
    free(data);
}

static benc_obj* benc_obj_from_txt(const char *txt)
{
    int txtLen = 0;
    if (txt)
        txtLen = strlen(txt);
    return benc_obj_from_data(txt, txtLen);
}

static void assert_serialized(benc_obj *bobj, const char *dataOrig, size_t lenOrig)
{
    size_t serializedLen;
    char * serializedData = benc_obj_to_data(bobj, &serializedLen);
    assert(serializedData);
    assert(lenOrig == serializedLen);
    assert(0 == memcmp(dataOrig, serializedData, lenOrig));
    free(serializedData);
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
    for (int i=0; g_int_data[i].txt != SENTINEL_VAL; i++) {
        const char * txt = g_int_data[i].txt;
        bool valid = g_int_data[i].valid;
        benc_obj * bobj = benc_obj_from_txt(txt);
        if (valid) {
            assert(bobj);
            assert(BOT_INT64 == bobj->m_type);
            benc_int64 * bobjInt = (benc_int64*)bobj;
            int64_t expectedVal = g_int_data[i].value;
            assert(bobjInt->m_val == expectedVal);
            assert_serialized(bobj, txt, strlen(txt));
            benc_obj_delete(bobj);
        } else {
            assert(!bobj);
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
    for (int i=0; g_str_data[i].txt != SENTINEL_VAL; i++) {
        const char *txt = g_str_data[i].txt;
        size_t txtLen = 0;
        if (txt)
            txtLen = strlen(txt);
        bool valid = g_str_data[i].valid;
        benc_obj *bobj = benc_obj_from_data(txt, txtLen);
        if (valid) {
            assert(bobj);
            assert(BOT_STRING== bobj->m_type);
            benc_str *bobjStr = (benc_str*)bobj;
            char *expectedValTxt = g_str_data[i].value;
            assert(str_eq(bobjStr->m_str, expectedValTxt));
            assert_serialized(bobj, txt, txtLen);
            benc_obj_delete(bobj);
        } else {
            assert(!bobj);
        }
    }
}

static void _test_parse_array_ok(const char *txt, int expectedLen)
{
    int realLen;
    benc_obj * bobj = benc_obj_from_txt(txt);
    assert(bobj);
    assert(BOT_ARRAY == bobj->m_type);
    realLen = benc_obj_len(bobj);
    assert(realLen == expectedLen);
    assert_serialized(bobj, txt, strlen(txt));
    benc_obj_delete(bobj);
}

static void test_parse_array()
{   
    benc_obj *      bobj;

    bobj = benc_obj_from_txt("l");
    assert(!bobj);
    bobj = benc_obj_from_txt("l123");
    assert(!bobj);
    bobj = benc_obj_from_txt("li12e");
    assert(!bobj);

    _test_parse_array_ok("le", 0);
    _test_parse_array_ok("li35ee", 1);
    _test_parse_array_ok("llleee", 1);
    _test_parse_array_ok("li35ei-23e2:abe", 3);
}

static void _test_parse_dict_ok(const char *txt, int expectedLen)
{
    int realLen;
    benc_obj * bobj = benc_obj_from_txt(txt);
    assert(bobj);
    assert(BOT_DICT == bobj->m_type);
    realLen = benc_obj_len(bobj);
    assert(realLen == expectedLen);
    assert_serialized(bobj, txt, strlen(txt));
    benc_obj_delete(bobj);
}

static void test_parse_dict()
{   
    benc_obj *      bobj;

    bobj = benc_obj_from_txt("d");
    assert(!bobj);
    bobj = benc_obj_from_txt("d123");
    assert(!bobj);
    bobj = benc_obj_from_txt("di12e");
    assert(!bobj);
    bobj = benc_obj_from_txt("di12e2:ale");
    assert(!bobj);

    _test_parse_dict_ok("de", 0);
    _test_parse_dict_ok("d2:hai35ee", 1);
    _test_parse_dict_ok("d3:rum1:a4:borg3:leee", 2);
    _test_parse_dict_ok("d3:keyi35e1:Zi-23e2:ablee", 3);
}

static void test_array_append()
{
    int i;
    int arrLen;
    benc_array* arr = benc_array_new();
    for (i = 1; i <= 4098; i++) {
        benc_obj* obj = (benc_obj*)benc_int64_new((int64_t)i);
        benc_array_append(arr, obj);
        arrLen = benc_array_len(arr);
        assert(i == arrLen);
    }
    for (i = 1; i <= 4098; i++) {
        benc_obj* obj = benc_array_get(arr, i-1);
        benc_int64* objInt = (benc_int64*)obj;
        assert(BOT_INT64 == obj->m_type);
        assert((int64_t)i == objInt->m_val);
    }
    benc_obj_delete((benc_obj*)arr);
}

static void test_dict_insert()
{
    char        key[256];
    size_t      dictLen, keyLen, i;
    BOOL        ok;
    benc_obj *  found;
    benc_dict * dict;

    /* test insertion in ascending order */
    dict = benc_dict_new();
    for (i = 1; i <= 4098; i++) {
        benc_obj* obj = (benc_obj*)benc_int64_new((int64_t)i);
        int64_to_string_zero_pad(i, 4, key, dimof(key));
        keyLen = strlen(key);
        assert(4 == keyLen);
        ok = benc_dict_insert(dict, (const char*)key, keyLen, obj);
        assert(ok);
        dictLen = benc_dict_len(dict);
        assert(i == dictLen);
        found = benc_dict_find(dict, key, keyLen);
        assert(found == obj);
    }
    benc_obj_delete((benc_obj*)dict);

    /* test insertion in descending order */
    dict = benc_dict_new();
    for (i = 4098; i > 0; i--) {
        benc_obj* obj = (benc_obj*)benc_int64_new((int64_t)i);
        int64_to_string_zero_pad(i, 4, key, dimof(key));
        keyLen = strlen(key);
        assert(4 == keyLen);
        ok = benc_dict_insert(dict, (const char*)key, keyLen, obj);
        assert(ok);
        dictLen = benc_dict_len(dict);
        assert(4099 - i == dictLen);
        found = benc_dict_find(dict, key, keyLen);
        assert(found == obj);
    }
    benc_obj_delete((benc_obj*)dict);

    /* TODO: test insertion in random order */
}

int main(int argc, char **argv)
{
    test_array_append();
    test_dict_insert();
    test_parse_int();
    test_parse_str();
    test_parse_array();
    test_parse_dict();
    test_parse_torrent("torrent1.torrent");
    test_parse_torrent("torrent2.torrent");
    test_parse_torrent("torrent3.torrent");
    test_parse_torrent("torrent4.torrent");
    test_parse_torrent("torrent5.torrent");
}

