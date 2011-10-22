/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#include "BaseUtil.h"
#include "BencUtil.h"
#include "StrUtil.h"

BencObj *BencObj::Decode(const char *bytes, size_t *lenOut)
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
    if (result && !lenOut && bytes[len] != '\0') {
        delete result;
        result = NULL;
    }

    if (result && lenOut)
        *lenOut = len;
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

BencString::BencString(const TCHAR *value) : BencObj(BT_STRING)
{
    assert(value);
    this->value = str::conv::ToUtf8(value);
}

BencString::BencString(const char *rawValue, size_t len) : BencObj(BT_STRING)
{
    assert(rawValue);
    if (len == (size_t)-1)
        len = str::Len(rawValue);
    value = str::DupN(rawValue, len);
}

TCHAR *BencString::Value() const
{
    return str::conv::FromUtf8(value);
}

char *BencString::Encode() const
{
    return str::Format("%" PRIuPTR ":%s", str::Len(value), value);
}

BencString *BencString::Decode(const char *bytes, size_t *lenOut)
{
    if (!bytes || !ChrIsDigit(*bytes))
        return NULL;

    int64_t len;
    const char *start = ParseBencInt(bytes, len);
    if (!start || *start != ':' || len < 0)
        return NULL;

    start++;
    if (memchr(start, '\0', (size_t)len))
        return NULL;

    if (lenOut)
        *lenOut = (start - bytes) + (size_t)len;
    return new BencString(start, (size_t)len);
}

char *BencInt::Encode() const
{
    return str::Format("i%" PRId64 "e", value);
}

BencInt *BencInt::Decode(const char *bytes, size_t *lenOut)
{
    if (!bytes || *bytes != 'i')
        return NULL;

    int64_t value;
    const char *end = ParseBencInt(bytes + 1, value);
    if (!end || *end != 'e')
        return NULL;

    if (lenOut)
        *lenOut = (end - bytes) + 1;
    return new BencInt(value);
}

BencDict *BencArray::GetDict(size_t index) const {
    if (index < Length() && value.At(index)->Type() == BT_DICT)
        return static_cast<BencDict *>(value.At(index));
    return NULL;
}

char *BencArray::Encode() const
{
    str::Str<char> bytes(256);
    bytes.Append('l');
    for (size_t i = 0; i < Length(); i++) {
        ScopedMem<char> objBytes(value.At(i)->Encode());
        bytes.Append(objBytes.Get());
    }
    bytes.Append('e');
    return bytes.StealData();
}

BencArray *BencArray::Decode(const char *bytes, size_t *lenOut)
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

    if (lenOut)
        *lenOut = ix + 1;
    return list;
}

static int keycmp(const void *a, const void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}

BencObj *BencDict::GetObj(const char *key) const
{
    char **found = (char **)bsearch(&key, keys.LendData(), keys.Count(), sizeof(key), keycmp);
    if (found)
        return values.At(found - keys.LendData());
    return NULL;
}

/* Per bencoding spec, keys must be ordered alphabetically when serialized,
   so we insert them in sorted order. This might be expensive due to lots of
   memory copying for lots of insertions in random order (more efficient would
   probably be: append at the end and sort when insertions are done or use a
   proper hash table instead of two parallel arrays). */
void BencDict::Add(const char *key, BencObj *obj)
{
    assert(key && obj && values.Find(obj) == -1);
    if (!key || !obj || values.Find(obj) != -1) return;

    // determine the ordered insertion index
    size_t oix = 0;
    if (keys.Count() > 0 && strcmp(keys.Last(), key) < 0)
        oix = keys.Count();
    for (; oix < keys.Count(); oix++)
        if (strcmp(keys.At(oix), key) >= 0)
            break;

    if (oix < keys.Count() && str::Eq(keys.At(oix), key)) {
        // overwrite a previous value
        delete values.At(oix);
        values.At(oix) = obj;
    }
    else {
        keys.InsertAt(oix, str::Dup(key));
        values.InsertAt(oix, obj);
    }
}

char *BencDict::Encode() const
{
    str::Str<char> bytes(256);
    bytes.Append('d');
    for (size_t i = 0; i < Length(); i++) {
        char *key = keys.At(i);
        BencObj *val = values.At(i);
        if (key && val) {
            bytes.AppendFmt("%" PRIuPTR ":%s", str::Len(key), key);
            bytes.AppendAndFree(val->Encode());
        }
    }
    bytes.Append('e');
    return bytes.StealData();
}

BencDict *BencDict::Decode(const char *bytes, size_t *lenOut)
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

    if (lenOut)
        *lenOut = ix + 1;
    return dict;
}
