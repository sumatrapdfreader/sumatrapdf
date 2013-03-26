/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#define INCLUDE_APPPREFS3_METADATA
#include "AppPrefs3.h"
#include "FileUtil.h"
#include "SerializeIni3.h"
#include "../sertxt_test/SerializeTxt.h"
#include "../sertxt_test/SettingsSumatra.h"

#define Check(x) CrashIf(!(x)); if (!(x)) return false; else NoOp()

static bool TestSerializeIni()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    Settings *s = DeserializeSettings(data, str::Len(data));
    Check(s); // failed to parse file
    //Check(str::Find(s->advanced->ws, L"\r\n"));

    size_t len;
    ScopedMem<char> ser((char *)SerializeSettings(s, &len));
    Check(str::Len(ser) ==len);
    Check(str::Eq(data, ser));
    FreeSettings(s);

    return true;
}

static bool TestSerializeIni3()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    GlobalPrefs *s = (GlobalPrefs *)serini3::Deserialize(data, str::Len(data), gGlobalPrefsInfo);
    Check(s); // failed to parse file
    Check(str::Find(s->inverseSearchCmdLine, L"\r\n"));

    size_t len;
    ScopedMem<char> ser(serini3::Serialize(s, gGlobalPrefsInfo, &len));
    Check(str::Len(ser) == len);
    Check(str::Eq(data, ser));
    serini3::FreeStruct(s, gGlobalPrefsInfo);

    return true;
}

static bool TestSerializeUserIni3()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3-user.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    Check(data); // failed to read file
    UserPrefs *s = (UserPrefs *)serini3::Deserialize(data, str::Len(data), gUserPrefsInfo);
    Check(s); // failed to parse file

    ScopedMem<char> ser(serini3::Serialize(s, gUserPrefsInfo, NULL, "cf. https://sumatrapdf.googlecode.com/svn/trunk/docs/SumatraPDF-user.ini"));
    serini3::FreeStruct(s, gUserPrefsInfo);
    Check(str::Eq(data, ser));

    return true;
}

struct Rec {
    size_t recCount;
    Rec ** rec;
    UserPrefs *up;
};

static SettingInfo gRecInfo[] = {
    /* TODO: replace this hack with a second meta-struct? */
    { NULL, (SettingType)3, sizeof(Rec), NULL },
    { "Rec", Type_Array, offsetof(Rec, rec), gRecInfo },
    { NULL, Type_Array, offsetof(Rec, recCount), gRecInfo },
    { "Up", Type_Struct, offsetof(Rec, up), gUserPrefsInfo },
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
    Rec *r = (Rec *)serini3::Deserialize(data, str::Len(data), gRecInfo);
    Check(2 == r->recCount && 2 == r->rec[0]->recCount && 2 == r->rec[0]->rec[0]->recCount);
    Check(0 == r->rec[0]->rec[0]->rec[0]->recCount && 0 == r->rec[0]->rec[0]->rec[1]->recCount);
    Check(1 == r->rec[0]->rec[1]->recCount && 0 == r->rec[0]->rec[1]->rec[0]->recCount);
    Check(1 == r->rec[1]->recCount && 0 == r->rec[1]->rec[0]->recCount);
    Check(1 == r->rec[1]->up->externalViewerCount && str::Eq(r->rec[1]->up->externalViewer[0]->commandLine, L"serini_test.exe"));
    serini3::FreeStruct(r, gRecInfo);

    // TODO: recurse even if array parents are missing?
    // (bounded by maximum section name length)
    data = "[Rec.Rec]";
    r = (Rec *)serini3::Deserialize(data, str::Len(data), gRecInfo);
    Check(0 == r->recCount);
    serini3::FreeStruct(r, gRecInfo);

    return true;
}

static bool TestDefaultValues()
{
    UserPrefs *p = (UserPrefs *)serini3::Deserialize(NULL, 0, gUserPrefsInfo);
    Check(!p->advancedPrefs->escToExit && !p->advancedPrefs->traditionalEbookUI);
    Check(0xffffff == p->advancedPrefs->pageColor && 0x000000 == p->advancedPrefs->textColor);
    Check(0x6581ff == p->forwardSearch3->highlightColor && 15 == p->forwardSearch3->highlightWidth);
    Check(4 == p->pagePadding->innerX && 2 == p->pagePadding->outerY);
    Check(str::Eq(p->printerDefaults->printScale, "shrink"));
    serini3::FreeStruct(p, gUserPrefsInfo);

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
    if (!TestSerializeIni3())
        errors++;
    if (!TestSerializeUserIni3())
        errors++;
    if (!TestSerializeRecursiveArray())
        errors++;
    if (!TestDefaultValues())
        errors++;
    return errors;
}
