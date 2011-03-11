/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#ifndef BencUtil_h
#define BencUtil_h

/* Handling of bencoded format. See:
   http://www.bittorrent.org/protocol.html or 
   http://en.wikipedia.org/wiki/Bencode or
   http://wiki.theory.org/BitTorrentSpecification

   Note: As an exception to the above specifications, this implementation
         only handles zero-terminated strings (i.e. \0 may not appear within
         strings) and considers all encoded strings to be UTF-8 encoded.
*/

#include <inttypes.h>
#include "Vec.h"

typedef enum { BT_STRING, BT_INT, BT_ARRAY, BT_DICT } BencType;

class BencObj {
    BencType type;

public:
    BencObj(BencType type) : type(type) { }
    virtual ~BencObj() { }
    BencType Type() const { return type; }

    virtual char *Encode() = 0;
    static BencObj *Decode(const char *bytes, size_t *len_out=NULL);
};

class BencString : public BencObj {
    char *value;

public:
    BencString(const char *value);
    BencString(const WCHAR *value);
    ~BencString() { free(value); }

    TCHAR *Value() const;
    char *RawValue() const { return value; }

    virtual char *Encode();
    static BencString *Decode(const char *bytes, size_t *len_out);
};

class BencInt : public BencObj {
    int64_t value;

public:
    BencInt(int64_t value) : BencObj(BT_INT), value(value) { }
    int64_t Value() const { return value; }

    virtual char *Encode();
    static BencInt *Decode(const char *bytes, size_t *len_out);
};

class BencDict;

class BencArray : public BencObj {
    Vec<BencObj *> value;

public:
    BencArray() : BencObj(BT_ARRAY) { }
    ~BencArray() { DeleteVecMembers(value); }
    size_t Length() const { return value.Count(); }

    void Add(BencObj *obj) {
        assert(obj && value.Find(obj) == -1);
        value.Append(obj);
    }
    void Add(const char *string) {
        Add(new BencString(string));
    }
    void Add(const WCHAR *string) {
        Add(new BencString(string));
    }
    void Add(int64_t val) {
        Add(new BencInt(val));
    }

    BencString *GetString(size_t index) const {
        if (index < Length() && value[index]->Type() == BT_STRING)
            return static_cast<BencString *>(value[index]);
        return NULL;
    }
    BencInt *GetInt(size_t index) const {
        if (index < Length() && value[index]->Type() == BT_INT)
            return static_cast<BencInt *>(value[index]);
        return NULL;
    }
    BencArray *GetArray(size_t index) const {
        if (index < Length() && value[index]->Type() == BT_ARRAY)
            return static_cast<BencArray *>(value[index]);
        return NULL;
    }
    BencDict *GetDict(size_t index) const;

    virtual char *Encode();
    static BencArray *Decode(const char *bytes, size_t *len_out);
};

class BencDict : public BencObj {
    Vec<char *> keys;
    Vec<BencObj *> values;

    BencObj *GetObj(const char *key) const {
        // TODO: sort keys at insertion and do a binary search?
        //       (would break inversibility of encoding/decoding)
        for (size_t i = 0; i < Length(); i++)
            if (!strcmp(key, keys[i]))
                return values[i];
        return NULL;
    }

public:
    BencDict() : BencObj(BT_DICT) { }
    ~BencDict() {
        FreeVecMembers(keys);
        DeleteVecMembers(values);
    }
    size_t Length() const { return values.Count(); }

    void Add(const char *key, BencObj *obj) {
        assert(key && obj && !GetObj(key) && values.Find(obj) == -1);
        keys.Append(strdup(key));
        values.Append(obj);
    }
    void Add(const char *key, const char *string) {
        Add(key, new BencString(string));
    }
    void Add(const char *key, const WCHAR *string) {
        Add(key, new BencString(string));
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

    virtual char *Encode();
    static BencDict *Decode(const char *bytes, size_t *len_out);
};

#endif
