/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef DeserializeBenc_h
#define DeserializeBenc_h

struct SettingInfo;

class BencDict;
struct FieldInfo;

namespace benc {

// Benc doesn't need compact serialization, so allow to use Type_Compact for custom deserialization
typedef bool (* CompactCallback)(BencDict *dict, const FieldInfo *field, const char *fieldName, uint8_t *fieldPtr);

void *Deserialize(const char *data, size_t dataLen, const SettingInfo *meta, CompactCallback cb);

};

#endif
