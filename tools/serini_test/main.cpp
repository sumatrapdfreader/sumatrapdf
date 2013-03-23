#include "BaseUtil.h"
#include "FileUtil.h"
#include "../sertxt_test/SettingsSumatra.h"

int main(int argc, char **argv)
{
#ifdef DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    const WCHAR *path = L"..\\tools\\serini_test\\data.ini";

    ScopedMem<char> data(file::ReadAll(path, NULL));
    CrashIf(!data); // failed to read file
    if (!data)
        return 2;
    bool usedDefault;
    Settings *s = DeserializeSettings((const uint8_t *)data.Get(), str::Len(data), &usedDefault);
    CrashIf(!s); // failed to parse file
    if (!s) 
        return 1;
    CrashIf(usedDefault);
    CrashIf(!str::Find(s->advanced->ws, L"\r\n"));

    int len;
    ScopedMem<char> ser((char *)SerializeSettings(s, &len));
    CrashIf(str::Len(ser) != (size_t)len);
    CrashIf(!str::Eq(data, ser));
    FreeSettings(s);

    return 0;
}
