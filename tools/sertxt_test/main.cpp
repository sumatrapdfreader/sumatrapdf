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
    parser.toParse.Init(s, fileSize);
    bool ok = ParseTxt(parser);
    if (!ok)
        printf("%s", "failed to parse");
}

static bool TestString(const char *s, char *expected)
{
    TxtParser parser;

    size_t n = str::NormalizeNewlinesInPlace(expected);
    expected[n] = '\n';
    expected[n+1] = 0;

    char *sCopy = str::Dup(s);
    parser.toParse.Init(sCopy, str::Len(sCopy));
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
        for (char c = *s; c = *s; s++) {
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
    char *sCopy = str::Dup(s);
    bool usedDefault;
    Settings *settings = DeserializeSettings((const uint8_t *)s, (int)fileSize, &usedDefault);
    int serializedLen;
    char *s2 = (char*)SerializeSettings(settings, &serializedLen);
    str::NormalizeNewlinesInPlace(s2);
    str::NormalizeNewlinesInPlace(sCopy);
    if (!str::Eq(sCopy, s2)) {
        printf("'%s'\n != \n'%s'\n", sCopy, s2);
    }
    free(sCopy);
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
