#include "BaseUtil.h"
#include "FileUtil.h"
#include "SerializeTxtParser.h"
#include "SettingsSumatra.h"

static void testFile()
{
    size_t fileSize;
    TxtParser parser;

    char *p1 = path::JoinUtf("..", "tools", NULL);
    char *p2 = path::JoinUtf(p1, "sertxt_test", NULL);
    char *p3 = path::JoinUtf(p2, "data.txt", NULL);
    char *s = file::ReadAllUtf(p3, &fileSize);
    if (!s) {
        printf("failed to load '%s'", p3);
        goto Exit;
    }

    parser.toParse.Init(s, fileSize);
    bool ok = ParseTxt(parser);
    if (!ok)
        printf("%s", "failed to parse");
Exit:
    free(p3);
    free(p2);
    free(p1);
}

const char *gTests[] = {
    "les:\n[\n foo: bar\n [\n val \n] go\n]",
    "foo [\n  bar\n]",
    NULL
};

static void testString(const char *s)
{
    TxtParser parser;
    char *sCopy = str::Dup(s);
    parser.toParse.Init(sCopy, str::Len(sCopy));
    bool ok = ParseTxt(parser);
    CrashIf(!ok);
    free(sCopy);
}

int main(int argc, char **argv)
{
    for (int i=0; gTests[i]; i++) {
        testString(gTests[i]);
    }
    testFile();
}
