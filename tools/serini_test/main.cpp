/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#define INCLUDE_APPPREFS3_METADATA
#include "AppPrefs3.h"
#include "FileUtil.h"
#include "SerializeIni.h"
#include "SerializeIni3.h"
#include "SerializeSqt.h"
#include "../sertxt_test/SettingsSumatra.h"

#define Check(x) CrashIf(!(x)); if (!(x)) return false; else NoOp()

#pragma warning(disable: 4505)

static bool TestSerializeIni()
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
    sertxt::Settings *s = sertxt::DeserializeSettings(data, str::Len(data));
    Check(s); // failed to parse file
#ifndef TEST_SERIALIZE_TXT
    Check(str::Find(s->basic->inverseSearchCmdLine, L"\r\n"));
#endif

    size_t len;
    ScopedMem<char> ser((char *)sertxt::SerializeSettings(s, &len));
    Check(str::Len(ser) == len);
    Check(str::Eq(data, ser));
    FreeSettings(s);

    return true;
}

static bool TestSerializeIniWithDefaults()
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
    sertxt::Settings *s = sertxt::DeserializeSettingsWithDefault(data, str::Len(data), defaultData, str::Len(defaultData));
    Check(s); // failed to parse file
    Check(s->basic->globalPrefsOnly && 38.5f == s->basic->defaultZoom);
    Check(s->basic->showStartPage && !s->basic->pdfAssociateDoIt);
    sertxt::FreeSettings(s);

    return true;
}

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

static bool TestSerializeIniAsSqt()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3-user.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    UserPrefs *s = (UserPrefs *)Deserialize(data, str::Len(data), gUserPrefsInfo);
    Check(s); // failed to parse file

    ScopedMem<char> ser(ini3::Serialize(s, gUserPrefsInfo, NULL, "cf. https://sumatrapdf.googlecode.com/svn/trunk/docs/SumatraPDF-user.ini"));
    Check(str::Eq(data, ser));
    FreeStruct(s, gUserPrefsInfo);

    return true;
}

}; // namespace sqt

int main(int argc, char **argv)
{
#ifdef DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    int errors = 0;
    if (!TestSerializeIni())
        errors++;
    if (!TestSerializeIniWithDefaults())
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
    if (!sqt::TestSerializeIniAsSqt())
        errors++;
    return errors;
}
