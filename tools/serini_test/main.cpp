/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#define INCLUDE_APPPREFS3_METADATA
#include "AppPrefs3.h"
#include "FileUtil.h"
#include "SerializeIni.h"
#include "SerializeIni3.h"
#include "SerializeTxt3.h"
#include "../sertxt_test/SettingsSumatra.h"

#define Check(x) CrashIf(!(x)); if (!(x)) return false; else NoOp()

using namespace serini3;

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

static bool TestSerializeIni3()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    GlobalPrefs *s = (GlobalPrefs *)Deserialize(data, str::Len(data), gGlobalPrefsInfo);
    Check(s); // failed to parse file
    Check(str::Find(s->inverseSearchCmdLine, L"\r\n"));

    size_t len;
    ScopedMem<char> ser(Serialize(s, gGlobalPrefsInfo, &len));
    Check(str::Len(ser) == len);
    Check(str::Eq(data, ser));
    FreeStruct(s, gGlobalPrefsInfo);

    return true;
}

static bool TestSerializeUserIni3()
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

struct Rec {
    size_t recCount;
    Rec * rec;
    UserPrefs up;
};

static SettingInfo gRecInfo[] = {
    { NULL, Type_Meta, sizeof(Rec), NULL, 3 },
    { "Rec", Type_Array, offsetof(Rec, rec), gRecInfo, NULL },
    { NULL, Type_Meta, offsetof(Rec, recCount), gRecInfo, NULL },
    { "Up", Type_Struct, offsetof(Rec, up), gUserPrefsInfo, NULL },
};

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
    Check(2 == r->recCount && 2 == r->rec[0].recCount && 2 == r->rec[0].rec[0].recCount);
    Check(0 == r->rec[0].rec[0].rec[0].recCount && 0 == r->rec[0].rec[0].rec[1].recCount);
    Check(1 == r->rec[0].rec[1].recCount && 0 == r->rec[0].rec[1].rec[0].recCount);
    Check(1 == r->rec[1].recCount && 0 == r->rec[1].rec[0].recCount);
    Check(1 == r->rec[1].up.externalViewerCount && str::Eq(r->rec[1].up.externalViewer[0].commandLine, L"serini_test.exe"));
    Check(str::Eq(r->rec[0].rec[1].up.printerDefaults.printScale, "shrink"));
    FreeStruct(r, gRecInfo);

    // TODO: recurse even if array parents are missing?
    // (bounded by maximum section name length)
    data = "[Rec.Rec]";
    r = (Rec *)Deserialize(data, str::Len(data), gRecInfo);
    Check(0 == r->recCount);
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

struct TestCD2 { bool val1; float val2; };
struct TestCD1 { TestCD2 compact; };

static SettingInfo gTestCD2Info[] = {
    { NULL, Type_Meta, sizeof(TestCD2), NULL, 2 },
    { "Val1", Type_Bool, offsetof(TestCD2, val1), NULL, false },
    { "Val2", Type_Float, offsetof(TestCD2, val2), NULL, (int64_t)"3.14" },
};

static SettingInfo gTestCD1Info[] = {
    { NULL, Type_Meta, sizeof(TestCD1), NULL, 1 },
    { "Compact", Type_Compact, offsetof(TestCD1, compact), gTestCD2Info, NULL },
};

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

static bool TestSerializeTxt3()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3.sqt";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    GlobalPrefs *s = (GlobalPrefs *)sertxt3::Deserialize(data, str::Len(data), gGlobalPrefsInfo);
    Check(s); // failed to parse file
    Check(str::Find(s->inverseSearchCmdLine, L"\r\n"));

    size_t len;
    ScopedMem<char> ser(sertxt3::Serialize(s, gGlobalPrefsInfo, &len));
    Check(str::Len(ser) == len);
    Check(str::Eq(data, ser));
    sertxt3::FreeStruct(s, gGlobalPrefsInfo);

    return true;
}

static bool TestSerializeUserTxt3()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3-user.sqt";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    UserPrefs *s = (UserPrefs *)sertxt3::Deserialize(data, str::Len(data), gUserPrefsInfo);
    Check(s); // failed to parse file

    ScopedMem<char> ser(sertxt3::Serialize(s, gUserPrefsInfo, NULL, "cf. https://sumatrapdf.googlecode.com/svn/trunk/docs/SumatraPDF-user.sqt"));
    sertxt3::FreeStruct(s, gUserPrefsInfo);
    Check(str::Eq(data, ser));

    return true;
}

static bool TestSerializeRecursiveArrayTxt3()
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
  Up [\n\
    ExternalViewer = [\n\
      CommandLine = serini_test.exe\n\
    ]\n\
  ]\n\
]";
    Rec *r = (Rec *)sertxt3::Deserialize(data, str::Len(data), gRecInfo);
    Check(2 == r->recCount && 2 == r->rec[0].recCount && 2 == r->rec[0].rec[0].recCount);
    Check(0 == r->rec[0].rec[0].rec[0].recCount && 0 == r->rec[0].rec[0].rec[1].recCount);
    Check(1 == r->rec[0].rec[1].recCount && 0 == r->rec[0].rec[1].rec[0].recCount);
    Check(1 == r->rec[1].recCount && 0 == r->rec[1].rec[0].recCount);
    Check(1 == r->rec[1].up.externalViewerCount && str::Eq(r->rec[1].up.externalViewer[0].commandLine, L"serini_test.exe"));
    Check(str::Eq(r->rec[0].rec[1].up.printerDefaults.printScale, "shrink"));
    sertxt3::FreeStruct(r, gRecInfo);

    return true;
}

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
    if (!TestSerializeIni3())
        errors++;
    if (!TestSerializeUserIni3())
        errors++;
    if (!TestSerializeRecursiveArray())
        errors++;
    if (!TestDefaultValues())
        errors++;
    if (!TestCompactDefaultValues())
        errors++;
    if (!TestSerializeTxt3())
        errors++;
    if (!TestSerializeUserTxt3())
        errors++;
    if (!TestSerializeRecursiveArrayTxt3())
        errors++;
    return errors;
}
