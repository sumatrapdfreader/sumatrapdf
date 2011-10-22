/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#ifndef BencUtil_h
#define BencUtil_h

/* Handling of bencoded format. See:
   http://www.bittorrent.org/protocol.html or 
   http://en.wikipedia.org/wiki/Bencode or
   http://wiki.theory.org/BitTorrentSpecification

   Note: As an exception to the above specifications, this implementation
         only handles zero-terminated strings (i.e. \0 may not appear within
         strings) and considers all encoded strings to be UTF-8 encoded
         (unless added with AddRaw and retrieved with RawValue()).
*/

#include <inttypes.h>
#include "Vec.h"

enum BencType { BT_STRING, BT_INT, BT_ARRAY, BT_DICT };

class BencObj {
    BencType type;

public:
    BencObj(BencType type) : type(type) { }
    virtual ~BencObj() { }
    BencType Type() const { return type; }

    virtual char *Encode() const = 0;
    static BencObj *Decode(const char *bytes, size_t *lenOut=NULL);
};

class BencArray;
class BencDict;

class BencString : public BencObj {
    friend BencArray;
    friend BencDict;

    char *value;

protected:
    BencString(const TCHAR *value);
    BencString(const char *rawValue, size_t len);

public:
    virtual ~BencString() { free(value); }

    TCHAR *Value() const;
    const char *RawValue() const { return value; }

    virtual char *Encode() const;
    static BencString *Decode(const char *bytes, size_t *lenOut);
};

class BencInt : public BencObj {
    int64_t value;

public:
    BencInt(int64_t value) : BencObj(BT_INT), value(value) { }
    int64_t Value() const { return value; }

    virtual char *Encode() const;
    static BencInt *Decode(const char *bytes, size_t *lenOut);
};

class BencArray : public BencObj {
    Vec<BencObj *> value;

public:
    BencArray() : BencObj(BT_ARRAY) { }
    virtual ~BencArray() { DeleteVecMembers(value); }
    size_t Length() const { return value.Count(); }

    void Add(BencObj *obj) {
        assert(obj && value.Find(obj) == -1);
        if (!obj || value.Find(obj) != -1) return;
        value.Append(obj);
    }
    void Add(const TCHAR *string) {
        assert(string);
        if (string)
            Add(new BencString(string));
    }
    void AddRaw(const char *string, size_t len=-1) {
        assert(string);
        if (string)
            Add(new BencString(string, len));
    }
    void Add(int64_t val) {
        Add(new BencInt(val));
    }

    BencString *GetString(size_t index) const {
        if (index < Length() && value.At(index)->Type() == BT_STRING)
            return static_cast<BencString *>(value.At(index));
        return NULL;
    }
    BencInt *GetInt(size_t index) const {
        if (index < Length() && value.At(index)->Type() == BT_INT)
            return static_cast<BencInt *>(value.At(index));
        return NULL;
    }
    BencArray *GetArray(size_t index) const {
        if (index < Length() && value.At(index)->Type() == BT_ARRAY)
            return static_cast<BencArray *>(value.At(index));
        return NULL;
    }
    BencDict *GetDict(size_t index) const;

    virtual char *Encode() const;
    static BencArray *Decode(const char *bytes, size_t *lenOut);
};

class BencDict : public BencObj {
    Vec<char *> keys;
    Vec<BencObj *> values;

    BencObj *GetObj(const char *key) const;

public:
    BencDict() : BencObj(BT_DICT) { }
    virtual ~BencDict() {
        FreeVecMembers(keys);
        DeleteVecMembers(values);
    }
    size_t Length() const { return values.Count(); }

    void Add(const char *key, BencObj *obj);
    void Add(const char *key, const TCHAR *string) {
        assert(string);
        if (string)
            Add(key, new BencString(string));
    }
    void AddRaw(const char *key, const char *string, size_t len=-1) {
        assert(string);
        if (string)
            Add(key, new BencString(string, len));
    }
    void Add(const char *key, int64_t val) {
        Add(key, new BencInt(val));
    }

    BencString *GetString(const char *key) const {
        BencObj *obj = GetObj(key);
        if (obj && obj->Type() == BT_STRING)
            return static_cast<BencString *>(obj);
        return NULL;
    }
    BencInt *GetInt(const char *key) const {
        BencObj *obj = GetObj(key);
        if (obj && obj->Type() == BT_INT)
            return static_cast<BencInt *>(obj);
        return NULL;
    }
    BencArray *GetArray(const char *key) const {
        BencObj *obj = GetObj(key);
        if (obj && obj->Type() == BT_ARRAY)
            return static_cast<BencArray *>(obj);
        return NULL;
    }
    BencDict *GetDict(const char *key) const {
        BencObj *obj = GetObj(key);
        if (obj && obj->Type() == BT_DICT)
            return static_cast<BencDict *>(obj);
        return NULL;
    }

    virtual char *Encode() const;
    static BencDict *Decode(const char *bytes, size_t *lenOut);
};

#endif
