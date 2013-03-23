/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#define INCLUDE_APPPREFS3_METADATA
#include "AppPrefs3.h"
#include "FileUtil.h"
#include "SerializeIni3.h"
#include "../sertxt_test/SettingsSumatra.h"

static bool TestSerializeIni()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    CrashIf(!data); // failed to read file
    if (!data)
        return false;
    bool usedDefault;
    Settings *s = DeserializeSettings((const uint8_t *)data.Get(), str::Len(data), &usedDefault);
    CrashIf(!s); // failed to parse file
    if (!s) 
        return false;
    CrashIf(usedDefault);
    CrashIf(!str::Find(s->advanced->ws, L"\r\n"));

    int len;
    ScopedMem<char> ser((char *)SerializeSettings(s, &len));
    CrashIf(str::Len(ser) != (size_t)len);
    CrashIf(!str::Eq(data, ser));
    FreeSettings(s);

    return true;
}

static bool TestSerializeIni3()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    CrashIf(!data); // failed to read file
    if (!data)
        return false;
    GlobalPrefs *s = (GlobalPrefs *)serini3::Deserialize(data, str::Len(data), gGlobalPrefsInfo);
    CrashIf(!s); // failed to parse file
    if (!s) 
        return false;
    CrashIf(!str::Find(s->inverseSearchCmdLine, L"\r\n"));

    size_t len;
    ScopedMem<char> ser(serini3::Serialize(s, gGlobalPrefsInfo, &len));
    CrashIf(str::Len(ser) != len);
    CrashIf(!str::Eq(data, ser));
    serini3::FreeStruct(s, gGlobalPrefsInfo);

    return true;
}

static bool TestSerializeUserIni3()
{
    const WCHAR *path = L"..\\tools\\serini_test\\data3-user.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    CrashIf(!data); // failed to read file
    if (!data)
        return false;
    UserPrefs *s = (UserPrefs *)serini3::Deserialize(data, str::Len(data), gUserPrefsInfo);
    CrashIf(!s); // failed to parse file
    if (!s) 
        return false;

    ScopedMem<char> ser(serini3::Serialize(s, gUserPrefsInfo, NULL, "cf. https://sumatrapdf.googlecode.com/svn/trunk/docs/SumatraPDF-user.ini"));
    serini3::FreeStruct(s, gUserPrefsInfo);
    CrashIf(!str::Eq(data, ser));

    return true;
}

int main(int argc, char **argv)
{
#ifdef DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    if (!TestSerializeIni())
        return 1;
    if (!TestSerializeIni3())
        return 2;
    if (!TestSerializeUserIni3())
        return 3;
    return 0;
}
