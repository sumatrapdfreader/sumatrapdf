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
#include "../sertxt_test/SettingsSumatra.h"

#define Check(x) CrashIf(!(x)); if (!(x)) return false; else NoOp()

// allow to easily disable tests for binary size comparison
#pragma warning(disable: 4505)

// structs for recursion and compactness tests

struct Rec {
    Vec<Rec *> * rec;
    UserPrefs up;
};

static SettingInfo gRecInfo[] = {
    { Type_Meta, 2, sizeof(Rec), (intptr_t)"Rec\0Up" },
    { Type_Array, 0, offsetof(Rec, rec), (intptr_t)gRecInfo },
    { Type_Struct, 4, offsetof(Rec, up), (intptr_t)gUserPrefsInfo },
};

struct TestCD2 { bool val1; float val2; };
struct TestCD1 { TestCD2 compact; };

static SettingInfo gTestCD2Info[] = {
    { Type_Meta, 2, sizeof(TestCD2), (intptr_t)"Val1\0Val2" },
    { Type_Bool, 0, offsetof(TestCD2, val1), false },
    { Type_Float, 5, offsetof(TestCD2, val2), (intptr_t)"3.14" },
};

static SettingInfo gTestCD1Info[] = {
    { Type_Meta, 1, sizeof(TestCD1), (intptr_t)"Compact" },
    { Type_Compact, 0, offsetof(TestCD1, compact), (intptr_t)gTestCD2Info },
};

namespace sertxt {

static bool TestSerialize()
{
#ifdef TEST_SERIALIZE_SQT
    const WCHAR *path = L"..\\tools\\serini_test\\data.sqt";
#elif defined(TEST_SERIALIZE_TXT)
    const WCHAR *path = L"..\\tools\\sertxt_test\\data.txt";
#else
    const WCHAR *path = L"..\\tools\\serini_test\\data.ini";
#endif

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    Settings *s = DeserializeSettings(data, str::Len(data));
    Check(s); // failed to parse file
#ifndef TEST_SERIALIZE_TXT
    Check(str::Find(s->basic->inverseSearchCmdLine, L"\r\n"));
#endif

    size_t len;
    ScopedMem<char> ser((char *)SerializeSettings(s, &len));
    Check(str::Len(ser) == len);
    Check(str::Eq(data, ser));
    FreeSettings(s);

    return true;
}

static bool TestSerializeWithDefaults()
{
#if defined(TEST_SERIALIZE_SQT) || defined(TEST_SERIALIZE_TXT)
#ifdef TEST_SERIALIZE_TXT
    const WCHAR *defaultPath = L"..\\tools\\sertxt_test\\data.txt";
#else
    const WCHAR *defaultPath = L"..\\tools\\serini_test\\data.sqt";
#endif
    const char *data = "\
basic [\n\
global_prefs_only = true\n\
default_zoom = 38.5\n\
]";
#else
    const WCHAR *defaultPath = L"..\\tools\\serini_test\\data.ini";
    const char *data = "\
[basic]\n\
global_prefs_only = true\n\
default_zoom = 38.5\n\
";
#endif

    ScopedMem<char> defaultData(file::ReadAll(defaultPath, NULL));
    Check(defaultData); // failed to read file
    Settings *s = DeserializeSettingsWithDefault(data, str::Len(data), defaultData, str::Len(defaultData));
    Check(s); // failed to parse file
    Check(s->basic->globalPrefsOnly && 38.5f == s->basic->defaultZoom);
    Check(s->basic->showStartPage && !s->basic->pdfAssociateDoIt);
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
    GlobalPrefs *s = (GlobalPrefs *)Deserialize(data, str::Len(data), gGlobalPrefsInfo);
    Check(s); // failed to parse file
    Check(str::Find(s->inverseSearchCmdLine, L"\r\n"));
    Check(1 == s->lastUpdateTime.dwHighDateTime && MAXDWORD == s->lastUpdateTime.dwLowDateTime);

    size_t len;
    ScopedMem<char> ser(Serialize(s, gGlobalPrefsInfo, &len));
    Check(str::Len(ser) == len);
    Check(str::Eq(data, ser));
    FreeStruct(s, gGlobalPrefsInfo);

    return true;
}

static bool TestSerializeUser()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3-user.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    UserPrefs *s = (UserPrefs *)Deserialize(data, str::Len(data), gUserPrefsInfo);
    Check(s); // failed to parse file

    ScopedMem<char> ser(Serialize(s, gUserPrefsInfo, NULL, "cf. https://sumatrapdf.googlecode.com/svn/trunk/docs/SumatraPDF-user.ini"));
    FreeStruct(s, gUserPrefsInfo);
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
    Rec *r = (Rec *)Deserialize(data, str::Len(data), gRecInfo);
    Check(2 == r->rec->Count() && 2 == r->rec->At(0)->rec->Count() && 2 == r->rec->At(0)->rec->At(0)->rec->Count());
    Check(0 == r->rec->At(0)->rec->At(0)->rec->At(0)->rec->Count() && 0 == r->rec->At(0)->rec->At(0)->rec->At(1)->rec->Count());
    Check(1 == r->rec->At(0)->rec->At(1)->rec->Count() && 0 == r->rec->At(0)->rec->At(1)->rec->At(0)->rec->Count());
    Check(1 == r->rec->At(1)->rec->Count() && 0 == r->rec->At(1)->rec->At(0)->rec->Count());
    Check(1 == r->rec->At(1)->up.externalViewer->Count() && str::Eq(r->rec->At(1)->up.externalViewer->At(0)->commandLine, L"serini_test.exe"));
    Check(str::Eq(r->rec->At(0)->rec->At(1)->up.printerDefaults.printScale, "shrink"));
    FreeStruct(r, gRecInfo);

    // TODO: recurse even if array parents are missing?
    // (bounded by maximum section name length)
    data = "[Rec.Rec]";
    r = (Rec *)Deserialize(data, str::Len(data), gRecInfo);
    Check(0 == r->rec->Count());
    FreeStruct(r, gRecInfo);

    return true;
}

static bool TestDefaultValues()
{
    UserPrefs *p = (UserPrefs *)Deserialize(NULL, 0, gUserPrefsInfo);
    Check(!p->advancedPrefs.escToExit && !p->advancedPrefs.traditionalEbookUI);
    Check(0xffffff == p->advancedPrefs.pageColor && 0x000000 == p->advancedPrefs.textColor);
    Check(0x6581ff == p->forwardSearch.highlightColor && 15 == p->forwardSearch.highlightWidth);
    Check(4 == p->pagePadding.innerX && 2 == p->pagePadding.outerY);
    Check(str::Eq(p->printerDefaults.printScale, "shrink"));
    FreeStruct(p, gUserPrefsInfo);

    return true;
}

static bool TestCompactDefaultValues()
{
    static const char *data = "Compact = true -4.25";
    TestCD1 *cd = (TestCD1 *)Deserialize(data, str::Len(data), gTestCD1Info);
    Check(cd && cd->compact.val1 && -4.25f == cd->compact.val2);
    FreeStruct(cd, gTestCD1Info);

    cd = (TestCD1 *)Deserialize(NULL, 0, gTestCD1Info);
    Check(cd && !cd->compact.val1 && 3.14f == cd->compact.val2);
    FreeStruct(cd, gTestCD1Info);

    return true;
}

}; // namespace ini3

namespace sqt {

static bool TestSerialize()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3.sqt";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    GlobalPrefs *s = (GlobalPrefs *)Deserialize(data, str::Len(data), gGlobalPrefsInfo);
    Check(s); // failed to parse file
    Check(str::Find(s->inverseSearchCmdLine, L"\r\n"));
    Check(1 == s->lastUpdateTime.dwHighDateTime && MAXDWORD == s->lastUpdateTime.dwLowDateTime);

    size_t len;
    ScopedMem<char> ser(Serialize(s, gGlobalPrefsInfo, &len));
    Check(str::Len(ser) == len);
    Check(str::Eq(data, ser));
    FreeStruct(s, gGlobalPrefsInfo);

    return true;
}

static bool TestSerializeUser()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3-user.sqt";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    UserPrefs *s = (UserPrefs *)Deserialize(data, str::Len(data), gUserPrefsInfo);
    Check(s); // failed to parse file

    ScopedMem<char> ser(Serialize(s, gUserPrefsInfo, NULL, "cf. https://sumatrapdf.googlecode.com/svn/trunk/docs/SumatraPDF-user.sqt"));
    FreeStruct(s, gUserPrefsInfo);
    Check(str::Eq(data, ser));

    return true;
}

static bool TestSerializeRecursiveArray()
{
    static const char *data ="\
Rec[\n\
  Rec = [ \n\
    Rec :[\n\n\
      ; nesting three levels deep \n\
    ][ \n\
    ] \n\
  ]\n\
  Rec: [\n\
    Rec = [\n\
      # nesting three levels deep\n\
    ]\n\
  ]\n\
  ] ; this line is ignored due to the comment \n\
]\n\
# the following superfluous closing bracket is ignored \n\
]\n\
Rec [\n\
  Rec [\n\
  ]\n\
  Up \n\
  [\n\
    ExternalViewer = [\n\
      CommandLine = serini_test.exe\n\
    ]\n\
  ]\n\
]";
    Rec *r = (Rec *)Deserialize(data, str::Len(data), gRecInfo);
    Check(2 == r->rec->Count() && 2 == r->rec->At(0)->rec->Count() && 2 == r->rec->At(0)->rec->At(0)->rec->Count());
    Check(0 == r->rec->At(0)->rec->At(0)->rec->At(0)->rec->Count() && 0 == r->rec->At(0)->rec->At(0)->rec->At(1)->rec->Count());
    Check(1 == r->rec->At(0)->rec->At(1)->rec->Count() && 0 == r->rec->At(0)->rec->At(1)->rec->At(0)->rec->Count());
    Check(1 == r->rec->At(1)->rec->Count() && 0 == r->rec->At(1)->rec->At(0)->rec->Count());
    Check(1 == r->rec->At(1)->up.externalViewer->Count() && str::Eq(r->rec->At(1)->up.externalViewer->At(0)->commandLine, L"serini_test.exe"));
    Check(str::Eq(r->rec->At(0)->rec->At(1)->up.printerDefaults.printScale, "shrink"));
    FreeStruct(r, gRecInfo);

    return true;
}

static bool TestDefaultValues()
{
    UserPrefs *p = (UserPrefs *)Deserialize(NULL, 0, gUserPrefsInfo);
    Check(!p->advancedPrefs.escToExit && !p->advancedPrefs.traditionalEbookUI);
    Check(0xffffff == p->advancedPrefs.pageColor && 0x000000 == p->advancedPrefs.textColor);
    Check(0x6581ff == p->forwardSearch.highlightColor && 15 == p->forwardSearch.highlightWidth);
    Check(4 == p->pagePadding.innerX && 2 == p->pagePadding.outerY);
    Check(str::Eq(p->printerDefaults.printScale, "shrink"));
    FreeStruct(p, gUserPrefsInfo);

    return true;
}

static bool TestCompactDefaultValues()
{
    static const char *data = "Compact = true -4.25";
    TestCD1 *cd = (TestCD1 *)Deserialize(data, str::Len(data), gTestCD1Info);
    Check(cd && cd->compact.val1 && -4.25f == cd->compact.val2);
    FreeStruct(cd, gTestCD1Info);

    cd = (TestCD1 *)Deserialize(NULL, 0, gTestCD1Info);
    Check(cd && !cd->compact.val1 && 3.14f == cd->compact.val2);
    FreeStruct(cd, gTestCD1Info);

    return true;
}

}; // namespace sqt

namespace benc {

static SettingInfo gGlobalPrefsInfoBenc[] = {
    { Type_Meta, 26, sizeof(GlobalPrefs), (intptr_t)"CBX_Right2Left\0Display Mode\0EnableAutoUpdate\0ExposeInverseSearch\0FavVisible\0GlobalPrefsOnly\0InverseSearchCommandLine\0LastUpdate\0OpenCountWeek\0PdfAssociateDontAskAgain\0PdfAssociateShouldAssociate\0RememberOpenedFiles\0ShowStartPage\0ShowToc\0ShowToolbar\0Toc DX\0Toc Dy\0UILanguage\0UseSysColors\0VersionToSkip\0Window DX\0Window DY\0Window State\0Window X\0Window Y\0ZoomVirtual" },
    { Type_Bool, 0, offsetof(GlobalPrefs, cbxR2L), false },
    { Type_String, 15, offsetof(GlobalPrefs, defaultDisplayMode), (intptr_t)L"automatic" },
    { Type_Bool, 28, offsetof(GlobalPrefs, enableAutoUpdate), true },
    { Type_Bool, 45, offsetof(GlobalPrefs, enableTeXEnhancements), false },
    { Type_Bool, 65, offsetof(GlobalPrefs, favVisible), false },
    { Type_Bool, 76, offsetof(GlobalPrefs, globalPrefsOnly), false },
    { Type_String, 92, offsetof(GlobalPrefs, inverseSearchCmdLine), NULL },
    { Type_Meta, 117, offsetof(GlobalPrefs, lastUpdateTime), 0 },
    { Type_Int, 128, offsetof(GlobalPrefs, openCountWeek), 0 },
    { Type_Bool, 142, offsetof(GlobalPrefs, pdfAssociateDontAskAgain), false },
    { Type_Bool, 167, offsetof(GlobalPrefs, pdfAssociateShouldAssociate), false },
    { Type_Bool, 195, offsetof(GlobalPrefs, rememberOpenedFiles), true },
    { Type_Bool, 215, offsetof(GlobalPrefs, showStartPage), true },
    { Type_Bool, 229, offsetof(GlobalPrefs, tocVisible), true },
    { Type_Bool, 237, offsetof(GlobalPrefs, toolbarVisible), true },
    { Type_Int, 249, offsetof(GlobalPrefs, sidebarDx), 0 },
    { Type_Int, 256, offsetof(GlobalPrefs, tocDy), 0 },
    { Type_Utf8String, 263, offsetof(GlobalPrefs, currLangCode), NULL },
    { Type_Bool, 274, offsetof(GlobalPrefs, useSysColors), false },
    { Type_String, 287, offsetof(GlobalPrefs, versionToSkip), NULL },
    { Type_Int, 301, offsetof(GlobalPrefs, windowPos.dx), 1 },
    { Type_Int, 311, offsetof(GlobalPrefs, windowPos.dy), 1 },
    { Type_Int, 321, offsetof(GlobalPrefs, windowState), 1 },
    { Type_Int, 334, offsetof(GlobalPrefs, windowPos.x), 1 },
    { Type_Int, 343, offsetof(GlobalPrefs, windowPos.y), 1 },
    { Type_Float, 352, offsetof(GlobalPrefs, defaultZoom), (intptr_t)"-1" },
};

static SettingInfo gFileInfoBenc[] = {
    { Type_Meta, 21, sizeof(File), (intptr_t)"Decryption Key\0Display Mode\0File\0Missing\0OpenCount\0Page\0Pinned\0ReparseIdx\0Rotation\0Scroll X2\0Scroll Y2\0ShowToc\0Toc DX\0TocToggles\0UseGlobalValues\0Window DX\0Window DY\0Window State\0Window X\0Window Y\0ZoomVirtual" },
    { Type_Utf8String, 0, offsetof(File, decryptionKey), NULL },
    { Type_String, 15, offsetof(File, displayMode), (intptr_t)L"automatic" },
    { Type_String, 28, offsetof(File, filePath), NULL },
    { Type_Bool, 33, offsetof(File, isMissing), false },
    { Type_Int, 41, offsetof(File, openCount), 0 },
    { Type_Int, 51, offsetof(File, pageNo), 1 },
    { Type_Bool, 56, offsetof(File, isPinned), false },
    { Type_Int, 63, offsetof(File, reparseIdx), 0 },
    { Type_Int, 74, offsetof(File, rotation), 0 },
    { Type_Int, 83, offsetof(File, scrollPos.x), 0 },
    { Type_Int, 93, offsetof(File, scrollPos.y), 0 },
    { Type_Bool, 103, offsetof(File, tocVisible), true },
    { Type_Int, 111, offsetof(File, sidebarDx), 0 },
    { Type_Meta, 118, offsetof(File, tocState), NULL },
    { Type_Bool, 129, offsetof(File, useGlobalValues), false },
    { Type_Int, 145, offsetof(File, windowPos.dx), 1 },
    { Type_Int, 155, offsetof(File, windowPos.dy), 1 },
    { Type_Int, 165, offsetof(File, windowState), 1 },
    { Type_Int, 178, offsetof(File, windowPos.x), 1 },
    { Type_Int, 187, offsetof(File, windowPos.y), 1 },
    { Type_Float, 196, offsetof(File, zoomVirtual), (intptr_t)"100" },
};

static SettingInfo gBencGlobalPrefs[] = {
    { Type_Meta, 3, sizeof(GlobalPrefs), (intptr_t)"Favorites\0File History\0gp" },
    { Type_Array, 10, offsetof(GlobalPrefs, file), (intptr_t)gFileInfoBenc },
    { Type_Struct, 23, 0 /* self */, (intptr_t)gGlobalPrefsInfoBenc },
    // Favorites must be read after File History
    { Type_Meta, 0, offsetof(GlobalPrefs, file), NULL },
};

static SettingInfo gUserPrefsInfoBenc[] = {
    { Type_Meta, 6, sizeof(UserPrefs), (intptr_t)"BgColor\0EscToExit\0ForwardSearch_HighlightColor\0ForwardSearch_HighlightOffset\0ForwardSearch_HighlightPermanent\0ForwardSearch_HighlightWidth" },
    { Type_Color, 0, offsetof(UserPrefs, advancedPrefs.mainWindowBackground), 0xfff200 },
    { Type_Bool, 8, offsetof(UserPrefs, advancedPrefs.escToExit), false },
    { Type_Color, 18, offsetof(UserPrefs, forwardSearch.highlightColor), 0x6581ff },
    { Type_Int, 47, offsetof(UserPrefs, forwardSearch.highlightOffset), 0 },
    { Type_Bool, 77, offsetof(UserPrefs, forwardSearch.highlightPermanent), false },
    { Type_Int, 110, offsetof(UserPrefs, forwardSearch.highlightWidth), 15 },
};

static SettingInfo gBencUserPrefs[] = {
    { Type_Meta, 1, sizeof(UserPrefs), (intptr_t)"gp" },
    { Type_Struct, 0, 0 /* self */, (intptr_t)gUserPrefsInfoBenc },
};

static bool TestDeserialize()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3.benc";
    const WCHAR *pathCmp = L"..\\tools\\serini_test\\data3.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    GlobalPrefs *s = (GlobalPrefs *)Deserialize(data, str::Len(data), gBencGlobalPrefs);
    Check(s); // failed to parse file

    ScopedMem<char> dataCmp(file::ReadAll(pathCmp, NULL));
    dataCmp.Set(str::Replace(dataCmp, "PageLabel = ii\r\n", ""));
    ScopedMem<char> ser(ini3::Serialize(s, gGlobalPrefsInfo, NULL));
    Check(str::Eq(dataCmp, ser));
    ini3::FreeStruct(s, gGlobalPrefsInfo);

    return true;
}

}; // namespace benc

static bool TestSerializeIniAsSqt()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3-user.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    UserPrefs *s = (UserPrefs *)sqt::Deserialize(data, str::Len(data), gUserPrefsInfo);
    Check(s); // failed to parse file

    ScopedMem<char> ser(ini3::Serialize(s, gUserPrefsInfo, NULL, "cf. https://sumatrapdf.googlecode.com/svn/trunk/docs/SumatraPDF-user.ini"));
    Check(str::Eq(data, ser));
    sqt::FreeStruct(s, gUserPrefsInfo);

    return true;
}

int main(int argc, char **argv)
{
#ifdef DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    int errors = 0;
    if (!sertxt::TestSerialize())
        errors++;
    if (!sertxt::TestSerializeWithDefaults())
        errors++;
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
    if (!benc::TestDeserialize())
        errors++;
    if (!TestSerializeIniAsSqt())
        errors++;
    return errors;
}
