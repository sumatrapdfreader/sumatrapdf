#include "BaseUtil.h"
#include "FileUtil.h"
#include "SerializeTxt.h"
#include "SerializeTxtParser.h"
#include "SettingsSumatra.h"

static void TestParseSumatraSettings()
{
    size_t fileSize;
    const char *path = "..\\tools\\sertxt_test\\data.txt";
    char *s = file::ReadAllUtf(path, &fileSize);
    if (!s) {
        printf("failed to load '%s'", path);
        return;
    }

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
    const char *res = PrettyPrintTxt(parser);
    ok = str::Eq(res, expected);
    if (!ok) {
        printf("'%s'\npretty printed as\n'%s'\nand we expected\n'%s'\n", s, res, expected);
    }
    free((void*)res);
    free(sCopy);
    return ok;
}

static void TestFromFile()
{
    size_t fileSize;
    const char *path = "..\\tools\\sertxt_test\\tests.txt";
    char *s = file::ReadAllUtf(path, &fileSize);
    if (!s) {
        printf("failed to load '%s'", path);
        return;
    }
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
}

static void TestSettingsDeserialize()
{
    size_t fileSize;
    const char *path = "..\\tools\\sertxt_test\\data.txt";
    char *s = file::ReadAllUtf(path, &fileSize);
    if (!s) {
        printf("failed to load '%s'", path);
        return;
    }
    Settings *settings = DeserializeSettings(s, (int)fileSize);
    if (!settings) {
        printf("failed to deserialize\n'%s'\n", s);
        return;
    }
    int serializedLen;
    char *s2 = (char*)SerializeSettings(settings, &serializedLen);
    s2 += 3; // skip utf8 bom
    char *toFree = s;
    if (str::EqN(s, UTF8_BOM, 3))
        s += 3;
    str::NormalizeNewlinesInPlace(s2);
    str::NormalizeNewlinesInPlace(s);
    if (!str::Eq(s, s2)) {
        printf("'%s'\n != \n'%s'\n", s, s2);
    }
    free(toFree);
    FreeSettings(settings);
}

int main(int argc, char **argv)
{
#ifdef DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    TestFromFile();
    TestSettingsDeserialize();
    TestParseSumatraSettings();
}
