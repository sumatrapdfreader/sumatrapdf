/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#define INCLUDE_APPPREFS3_METADATA
#include "AppPrefs3.h"
#include "BencUtil.h"
#include "FileUtil.h"
#include "DeserializeBenc.h"
#include "SerializeSqt.h"
#include "SerializeTxt2.h"
#include "../sertxt_test/SettingsTxtSumatra.h"

#define Check(x) CrashIf(!(x)); if (!(x)) return false; else NoOp()

// allow to easily disable tests for binary size comparison
#pragma warning(disable: 4505)

// structs for recursion and compactness tests

struct Rec {
    Vec<Rec *> * rec;
    UserPrefs up;
};

static FieldInfo gRecFields[] = {
    { offsetof(Rec, rec), Type_Array,  -1 /* set to (intptr_t)gRecInfo before usage */ },
    { offsetof(Rec, up),  Type_Struct, (intptr_t)&gUserPrefsInfo },
};
static SettingInfo gRecInfo = { sizeof(Rec), 2, gRecFields, "Rec\0Up" };

struct TestCD2 { bool val1; float val2; };
struct TestCD1 { TestCD2 compact; };

static FieldInfo gTestCD2Fields[] = {
    { offsetof(TestCD2, val1), Type_Bool,  false },
    { offsetof(TestCD2, val2), Type_Float, (intptr_t)"3.14" },
};
static SettingInfo gTestCD2Info = { sizeof(TestCD2), 2, gTestCD2Fields, "Val1\0Val2" };

static FieldInfo gTestCD1Fields[] = {
    { offsetof(TestCD1, compact), Type_Compact, (intptr_t)&gTestCD2Info },
};
static SettingInfo gTestCD1Info = { sizeof(TestCD1), 1, gTestCD1Fields, "Compact" };

namespace sertxt {

static bool TestSerializeIni()
{
    sertxt::SetSerializeTxtFormat(Format_Ini);

    const WCHAR *path = L"..\\tools\\serini_test\\data.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    Settings *s = DeserializeSettings(data, str::Len(data));
    Check(s); // failed to parse file
    Check(str::Eq(s->basic->inverseSearchCmdLine, L" A wide string\r\nsplit over multiple lines\r\nwith trailing whitespace "));

    size_t len;
    ScopedMem<char> ser((char *)SerializeSettings(s, &len));
    Check(str::Len(ser) == len);
    Check(str::Eq(data, ser));
    FreeSettings(s);

    return true;
}

static bool TestSerializeSqt()
{
    sertxt::SetSerializeTxtFormat(Format_Sqt);

    const WCHAR *path = L"..\\tools\\serini_test\\data.sqt";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    Settings *s = DeserializeSettings(data, str::Len(data));
    Check(s); // failed to parse file
    Check(str::Eq(s->basic->inverseSearchCmdLine, L" A wide string\r\nsplit over multiple lines\r\nwith trailing whitespace "));

    size_t len;
    ScopedMem<char> ser((char *)SerializeSettings(s, &len));
    Check(str::Len(ser) == len);
    Check(str::Eq(data, ser));
    FreeSettings(s);

    return true;
}

static bool TestSerializeTxt()
{
    sertxt::SetSerializeTxtFormat(Format_Txt);

    const WCHAR *path = L"..\\tools\\sertxt_test\\data.txt";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    Settings *s = DeserializeSettings(data, str::Len(data));
    Check(s); // failed to parse file

    size_t len;
    ScopedMem<char> ser((char *)SerializeSettings(s, &len));
    Check(str::Len(ser) == len);
    Check(str::Eq(data, ser));
    FreeSettings(s);

    return true;
}

static bool TestSerializeTxt2()
{
    sertxt::SetSerializeTxtFormat(Format_Txt2);

    const WCHAR *path = L"..\\tools\\sertxt_test\\data.txt";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    Settings *s = DeserializeSettings(data, str::Len(data));
    Check(s); // failed to parse file

    size_t len;
    ScopedMem<char> ser((char *)SerializeSettings(s, &len));
    Check(str::Len(ser) == len);
    Check(str::Eq(data, ser));
    FreeSettings(s);

    return true;
}

static bool TestSerializeWithDefaultsIni()
{
    sertxt::SetSerializeTxtFormat(Format_Ini);

    const WCHAR *defaultPath = L"..\\tools\\serini_test\\data.ini";
    const char *data = "\
[basic]\n\
global_prefs_only = true\n\
default_zoom = 38.5\n\
";

    ScopedMem<char> defaultData(file::ReadAll(defaultPath, NULL));
    Check(defaultData); // failed to read file
    Settings *s = DeserializeSettingsWithDefault(data, str::Len(data), defaultData, str::Len(defaultData));
    Check(s); // failed to parse file
    Check(s->basic->globalPrefsOnly && 38.5f == s->basic->defaultZoom);
    Check(s->basic->showStartPage && !s->basic->pdfAssociateDoIt);
    FreeSettings(s);

    return true;
}

static bool TestSerializeWithDefaultsSqt()
{
    sertxt::SetSerializeTxtFormat(Format_Sqt);

    const WCHAR *defaultPath = L"..\\tools\\serini_test\\data.sqt";
    const char *data = "\
basic [\n\
global_prefs_only = true\n\
default_zoom = 38.5\n\
]";

    ScopedMem<char> defaultData(file::ReadAll(defaultPath, NULL));
    Check(defaultData); // failed to read file
    Settings *s = DeserializeSettingsWithDefault(data, str::Len(data), defaultData, str::Len(defaultData));
    Check(s); // failed to parse file
    Check(s->basic->globalPrefsOnly && 38.5f == s->basic->defaultZoom);
    Check(s->basic->showStartPage && !s->basic->pdfAssociateDoIt);
    FreeSettings(s);

    return true;
}

static bool TestSerializeWithDefaultsTxt()
{
    sertxt::SetSerializeTxtFormat(Format_Txt);

    const WCHAR *defaultPath = L"..\\tools\\sertxt_test\\data.txt";
    const char *data = "\
basic [\n\
global_prefs_only = true\n\
default_zoom = 38.5\n\
]";

    ScopedMem<char> defaultData(file::ReadAll(defaultPath, NULL));
    Check(defaultData); // failed to read file
    Settings *s = DeserializeSettingsWithDefault(data, str::Len(data), defaultData, str::Len(defaultData));
    Check(s); // failed to parse file
    Check(s->basic->globalPrefsOnly && 38.5f == s->basic->defaultZoom);
    Check(s->basic->showStartPage && !s->basic->pdfAssociateDoIt);
    FreeSettings(s);

    return true;
}

static bool TestSerializeWithDefaultsTxt2()
{
    sertxt::SetSerializeTxtFormat(Format_Txt2);

    const WCHAR *defaultPath = L"..\\tools\\sertxt_test\\data.txt";
    const char *data = "\
basic [\n\
global_prefs_only = true\n\
default_zoom = 38.5\n\
]";

    ScopedMem<char> defaultData(file::ReadAll(defaultPath, NULL));
    Check(defaultData); // failed to read file
    Settings *s = DeserializeSettingsWithDefault(data, str::Len(data), defaultData, str::Len(defaultData));
    Check(s); // failed to parse file
    Check(s->basic->globalPrefsOnly && 38.5f == s->basic->defaultZoom);
    Check(s->basic->showStartPage && !s->basic->pdfAssociateDoIt);
    FreeSettings(s);

    return true;
}

static bool TestDefaultValues()
{
    sertxt::SetSerializeTxtFormat(Format_Ini);

    Settings *s = DeserializeSettings(NULL, 0);
    Check(s->basic->toolbarVisible && !s->basic->globalPrefsOnly);
    Check(-1 == s->basic->defaultZoom && 2 == s->advanced->pagePadding->top);
    Check(0x6581ff == s->advanced->forwardSearch->highlightColor);
    Check(!s->basic->currLanguage);
    FreeSettings(s);

    return true;
}

static bool TestSerializeTxtAsSqt()
{
    sertxt::SetSerializeTxtFormat(Format_Txt_Sqt);

    const WCHAR *path = L"..\\tools\\sertxt_test\\data.txt";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    Settings *s = DeserializeSettings(data, str::Len(data));
    Check(s); // failed to parse file

    size_t len;
    ScopedMem<char> ser((char *)SerializeSettings(s, &len));
    Check(str::Len(ser) == len);
    Check(str::Eq(data, ser));
    FreeSettings(s);

    return true;
}

}; // namespace sertxt

namespace sqt {

static bool TestSerialize()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3.sqt";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    GlobalPrefs *s = (GlobalPrefs *)Deserialize(data, str::Len(data), &gGlobalPrefsInfo);
    Check(s); // failed to parse file
    Check(str::Eq(s->inverseSearchCmdLine, L" A wide string\r\nsplit over multiple lines\r\nwith trailing whitespace "));
    Check(1 == s->lastUpdateTime.dwHighDateTime && MAXDWORD == s->lastUpdateTime.dwLowDateTime);

    size_t len;
    ScopedMem<char> ser(Serialize(s, &gGlobalPrefsInfo, &len));
    Check(str::Len(ser) == len);
    Check(str::Eq(data, ser));
    FreeStruct(s, &gGlobalPrefsInfo);

    return true;
}

static bool TestSerializeUser()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3-user.sqt";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    UserPrefs *s = (UserPrefs *)Deserialize(data, str::Len(data), &gUserPrefsInfo);
    Check(s); // failed to parse file

    ScopedMem<char> ser(Serialize(s, &gUserPrefsInfo, NULL, "cf. https://sumatrapdf.googlecode.com/svn/trunk/docs/SumatraPDF-user.sqt"));
    FreeStruct(s, &gUserPrefsInfo);
    Check(str::Eq(data, ser));

    return true;
}

static bool TestSerializeRecursiveArray()
{
    static const char *data ="\
Rec[\n\
  Rec [ \n\
    Rec [\n\n\
      ; nesting three levels deep \n\
    ][ \n\
    ] \n\
  ]\n\
  Rec [ # this comment should be ignored\n\
    Rec \n\
    # comments between key and child nodes are tolerated \n\
    [\n\
      # nesting three levels deep\n\
    ]\n\
  ]\n\
  ] ; the superfluous closing brace on the next line is ignored \n\
]\n\
# the following superfluous closing bracket is ignored as well \n\
]\n\
Rec [\n\
  Rec [\n\
  ]\n\
  # the following line is invalid an will be ignored \n\
  Rec [ ]\n\
  Up \n\
  [\n\
    ExternalViewer [\n\
      CommandLine = serini_test.exe\n\
    ]\n\
  ]\n\
]";
    gRecFields[0].value = (intptr_t)&gRecInfo; // needed for recursion
    Rec *r = (Rec *)Deserialize(data, str::Len(data), &gRecInfo);
    Check(2 == r->rec->Count() && 2 == r->rec->At(0)->rec->Count() && 2 == r->rec->At(0)->rec->At(0)->rec->Count());
    Check(0 == r->rec->At(0)->rec->At(0)->rec->At(0)->rec->Count() && 0 == r->rec->At(0)->rec->At(0)->rec->At(1)->rec->Count());
    Check(1 == r->rec->At(0)->rec->At(1)->rec->Count() && 0 == r->rec->At(0)->rec->At(1)->rec->At(0)->rec->Count());
    Check(1 == r->rec->At(1)->rec->Count() && 0 == r->rec->At(1)->rec->At(0)->rec->Count());
    Check(1 == r->rec->At(1)->up.externalViewer->Count() && str::Eq(r->rec->At(1)->up.externalViewer->At(0)->commandLine, L"serini_test.exe"));
    Check(str::Eq(r->rec->At(0)->rec->At(1)->up.printerDefaults.printScale, "shrink"));
    FreeStruct(r, &gRecInfo);

    return true;
}

static bool TestDefaultValues()
{
    UserPrefs *p = (UserPrefs *)Deserialize(NULL, 0, &gUserPrefsInfo);
    Check(!p->advancedPrefs.escToExit && !p->ebookUI.traditionalEbookUI);
    Check(0xffffff == p->advancedPrefs.pageColor && 0x000000 == p->advancedPrefs.textColor);
    Check(0x6581ff == p->forwardSearch.highlightColor && 15 == p->forwardSearch.highlightWidth);
    Check(4 == p->pagePadding.innerX && 2 == p->pagePadding.outerY);
    Check(str::Eq(p->printerDefaults.printScale, "shrink"));
    Check(24 == p->advancedPrefs.zoomLevels->Count() && 6400 == p->advancedPrefs.zoomLevels->At(23));
    Check(3 == p->backgroundGradient.colors->Count() && RGB(0x28, 0x28, 0xAA) == p->backgroundGradient.colors->At(0));
    FreeStruct(p, &gUserPrefsInfo);

    return true;
}

static bool TestCompactDefaultValues()
{
    static const char *data = "Compact = true -4.25";
    TestCD1 *cd = (TestCD1 *)Deserialize(data, str::Len(data), &gTestCD1Info);
    Check(cd && cd->compact.val1 && -4.25f == cd->compact.val2);
    FreeStruct(cd, &gTestCD1Info);

    cd = (TestCD1 *)Deserialize(NULL, 0, &gTestCD1Info);
    Check(cd && !cd->compact.val1 && 3.14f == cd->compact.val2);
    FreeStruct(cd, &gTestCD1Info);

    return true;
}

static bool TestSerializeIniAsSqt()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3-user.ini";
    const WCHAR *pathCmp = L"..\\tools\\serini_test\\data3-user.sqt";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    UserPrefs *s = (UserPrefs *)Deserialize(data, str::Len(data), &gUserPrefsInfo);
    Check(s); // failed to parse file

    ScopedMem<char> dataCmp(file::ReadAll(pathCmp, NULL));
    ScopedMem<char> ser(Serialize(s, &gUserPrefsInfo, NULL, "cf. https://sumatrapdf.googlecode.com/svn/trunk/docs/SumatraPDF-user.sqt"));
    Check(str::Eq(dataCmp, ser));
    FreeStruct(s, &gUserPrefsInfo);

    return true;
}

}; // namespace sqt

namespace benc {

static FieldInfo gGlobalPrefsFieldsBenc[] = {
    { offsetof(GlobalPrefs, cbxR2L), Type_Bool, false },
    { offsetof(GlobalPrefs, defaultDisplayMode), Type_String, (intptr_t)L"automatic" },
    { offsetof(GlobalPrefs, enableAutoUpdate), Type_Bool, true },
    { offsetof(GlobalPrefs, enableTeXEnhancements), Type_Bool, false },
    { offsetof(GlobalPrefs, favVisible), Type_Bool, false },
    { offsetof(GlobalPrefs, globalPrefsOnly), Type_Bool, false },
    { offsetof(GlobalPrefs, inverseSearchCmdLine), Type_String, NULL },
    { offsetof(GlobalPrefs, lastUpdateTime), Type_Compact, 0 },
    { offsetof(GlobalPrefs, openCountWeek), Type_Int, 0 },
    { offsetof(GlobalPrefs, pdfAssociateDontAskAgain), Type_Bool, false },
    { offsetof(GlobalPrefs, pdfAssociateShouldAssociate), Type_Bool, false },
    { offsetof(GlobalPrefs, rememberOpenedFiles), Type_Bool, true },
    { offsetof(GlobalPrefs, showStartPage), Type_Bool, true },
    { offsetof(GlobalPrefs, tocVisible), Type_Bool, true },
    { offsetof(GlobalPrefs, toolbarVisible), Type_Bool, true },
    { offsetof(GlobalPrefs, sidebarDx), Type_Int, 0 },
    { offsetof(GlobalPrefs, tocDy), Type_Int, 0 },
    { offsetof(GlobalPrefs, currLangCode), Type_Utf8String, NULL },
    { offsetof(GlobalPrefs, useSysColors), Type_Bool, false },
    { offsetof(GlobalPrefs, versionToSkip), Type_String, NULL },
    { offsetof(GlobalPrefs, windowPos.dx), Type_Int, 1 },
    { offsetof(GlobalPrefs, windowPos.dy), Type_Int, 1 },
    { offsetof(GlobalPrefs, windowState), Type_Int, 1 },
    { offsetof(GlobalPrefs, windowPos.x), Type_Int, 1 },
    { offsetof(GlobalPrefs, windowPos.y), Type_Int, 1 },
    { offsetof(GlobalPrefs, defaultZoom), Type_Float, (intptr_t)"-1" },
};
static SettingInfo gGlobalPrefsInfoBenc = { sizeof(GlobalPrefs), 26, gGlobalPrefsFieldsBenc, "CBX_Right2Left\0Display Mode\0EnableAutoUpdate\0ExposeInverseSearch\0FavVisible\0GlobalPrefsOnly\0InverseSearchCommandLine\0LastUpdate\0OpenCountWeek\0PdfAssociateDontAskAgain\0PdfAssociateShouldAssociate\0RememberOpenedFiles\0ShowStartPage\0ShowToc\0ShowToolbar\0Toc DX\0Toc Dy\0UILanguage\0UseSysColors\0VersionToSkip\0Window DX\0Window DY\0Window State\0Window X\0Window Y\0ZoomVirtual" };

static FieldInfo gFileFieldsBenc[] = {
    { offsetof(File, decryptionKey), Type_Utf8String, NULL },
    { offsetof(File, displayMode), Type_String, (intptr_t)L"automatic" },
    { offsetof(File, filePath), Type_String, NULL },
    { offsetof(File, isMissing), Type_Bool, false },
    { offsetof(File, openCount), Type_Int, 0 },
    { offsetof(File, pageNo), Type_Int, 1 },
    { offsetof(File, isPinned), Type_Bool, false },
    { offsetof(File, reparseIdx), Type_Int, 0 },
    { offsetof(File, rotation), Type_Int, 0 },
    { offsetof(File, scrollPos.x), Type_Int, 0 },
    { offsetof(File, scrollPos.y), Type_Int, 0 },
    { offsetof(File, tocVisible), Type_Bool, true },
    { offsetof(File, sidebarDx), Type_Int, 0 },
    { offsetof(File, tocState), Type_IntArray, NULL },
    { offsetof(File, useGlobalValues), Type_Bool, false },
    { offsetof(File, windowPos.dx), Type_Int, 1 },
    { offsetof(File, windowPos.dy), Type_Int, 1 },
    { offsetof(File, windowState), Type_Int, 1 },
    { offsetof(File, windowPos.x), Type_Int, 1 },
    { offsetof(File, windowPos.y), Type_Int, 1 },
    { offsetof(File, zoomVirtual), Type_Float, (intptr_t)"100" },
};
static SettingInfo gFileInfoBenc = { sizeof(File), 21, gFileFieldsBenc, "Decryption Key\0Display Mode\0File\0Missing\0OpenCount\0Page\0Pinned\0ReparseIdx\0Rotation\0Scroll X2\0Scroll Y2\0ShowToc\0Toc DX\0TocToggles\0UseGlobalValues\0Window DX\0Window DY\0Window State\0Window X\0Window Y\0ZoomVirtual" };

static FieldInfo gBencGlobalPrefsFields[] = {
    { offsetof(GlobalPrefs, file), Type_Array, (intptr_t)&gFileInfoBenc },
    { 0 /* self */, Type_Struct, (intptr_t)&gGlobalPrefsInfoBenc },
    // Favorites must be read after File History
    { offsetof(GlobalPrefs, file), Type_Compact, NULL },
};
static SettingInfo gBencGlobalPrefs = { sizeof(GlobalPrefs), 3, gBencGlobalPrefsFields, "File History\0gp\0Favorites" };

static FieldInfo gUserPrefsFieldsBenc[] = {
    { offsetof(UserPrefs, advancedPrefs.mainWindowBackground), Type_Color, 0xfff200 },
    { offsetof(UserPrefs, advancedPrefs.escToExit), Type_Bool, false },
    { offsetof(UserPrefs, forwardSearch.highlightColor), Type_Color, 0x6581ff },
    { offsetof(UserPrefs, forwardSearch.highlightOffset), Type_Int, 0 },
    { offsetof(UserPrefs, forwardSearch.highlightPermanent), Type_Bool, false },
    { offsetof(UserPrefs, forwardSearch.highlightWidth), Type_Int, 15 },
};
static SettingInfo gUserPrefsInfoBenc = { sizeof(UserPrefs), 6, gUserPrefsFieldsBenc, "BgColor\0EscToExit\0ForwardSearch_HighlightColor\0ForwardSearch_HighlightOffset\0ForwardSearch_HighlightPermanent\0ForwardSearch_HighlightWidth" };

static FieldInfo gBencUserPrefsFields[] = {
    { 0 /* self */, Type_Struct, (intptr_t)&gUserPrefsInfoBenc },
};
static SettingInfo gBencUserPrefs = { sizeof(UserPrefs), 1, gBencUserPrefsFields, "gp" };

static bool BencGlobalPrefsCallback(BencDict *dict, const FieldInfo *field, const char *fieldName, uint8_t *fieldPtr)
{
    if (str::Eq(fieldName, "LastUpdate")) {
        BencString *val = dict ? dict->GetString(fieldName) : NULL;
        if (!val || !_HexToMem(val->RawValue(), (FILETIME *)fieldPtr))
            ZeroMemory(fieldPtr, sizeof(FILETIME));
        return true;
    }
    if (str::Eq(fieldName, "Favorites")) {
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
        return true;
    }
    return false;
}

static bool TestDeserialize()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3.benc";
    const WCHAR *pathCmp = L"..\\tools\\serini_test\\data3.sqt";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    GlobalPrefs *s = (GlobalPrefs *)Deserialize(data, str::Len(data), &gBencGlobalPrefs, BencGlobalPrefsCallback);
    Check(s); // failed to parse file

    ScopedMem<char> dataCmp(file::ReadAll(pathCmp, NULL));
    dataCmp.Set(str::Replace(dataCmp, "\t\tPageLabel = ii\r\n", ""));
    ScopedMem<char> ser(sqt::Serialize(s, &gGlobalPrefsInfo, NULL));
    Check(str::Eq(dataCmp, ser));
    sqt::FreeStruct(s, &gGlobalPrefsInfo);

    return true;
}

}; // namespace benc

int main(int argc, char **argv)
{
#ifdef DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    int errors = 0;
    // tests for SerializeTxt2.cpp
    if (!sertxt::TestSerializeIni())
        errors++;
    if (!sertxt::TestSerializeWithDefaultsIni())
        errors++;
    if (!sertxt::TestSerializeSqt())
        errors++;
    if (!sertxt::TestSerializeWithDefaultsSqt())
        errors++;
    if (!sertxt::TestSerializeTxt())
        errors++;
    if (!sertxt::TestSerializeWithDefaultsTxt())
        errors++;
    if (!sertxt::TestSerializeTxt2())
        errors++;
    if (!sertxt::TestSerializeWithDefaultsTxt2())
        errors++;
    if (!sertxt::TestDefaultValues())
        errors++;
    if (!sertxt::TestSerializeTxtAsSqt())
        errors++;
    // tests for SerializeSqt.cpp
    if (!sqt::TestSerialize())
        errors++;
    if (!sqt::TestSerializeUser())
        errors++;
    if (!sqt::TestSerializeRecursiveArray())
        errors++;
    if (!sqt::TestDefaultValues())
        errors++;
    if (!sqt::TestCompactDefaultValues())
        errors++;
    if (!sqt::TestSerializeIniAsSqt())
        errors++;
    // tests for DeserializeBenc.cpp
    if (!benc::TestDeserialize())
        errors++;
    return errors;
}
