#include "BaseUtil.h"
#include "FileUtil.h"
#include "SerializeTxt.h"
#include "SerializeTxtParser.h"
#include "SettingsSumatra.h"
#include "SettingsSimple.h"

using namespace sertxt;

static char *CheckedLoad(const char *path, size_t *fileSizeOut)
{
    char *s = file::ReadAllUtf(path, fileSizeOut);
    if (!s)
        printf("failed to load '%s'", path);
    return s;
}

static void CheckStrEq(const char *s1, const char *s2)
{
    if (str::Eq(s1, s2))
        return;
    printf("'%s'\n != \n'%s'\n", s1, s2);
}

static void CheckStrEq(const WCHAR *s1, const WCHAR *s2)
{
    if (str::Eq(s1, s2))
        return;
    wprintf(L"'%s'\n != \n'%s'\n", s1, s2);
}

static void TestParseSumatraSettings()
{
    size_t fileSize;
    char *s = CheckedLoad( "..\\tools\\sertxt_test\\data.txt", &fileSize);
    if (!s)
        return;
    TxtParser parser;
    parser.SetToParse(s, fileSize);
    bool ok = ParseTxt(parser);
    if (!ok)
        printf("failed to parse \n'%s'\n", s);
    free(s);
}

static bool TestString(const char *s, char *expected)
{
    size_t n = str::NormalizeNewlinesInPlace(expected);
    expected[n] = '\n';
    expected[n+1] = 0;

    char *sCopy = str::Dup(s);
    TxtParser parser;
    parser.SetToParse(sCopy,  str::Len(sCopy));
    bool ok = ParseTxt(parser);
    if (!ok) {
        printf("Failed to parse:'\n%s'\n", s);
        return false;
    }
    char *res = PrettyPrintTxt(parser);
    ok = str::Eq(res, expected);
    if (!ok)
        printf("'%s'\npretty printed as\n'%s'\nand we expected\n'%s'\n", s, res, expected);
    free(res);
    free(sCopy);
    return ok;
}

static void TestFromFile()
{
    size_t fileSize;
    char *fileContent = CheckedLoad( "..\\tools\\sertxt_test\\tests.txt", &fileSize);
    char *s = fileContent;
    if (!s)
        return;

    Vec<char*> tests;
    for (;;) {
        tests.Append(s);
        s = (char*)str::Find(s, "---");
        if (NULL == s)
            break;
        *s = 0;
        s = s + 3; // skip "---"
        // also skip --->
        if (*s == '>')
            ++s;
        for (char c = *s; (c = *s) != '\0'; s++) {
            if (!(c == '\r' || c == '\n'))
                break;
        }
    }
    int n = tests.Count();
    if (n % 2 != 0) {
        printf("Number of tests cases must be even and is %d\n", (int)tests.Count());
        return;
    }
    n = n / 2;
    for (int i=0; i < n; i++) {
        bool ok = TestString(tests.At(i*2), tests.At(i*2+1));
        if (!ok)
            return;
    }
    free(fileContent);
}

#define STR_ESCAPE_TEST_EXP "[lo\r $fo\to\\ l\na]]"

static void TestSettingsDeserialize()
{
    size_t fileSize;
    char *s = CheckedLoad("..\\tools\\sertxt_test\\data.txt", &fileSize);
    if (!s)
        return;

    Settings *settings = DeserializeSettings(s, fileSize);
    if (!settings) {
        printf("failed to deserialize\n'%s'\n", s);
        return;
    }
    size_t serializedLen;
    char *s2 = (char*)SerializeSettings(settings, &serializedLen);

    str::NormalizeNewlinesInPlace(s);
    str::NormalizeNewlinesInPlace(s2);
    CheckStrEq(s, s2);
    CheckStrEq(settings->str_escape_test, STR_ESCAPE_TEST_EXP);
    CheckStrEq(settings->wstr_1, L"wide string Πραγματικό &Μέγεθος\tCtrl+1");
    free(s);
    free(s2);
    FreeSettings(settings);
}

static void TestSettingsSimple()
{
    size_t fileSize;
    char *s =  CheckedLoad("..\\tools\\sertxt_test\\data_simple_with_ws.txt", &fileSize);
    if (!s)
        return;
    Simple *settings = DeserializeSimple(s, fileSize);
    if (!settings) {
        printf("failed to deserialize\n'%s'\n", s);
        return;
    }
    CheckStrEq(settings->str_1, "lola");
    CheckStrEq(settings->str_escape, STR_ESCAPE_TEST_EXP);
    CheckStrEq(settings->wstr_1, L"wide string Πραγματικό &Μέγεθος\nCtrl+1");
    size_t serializedLen;
    char *s2 = (char*)SerializeSimple(settings, &serializedLen);
    char *s3 = CheckedLoad("..\\tools\\sertxt_test\\data_simple_no_ws.txt", &fileSize);
    CheckStrEq(s2, s3);
    free(s);
    free(s2);
    free(s3);
    FreeSimple(settings);
}

static void TestDefault()
{
    size_t dataLen, defaultDataLen;
    char *data = CheckedLoad("..\\tools\\sertxt_test\\data_for_default.txt", &dataLen);
    char *defaultData = CheckedLoad("..\\tools\\sertxt_test\\data_simple_no_ws.txt", &defaultDataLen);
    if (!data || !defaultData)
        return;
    Simple *settings = DeserializeSimpleWithDefault(data, dataLen, defaultData, defaultDataLen);
    if (!settings) {
        printf("failed to deserialize\n'%s'\n", data);
        return;
    }

    size_t serializedLen;
    char *s = (char*)SerializeSimple(settings, &serializedLen);
    CheckStrEq(defaultData, s);
    free(s);
    free(data);
    free(defaultData);
    FreeSimple(settings);
}

int main(int argc, char **argv)
{
#ifdef DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    TestFromFile();
    TestSettingsSimple();
    TestSettingsDeserialize();
    TestParseSumatraSettings();
    TestDefault();
}
