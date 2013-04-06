/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "DeserializeBenc.h"

#define INCLUDE_APPPREFS3_STRUCTS
#include "AppPrefs3.h"
#include "BencUtil.h"

namespace benc {

static void *DeserializeRec(BencDict *dict, const SettingInfo *meta, uint8_t *base, CompactCallback cb)
{
    if (!base)
        base = AllocArray<uint8_t>(meta->structSize);

    const char *fieldName = meta->fieldNames;
    for (size_t i = 0; i < meta->fieldCount; i++) {
        const FieldInfo& field = meta->fields[i];
        uint8_t *fieldPtr = base + field.offset;
        if (Type_Struct == field.type) {
            BencDict *child = dict ? dict->GetDict(fieldName) : NULL;
            DeserializeRec(child, GetSubstruct(field), fieldPtr, cb);
        }
        else if (Type_Array == field.type) {
            Vec<void *> *array = new Vec<void *>();
            BencArray *list = dict ? dict->GetArray(fieldName) : NULL;
            for (size_t j = 0; list && j < list->Length(); j++) {
                array->Append(DeserializeRec(list->GetDict(j), GetSubstruct(field), NULL, cb));
            }
            *(Vec<void *> **)fieldPtr = array;
        }
        else if (Type_Bool == field.type) {
            BencInt *val = dict ? dict->GetInt(fieldName) : NULL;
            *(bool *)fieldPtr = (val ? val->Value() : field.value) != 0;
        }
        else if (Type_Int == field.type) {
            BencInt *val = dict ? dict->GetInt(fieldName) : NULL;
            *(int *)fieldPtr = (int)(val ? val->Value() : field.value);
        }
        else if (Type_Float == field.type) {
            BencString *val = dict ? dict->GetString(fieldName) : NULL;
            if (!val || !str::Parse(val->RawValue(), "%f", (float *)fieldPtr))
                str::Parse((const char *)field.value, "%f", (float *)fieldPtr);
        }
        else if (Type_Color == field.type) {
            BencInt *val = dict ? dict->GetInt(fieldName) : NULL;
            *(COLORREF *)fieldPtr = (COLORREF)(val ? val->Value() : field.value);
        }
        else if (Type_String == field.type) {
            BencString *val = dict ? dict->GetString(fieldName) : NULL;
            *(WCHAR **)fieldPtr = val ? val->Value() : str::Dup((const WCHAR *)field.value);
        }
        else if (Type_Utf8String == field.type) {
            BencString *val = dict ? dict->GetString(fieldName) : NULL;
            *(char **)fieldPtr = str::Dup(val ? val->RawValue() : (const char *)field.value);
        }
        else if (Type_IntArray == field.type) {
            CrashIf(field.value);
            Vec<int> *vec = new Vec<int>();
            BencArray *val = dict ? dict->GetArray(fieldName) : NULL;
            for (size_t j = 0; val && j < val->Length(); j++) {
                BencInt *val2 = val->GetInt(j);
                vec->Append(val2 ? val2->Value() : -1);
            }
            *(Vec<int> **)fieldPtr = vec;
        }
        else if (Type_Compact == field.type) {
            bool ok = cb && cb(dict, &field, fieldName, fieldPtr);
            CrashIf(!ok);
        }
        else {
            CrashIf(true);
        }
        fieldName += str::Len(fieldName) + 1;
    }
    return base;
}

void *Deserialize(const char *data, size_t dataLen, const SettingInfo *def, CompactCallback cb)
{
    CrashIf(str::Len(data) != dataLen);
    void *result = NULL;
    BencObj *root = BencObj::Decode(data);
    if (root && BT_DICT == root->Type())
        result = DeserializeRec(static_cast<BencDict *>(root), def, NULL, cb);
    delete root;
    return result;
}

};
