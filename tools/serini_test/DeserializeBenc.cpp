/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "DeserializeBenc.h"

#include "AppPrefs3.h"
#include "BencUtil.h"

namespace benc {

static void *DeserializeRec(BencDict *dict, SettingInfo *meta, uint8_t *base=NULL)
{
    if (!base)
        base = AllocArray<uint8_t>(GetStructSize(meta));

    for (size_t i = 1; i <= GetFieldCount(meta); i++) {
        uint8_t *fieldPtr = base + meta[i].offset;
        if (Type_Struct == meta[i].type) {
            BencDict *child = dict ? dict->GetDict(GetFieldName(meta, i)) : NULL;
            DeserializeRec(child, GetSubstruct(meta[i]), fieldPtr);
        }
        else if (Type_Array == meta[i].type) {
            Vec<void *> *array = new Vec<void *>();
            BencArray *list = dict ? dict->GetArray(GetFieldName(meta, i)) : NULL;
            for (size_t j = 0; list && j < list->Length(); j++) {
                array->Append(DeserializeRec(list->GetDict(j), GetSubstruct(meta[i])));
            }
            *(Vec<void *> **)fieldPtr = array;
        }
        else if (Type_Bool == meta[i].type) {
            BencInt *val = dict ? dict->GetInt(GetFieldName(meta, i)) : NULL;
            *(bool *)fieldPtr = (val ? val->Value() : meta[i].value) != 0;
        }
        else if (Type_Int == meta[i].type) {
            BencInt *val = dict ? dict->GetInt(GetFieldName(meta, i)) : NULL;
            *(int *)fieldPtr = (int)(val ? val->Value() : meta[i].value);
        }
        else if (Type_Float == meta[i].type) {
            BencString *val = dict ? dict->GetString(GetFieldName(meta, i)) : NULL;
            if (!val || !str::Parse(val->RawValue(), "%f", (float *)fieldPtr))
                str::Parse((const char *)meta[i].value, "%f", (float *)fieldPtr);
        }
        else if (Type_Color == meta[i].type) {
            BencInt *val = dict ? dict->GetInt(GetFieldName(meta, i)) : NULL;
            *(COLORREF *)fieldPtr = (COLORREF)(val ? val->Value() : meta[i].value);
        }
        else if (Type_String == meta[i].type) {
            BencString *val = dict ? dict->GetString(GetFieldName(meta, i)) : NULL;
            *(WCHAR **)fieldPtr = val ? val->Value() : str::Dup((const WCHAR *)meta[i].value);
        }
        else if (Type_Utf8String == meta[i].type) {
            BencString *val = dict ? dict->GetString(GetFieldName(meta, i)) : NULL;
            *(char **)fieldPtr = str::Dup(val ? val->RawValue() : (const char *)meta[i].value);
        }
        else if (meta[i].type != Type_Meta) {
            CrashIf(true);
        }
        else if (str::Eq(GetFieldName(meta, i), "LastUpdate")) {
            BencString *val = dict ? dict->GetString(GetFieldName(meta, i)) : NULL;
            if (!val || !_HexToMem(val->RawValue(), (FILETIME *)fieldPtr))
                ZeroMemory(fieldPtr, sizeof(FILETIME));
        }
        else if (str::Eq(GetFieldName(meta, i), "TocToggles")) {
            BencArray *val = dict ? dict->GetArray(GetFieldName(meta, i)) : NULL;
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
        else if (str::Eq(GetFieldName(meta, i), "Favorites")) {
            BencArray *favDict = dict ? dict->GetArray(GetFieldName(meta, i)) : NULL;
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

void *Deserialize(const char *data, size_t dataLen, SettingInfo *def)
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
