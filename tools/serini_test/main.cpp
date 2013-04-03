/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#define INCLUDE_APPPREFS3_METADATA
#include "AppPrefs3.h"
#include "FileUtil.h"
#include "DeserializeBenc.h"
#include "SerializeIni.h"
#include "SerializeIni3.h"
#include "SerializeSqt.h"
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
    { Type_Array, offsetof(Rec, rec), -1 /* set to (intptr_t)gRecInfo before usage */ },
    { Type_Struct, offsetof(Rec, up), (intptr_t)&gUserPrefsInfo },
};
static SettingInfo gRecInfo = { sizeof(Rec), 2, gRecFields, "Rec\0Up" };

struct TestCD2 { bool val1; float val2; };
struct TestCD1 { TestCD2 compact; };

static FieldInfo gTestCD2Fields[] = {
    { Type_Bool, offsetof(TestCD2, val1), false },
    { Type_Float, offsetof(TestCD2, val2), (intptr_t)"3.14" },
};
static SettingInfo gTestCD2Info = { sizeof(TestCD2), 2, gTestCD2Fields, "Val1\0Val2" };

static FieldInfo gTestCD1Fields[] = {
    { Type_Compact, offsetof(TestCD1, compact), (intptr_t)&gTestCD2Info },
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
    Check(str::Find(s->basic->inverseSearchCmdLine, L"\r\n"));

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
    Check(str::Find(s->basic->inverseSearchCmdLine, L"\r\n"));

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

}; // namespace sertxt

namespace ini3 {

static bool TestSerialize()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    GlobalPrefs *s = (GlobalPrefs *)Deserialize(data, str::Len(data), &gGlobalPrefsInfo);
    Check(s); // failed to parse file
    Check(str::Find(s->inverseSearchCmdLine, L"\r\n"));
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
    const WCHAR *path = L"..\\tools\\serini_test\\data3-user.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    UserPrefs *s = (UserPrefs *)Deserialize(data, str::Len(data), &gUserPrefsInfo);
    Check(s); // failed to parse file

    ScopedMem<char> ser(Serialize(s, &gUserPrefsInfo, NULL, "cf. https://sumatrapdf.googlecode.com/svn/trunk/docs/SumatraPDF-user.ini"));
    FreeStruct(s, &gUserPrefsInfo);
    Check(str::Eq(data, ser));

    return true;
}

static bool TestSerializeRecursiveArray()
{
    static const char *data ="\
[Rec]\n\
[Rec.Rec]\n\
[Rec.Rec.Rec]\n\
[Rec.Rec.Rec]\n\
[Rec.Rec]\n\
[Rec.Rec.Rec]\n\
[Rec]\n\
[Rec.Rec]\n\
# [Rec.Up] may be omitted\n\
[Rec.Up.ExternalViewer]\n\
CommandLine = serini_test.exe\n\
";
    gRecFields[0].value = (intptr_t)&gRecInfo; // needed for recursion
    Rec *r = (Rec *)Deserialize(data, str::Len(data), &gRecInfo);
    Check(2 == r->rec->Count() && 2 == r->rec->At(0)->rec->Count() && 2 == r->rec->At(0)->rec->At(0)->rec->Count());
    Check(0 == r->rec->At(0)->rec->At(0)->rec->At(0)->rec->Count() && 0 == r->rec->At(0)->rec->At(0)->rec->At(1)->rec->Count());
    Check(1 == r->rec->At(0)->rec->At(1)->rec->Count() && 0 == r->rec->At(0)->rec->At(1)->rec->At(0)->rec->Count());
    Check(1 == r->rec->At(1)->rec->Count() && 0 == r->rec->At(1)->rec->At(0)->rec->Count());
    Check(1 == r->rec->At(1)->up.externalViewer->Count() && str::Eq(r->rec->At(1)->up.externalViewer->At(0)->commandLine, L"serini_test.exe"));
    Check(str::Eq(r->rec->At(0)->rec->At(1)->up.printerDefaults.printScale, "shrink"));
    FreeStruct(r, &gRecInfo);

    // TODO: recurse even if array parents are missing?
    // (bounded by maximum section name length)
    data = "[Rec.Rec]";
    r = (Rec *)Deserialize(data, str::Len(data), &gRecInfo);
    Check(0 == r->rec->Count());
    FreeStruct(r, &gRecInfo);

    return true;
}

static bool TestDefaultValues()
{
    UserPrefs *p = (UserPrefs *)Deserialize(NULL, 0, &gUserPrefsInfo);
    Check(!p->advancedPrefs.escToExit && !p->advancedPrefs.traditionalEbookUI);
    Check(0xffffff == p->advancedPrefs.pageColor && 0x000000 == p->advancedPrefs.textColor);
    Check(0x6581ff == p->forwardSearch.highlightColor && 15 == p->forwardSearch.highlightWidth);
    Check(4 == p->pagePadding.innerX && 2 == p->pagePadding.outerY);
    Check(str::Eq(p->printerDefaults.printScale, "shrink"));
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

}; // namespace ini3

namespace sqt {

static bool TestSerialize()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3.sqt";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    GlobalPrefs *s = (GlobalPrefs *)Deserialize(data, str::Len(data), &gGlobalPrefsInfo);
    Check(s); // failed to parse file
    Check(str::Find(s->inverseSearchCmdLine, L"\r\n"));
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
    Check(!p->advancedPrefs.escToExit && !p->advancedPrefs.traditionalEbookUI);
    Check(0xffffff == p->advancedPrefs.pageColor && 0x000000 == p->advancedPrefs.textColor);
    Check(0x6581ff == p->forwardSearch.highlightColor && 15 == p->forwardSearch.highlightWidth);
    Check(4 == p->pagePadding.innerX && 2 == p->pagePadding.outerY);
    Check(str::Eq(p->printerDefaults.printScale, "shrink"));
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

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    UserPrefs *s = (UserPrefs *)Deserialize(data, str::Len(data), &gUserPrefsInfo);
    Check(s); // failed to parse file

    ScopedMem<char> ser(ini3::Serialize(s, &gUserPrefsInfo, NULL, "cf. https://sumatrapdf.googlecode.com/svn/trunk/docs/SumatraPDF-user.ini"));
    Check(str::Eq(data, ser));
    FreeStruct(s, &gUserPrefsInfo);

    return true;
}

}; // namespace sqt

namespace benc {

static FieldInfo gGlobalPrefsFieldsBenc[] = {
    { Type_Bool, offsetof(GlobalPrefs, cbxR2L), false },
    { Type_String, offsetof(GlobalPrefs, defaultDisplayMode), (intptr_t)L"automatic" },
    { Type_Bool, offsetof(GlobalPrefs, enableAutoUpdate), true },
    { Type_Bool, offsetof(GlobalPrefs, enableTeXEnhancements), false },
    { Type_Bool, offsetof(GlobalPrefs, favVisible), false },
    { Type_Bool, offsetof(GlobalPrefs, globalPrefsOnly), false },
    { Type_String, offsetof(GlobalPrefs, inverseSearchCmdLine), NULL },
    { Type_Custom, offsetof(GlobalPrefs, lastUpdateTime), 0 },
    { Type_Int, offsetof(GlobalPrefs, openCountWeek), 0 },
    { Type_Bool, offsetof(GlobalPrefs, pdfAssociateDontAskAgain), false },
    { Type_Bool, offsetof(GlobalPrefs, pdfAssociateShouldAssociate), false },
    { Type_Bool, offsetof(GlobalPrefs, rememberOpenedFiles), true },
    { Type_Bool, offsetof(GlobalPrefs, showStartPage), true },
    { Type_Bool, offsetof(GlobalPrefs, tocVisible), true },
    { Type_Bool, offsetof(GlobalPrefs, toolbarVisible), true },
    { Type_Int, offsetof(GlobalPrefs, sidebarDx), 0 },
    { Type_Int, offsetof(GlobalPrefs, tocDy), 0 },
    { Type_Utf8String, offsetof(GlobalPrefs, currLangCode), NULL },
    { Type_Bool, offsetof(GlobalPrefs, useSysColors), false },
    { Type_String, offsetof(GlobalPrefs, versionToSkip), NULL },
    { Type_Int, offsetof(GlobalPrefs, windowPos.dx), 1 },
    { Type_Int, offsetof(GlobalPrefs, windowPos.dy), 1 },
    { Type_Int, offsetof(GlobalPrefs, windowState), 1 },
    { Type_Int, offsetof(GlobalPrefs, windowPos.x), 1 },
    { Type_Int, offsetof(GlobalPrefs, windowPos.y), 1 },
    { Type_Float, offsetof(GlobalPrefs, defaultZoom), (intptr_t)"-1" },
};
static SettingInfo gGlobalPrefsInfoBenc = { sizeof(GlobalPrefs), 26, gGlobalPrefsFieldsBenc, "CBX_Right2Left\0Display Mode\0EnableAutoUpdate\0ExposeInverseSearch\0FavVisible\0GlobalPrefsOnly\0InverseSearchCommandLine\0LastUpdate\0OpenCountWeek\0PdfAssociateDontAskAgain\0PdfAssociateShouldAssociate\0RememberOpenedFiles\0ShowStartPage\0ShowToc\0ShowToolbar\0Toc DX\0Toc Dy\0UILanguage\0UseSysColors\0VersionToSkip\0Window DX\0Window DY\0Window State\0Window X\0Window Y\0ZoomVirtual" };

static FieldInfo gFileFieldsBenc[] = {
    { Type_Utf8String, offsetof(File, decryptionKey), NULL },
    { Type_String, offsetof(File, displayMode), (intptr_t)L"automatic" },
    { Type_String, offsetof(File, filePath), NULL },
    { Type_Bool, offsetof(File, isMissing), false },
    { Type_Int, offsetof(File, openCount), 0 },
    { Type_Int, offsetof(File, pageNo), 1 },
    { Type_Bool, offsetof(File, isPinned), false },
    { Type_Int, offsetof(File, reparseIdx), 0 },
    { Type_Int, offsetof(File, rotation), 0 },
    { Type_Int, offsetof(File, scrollPos.x), 0 },
    { Type_Int, offsetof(File, scrollPos.y), 0 },
    { Type_Bool, offsetof(File, tocVisible), true },
    { Type_Int, offsetof(File, sidebarDx), 0 },
    { Type_Custom, offsetof(File, tocState), NULL },
    { Type_Bool, offsetof(File, useGlobalValues), false },
    { Type_Int, offsetof(File, windowPos.dx), 1 },
    { Type_Int, offsetof(File, windowPos.dy), 1 },
    { Type_Int, offsetof(File, windowState), 1 },
    { Type_Int, offsetof(File, windowPos.x), 1 },
    { Type_Int, offsetof(File, windowPos.y), 1 },
    { Type_Float, offsetof(File, zoomVirtual), (intptr_t)"100" },
};
static SettingInfo gFileInfoBenc = { sizeof(File), 21, gFileFieldsBenc, "Decryption Key\0Display Mode\0File\0Missing\0OpenCount\0Page\0Pinned\0ReparseIdx\0Rotation\0Scroll X2\0Scroll Y2\0ShowToc\0Toc DX\0TocToggles\0UseGlobalValues\0Window DX\0Window DY\0Window State\0Window X\0Window Y\0ZoomVirtual" };

static FieldInfo gBencGlobalPrefsFields[] = {
    { Type_Array, offsetof(GlobalPrefs, file), (intptr_t)&gFileInfoBenc },
    { Type_Struct, 0 /* self */, (intptr_t)&gGlobalPrefsInfoBenc },
    // Favorites must be read after File History
    { Type_Custom, offsetof(GlobalPrefs, file), NULL },
};
static SettingInfo gBencGlobalPrefs = { sizeof(GlobalPrefs), 3, gBencGlobalPrefsFields, "File History\0gp\0Favorites" };

static FieldInfo gUserPrefsFieldsBenc[] = {
    { Type_Color, offsetof(UserPrefs, advancedPrefs.mainWindowBackground), 0xfff200 },
    { Type_Bool, offsetof(UserPrefs, advancedPrefs.escToExit), false },
    { Type_Color, offsetof(UserPrefs, forwardSearch.highlightColor), 0x6581ff },
    { Type_Int, offsetof(UserPrefs, forwardSearch.highlightOffset), 0 },
    { Type_Bool, offsetof(UserPrefs, forwardSearch.highlightPermanent), false },
    { Type_Int, offsetof(UserPrefs, forwardSearch.highlightWidth), 15 },
};
static SettingInfo gUserPrefsInfoBenc = { sizeof(UserPrefs), 6, gUserPrefsFieldsBenc, "BgColor\0EscToExit\0ForwardSearch_HighlightColor\0ForwardSearch_HighlightOffset\0ForwardSearch_HighlightPermanent\0ForwardSearch_HighlightWidth" };

static FieldInfo gBencUserPrefsFields[] = {
    { Type_Struct, 0 /* self */, (intptr_t)&gUserPrefsInfoBenc },
};
static SettingInfo gBencUserPrefs = { sizeof(UserPrefs), 1, gBencUserPrefsFields, "gp" };

static bool TestDeserialize()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3.benc";
    const WCHAR *pathCmp = L"..\\tools\\serini_test\\data3.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    GlobalPrefs *s = (GlobalPrefs *)Deserialize(data, str::Len(data), &gBencGlobalPrefs);
    Check(s); // failed to parse file

    ScopedMem<char> dataCmp(file::ReadAll(pathCmp, NULL));
    dataCmp.Set(str::Replace(dataCmp, "PageLabel = ii\r\n", ""));
    ScopedMem<char> ser(ini3::Serialize(s, &gGlobalPrefsInfo, NULL));
    Check(str::Eq(dataCmp, ser));
    ini3::FreeStruct(s, &gGlobalPrefsInfo);

    return true;
}

}; // namespace benc

int main(int argc, char **argv)
{
#ifdef DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    int errors = 0;
    // tests for SerializeIni.cpp
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
    if (!sertxt::TestDefaultValues())
        errors++;
    // tests for SerializeIni3.cpp
    if (!ini3::TestSerialize())
        errors++;
    if (!ini3::TestSerializeUser())
        errors++;
    if (!ini3::TestSerializeRecursiveArray())
        errors++;
    if (!ini3::TestDefaultValues())
        errors++;
    if (!ini3::TestCompactDefaultValues())
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
