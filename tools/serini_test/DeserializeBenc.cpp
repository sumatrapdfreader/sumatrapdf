/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "DeserializeBenc.h"

#include "AppPrefs3.h"
#include "BencUtil.h"

namespace benc {

static void *DeserializeRec(BencDict *dict, const SettingInfo *meta, uint8_t *base=NULL)
{
    if (!base)
        base = AllocArray<uint8_t>(meta->structSize);

    for (size_t i = 0; i < meta->fieldCount; i++) {
        const FieldInfo& field = meta->fields[i];
        const char *fieldName = GetFieldName(meta, i);
        uint8_t *fieldPtr = base + field.offset;
        if (Type_Struct == field.type) {
            BencDict *child = dict ? dict->GetDict(fieldName) : NULL;
            DeserializeRec(child, GetSubstruct(field), fieldPtr);
        }
        else if (Type_Array == field.type) {
            Vec<void *> *array = new Vec<void *>();
            BencArray *list = dict ? dict->GetArray(fieldName) : NULL;
            for (size_t j = 0; list && j < list->Length(); j++) {
                array->Append(DeserializeRec(list->GetDict(j), GetSubstruct(field)));
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
        else if (field.type != Type_Custom) {
            CrashIf(true);
        }
        // the following could be achieved with a callback for custom fields
        else if (str::Eq(fieldName, "LastUpdate")) {
            BencString *val = dict ? dict->GetString(fieldName) : NULL;
            if (!val || !_HexToMem(val->RawValue(), (FILETIME *)fieldPtr))
                ZeroMemory(fieldPtr, sizeof(FILETIME));
        }
        else if (str::Eq(fieldName, "TocToggles")) {
            BencArray *val = dict ? dict->GetArray(fieldName) : NULL;
            ScopedMem<char> values;
            if (val) {
                for (size_t j = 0; j < val->Length(); j++) {
                    BencInt *val2 = val->GetInt(j);
                    if (!values)
                        values.Set(str::Format("%d", val2 ? val2->Value() : -1));
                    else
                        values.Set(str::Format("%s %d", values, val2 ? val2->Value() : -1));
                }
            }
            *(char **)fieldPtr = values.StealData();
        }
        else if (str::Eq(fieldName, "Favorites")) {
            BencArray *favDict = dict ? dict->GetArray(fieldName) : NULL;
            Vec<File *> *files = *(Vec<File *> **)fieldPtr;
            for (size_t j = 0; j < files->Count(); j++) {
                File *file = files->At(j);
                file->favorite = new Vec<Favorite *>();
                BencArray *favList = NULL;
                for (size_t k = 0; k < favDict->Length() && !favList; k += 2) {
                    BencString *path = favDict->GetString(k);
                    ScopedMem<WCHAR> filePath(path ? path->Value() : NULL);
                    if (str::Eq(filePath, file->filePath))
                        favList = favDict->GetArray(k + 1);
                }
                if (!favList)
                    continue;
                for (size_t k = 0; k < favList->Length(); k += 2) {
                    BencInt *page = favList->GetInt(k);
                    BencString *name = favList->GetString(k + 1);
                    Favorite *fav = AllocStruct<Favorite>();
                    fav->pageNo = page ? page->Value() : -1;
                    fav->name = name ? name->Value() : NULL;
                    file->favorite->Append(fav);
                }
            }
        }
        else {
            CrashIf(true);
        }
    }
    return base;
}

void *Deserialize(const char *data, size_t dataLen, const SettingInfo *def)
{
    CrashIf(str::Len(data) != dataLen);
    void *result = NULL;
    BencObj *root = BencObj::Decode(data);
    if (root && BT_DICT == root->Type())
        result = DeserializeRec(static_cast<BencDict *>(root), def);
    delete root;
    return result;
}

};
